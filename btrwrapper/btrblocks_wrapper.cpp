// ------------------------------------------------------------------------------
#include <atomic>
#include <filesystem>
#include <string>

#include "btrblocks.hpp"
#include "btrblocks_wrapper.hpp"
#include "util.hpp"

#include "common/Log.hpp"
#include "common/Utils.hpp"
#include "compression/BtrReader.hpp"
#include "scheme/SchemePool.hpp"

#include "cxx.h"
#include "tbb/parallel_for.h"
#include "tbb/task_scheduler_init.h"
// ------------------------------------------------------------------------------
namespace btrWrapper {
using namespace btrblocks;

template <typename T>
void chunk_to_vec(rust::Vec<T>& vec,
                  u32 tuple_count,
                  const std::pair<u32, u32>& counter,
                  const std::vector<u8>& decompressed_column,
                  std::vector<BtrReader>& readers,
                  bool requires_copy) {
  BtrReader& reader = readers[counter.first];

  switch (reader.getColumnType()) {
    case ColumnType::INTEGER: {
      if constexpr (std::is_same<T, int32_t>::value) {
        auto int_array = reinterpret_cast<const INTEGER*>(decompressed_column.data());
        for (size_t row = 0; row < tuple_count; row++) {
          bool is_null = btrWrapper::reader_is_null(reader, counter.second - 1, row);
          if (!is_null) {
            vec.push_back(int_array[row]);
          } else {
            vec.push_back(0);
          }
        }
        break;
      } else {
        throw Generic_Exception("Requested column type T does not match 'integer'");
      }
    }
    case ColumnType::DOUBLE: {
      if constexpr (std::is_same<T, double>::value) {
        auto double_array = reinterpret_cast<const DOUBLE*>(decompressed_column.data());
        for (size_t row = 0; row < tuple_count; row++) {
          bool is_null = btrWrapper::reader_is_null(reader, counter.second - 1, row);
          if (!is_null) {
            vec.push_back(double_array[row]);
          } else {
            vec.push_back(0);
          }
        }
        break;
      } else {
        throw Generic_Exception("Requested column type T does not match 'double'");
      }
    }
    case ColumnType::STRING: {
      if constexpr (std::is_same<T, rust::String>::value) {
        for (size_t row = 0; row < tuple_count; row++) {
          bool is_null = btrWrapper::reader_is_null(reader, counter.second - 1, row);

          if (!is_null) {
            std::string data;
            if (requires_copy) {
              auto string_pointer_array_viewer =
                  StringPointerArrayViewer(reinterpret_cast<const u8*>(decompressed_column.data()));
              data = string_pointer_array_viewer(row);
            } else {
              auto string_array_viewer =
                  StringArrayViewer(reinterpret_cast<const u8*>(decompressed_column.data()));
              data = string_array_viewer(row);
            }
            vec.push_back(rust::String(data));
          } else {
            vec.push_back("null");
          }
        }
        break;
      } else {
        throw Generic_Exception("Requested column type T does not match 'string'");
      }
    }
    default: {
      throw Generic_Exception("Type " + ConvertTypeToString(reader.getColumnType()) +
                              " not supported");
    }
  }
}

template <typename T>
void part_chunk_to_vec(rust::Vec<T>& vec,
                       u32 tuple_count,
                       u32 chunk_counter,
                       const std::vector<u8>& decompressed_column,
                       BtrReader& reader,
                       bool requires_copy) {
  switch (reader.getColumnType()) {
    case ColumnType::INTEGER: {
      if constexpr (std::is_same<T, int32_t>::value) {
        auto int_array = reinterpret_cast<const INTEGER*>(decompressed_column.data());
        for (size_t row = 0; row < tuple_count; row++) {
          bool is_null = btrWrapper::reader_is_null(reader, chunk_counter - 1, row);
          if (!is_null) {
            vec.push_back(int_array[row]);
          } else {
            vec.push_back(0);
          }
        }
        break;
      } else {
        throw Generic_Exception("Requested column type T does not match 'integer'");
      }
    }
    case ColumnType::DOUBLE: {
      if constexpr (std::is_same<T, double>::value) {
        auto double_array = reinterpret_cast<const DOUBLE*>(decompressed_column.data());
        for (size_t row = 0; row < tuple_count; row++) {
          bool is_null = btrWrapper::reader_is_null(reader, chunk_counter - 1, row);
          if (!is_null) {
            vec.push_back(double_array[row]);
          } else {
            vec.push_back(0);
          }
        }
        break;
      } else {
        throw Generic_Exception("Requested column type T does not match 'double'");
      }
    }
    case ColumnType::STRING: {
      if constexpr (std::is_same<T, rust::String>::value) {
        for (size_t row = 0; row < tuple_count; row++) {
          bool is_null = btrWrapper::reader_is_null(reader, chunk_counter - 1, row);

          if (!is_null) {
            std::string data;
            if (requires_copy) {
              auto string_pointer_array_viewer =
                  StringPointerArrayViewer(reinterpret_cast<const u8*>(decompressed_column.data()));
              data = string_pointer_array_viewer(row);
            } else {
              auto string_array_viewer =
                  StringArrayViewer(reinterpret_cast<const u8*>(decompressed_column.data()));
              data = string_array_viewer(row);
            }
            vec.push_back(rust::String(data));
          } else {
            vec.push_back("null");
          }
        }
        break;
      } else {
        throw Generic_Exception("Requested column type T does not match 'string'");
      }
    }
    default: {
      throw Generic_Exception("Type " + ConvertTypeToString(reader.getColumnType()) +
                              " not supported");
    }
  }
}

template <typename T>
bool validate_data(size_t size, T* input, T* output) {
  for (auto i = 0u; i != size; ++i) {
    if (input[i] != output[i]) {
      std::cout << "value @" << i << " does not match; in " << input[i] << " vs out" << output[i]
                << std::endl;
      return false;
    }
  }
  return true;
}

template <typename T>
rust::Vec<T> decompress_column(const rust::Vec<uint8_t>& column_part_bytes,
                               const rust::Vec<size_t>& part_ending_indexes,
                               uint32_t num_chunks) {
  // For unknown reasons, this is necessary...
  SchemePool::refresh();

  // Prepare the readers
  std::vector<BtrReader> readers;
  std::vector<std::vector<char>> compressed_data(part_ending_indexes.size());

  size_t start = 0;
  for (u32 part_i = 0; part_i < part_ending_indexes.size(); part_i++) {
    size_t end = part_ending_indexes[part_i];
    compressed_data[part_i] =
        std::vector<char>(column_part_bytes.begin() + start, column_part_bytes.begin() + end);
    readers.emplace_back(compressed_data[part_i].data());
    start = end;
  }

  // Counter contains a pair of <current_part_i, current_chunk_within_part_i>
  std::pair<u32, u32> counter = {0, 0};

  rust::Vec<T> vec;
  for (u32 chunk_i = 0; chunk_i < num_chunks; chunk_i++) {
    std::vector<u8> output;

    bool requires_copy = false;
    u32 tuple_count = 0;

    u32 part_i = counter.first;
    BtrReader& reader = readers[part_i];
    if (counter.second >= reader.getChunkCount()) {
      counter.first++;
      part_i++;
      counter.second = 0;
      reader = readers[part_i];
    }

    u32 part_chunk_i = counter.second;
    tuple_count = reader.getTupleCount(part_chunk_i);
    requires_copy = reader.readColumn(output, part_chunk_i);
    counter.second++;
    chunk_to_vec(vec, tuple_count, counter, output, readers, requires_copy);
  }

  return vec;
}

template <typename T>
rust::Vec<T> decompress_column_part(const rust::Vec<uint8_t>& part_bytes,
                                    const rust::Vec<uint8_t>& metadata_bytes,
                                    uint32_t column_index,
                                    uint32_t part_index) {
  // For unknown reasons, this is necessary...
  SchemePool::refresh();

  // Get the metadata to read the part counts
  std::vector<char> raw_file_metadata(metadata_bytes.begin(), metadata_bytes.end());

  FileMetadata* file_metadata = reinterpret_cast<FileMetadata*>(raw_file_metadata.data());

  // Check if the column exists
  if (file_metadata->num_columns < column_index) {
    throw Generic_Exception("column index:" + std::to_string(column_index) + " does not exist");
  }

  // Read the number of parts
  uint32_t num_parts = file_metadata->parts[column_index].num_parts;

  // Check if the part exists
  if (num_parts < part_index) {
    throw Generic_Exception("part index:" + std::to_string(part_index) + " does not exist");
  }

  // Prepare the reader
  std::vector<char> compressed_data(part_bytes.begin(), part_bytes.end());
  BtrReader reader(compressed_data.data());

  // Current chunk index in the current part_index
  u32 chunk_counter = 0;

  rust::Vec<T> vec;
  for (u32 chunk_i = 0; chunk_i < file_metadata->num_chunks; chunk_i++) {
    std::vector<u8> output;

    // If we are done with the chunks in the current part, we have read what we want, so return...
    if (chunk_counter >= reader.getChunkCount()) {
      break;
    }

    u32 tuple_count = reader.getTupleCount(chunk_counter);
    bool requires_copy = reader.readColumn(output, chunk_counter);
    chunk_counter++;
    part_chunk_to_vec(vec, tuple_count, chunk_counter, output, reader, requires_copy);
  }

  return vec;
}

bool compare_chunks(Relation* rel, Chunk* c1, Chunk* c2) {
  int size = rel->columns.at(0).size();
  bool check;
  for (auto col = 0u; col != rel->columns.size(); ++col) {
    auto& orig = c1->columns[col];
    auto& decomp = c2->columns[col];
    switch (rel->columns[col].type) {
      case btrblocks::ColumnType::INTEGER:
        check = btrWrapper::validate_data(size, reinterpret_cast<int32_t*>(orig.get()),
                                          reinterpret_cast<int32_t*>(decomp.get()));
        break;
      case btrblocks::ColumnType::DOUBLE:
        check = btrWrapper::validate_data(size, reinterpret_cast<double*>(orig.get()),
                                          reinterpret_cast<double*>(decomp.get()));
        break;
      default:
        UNREACHABLE();
    }
  }
  return check;
}

btrWrapper::Buffer::Buffer(unique_ptr<uint8_t[]> buffer) {
  data = std::move(buffer);
}

btrWrapper::Buffer::Buffer(size_t size) {
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[size]);
  data = std::move(buffer);
}

btrWrapper::Buffer* new_buffer(size_t size) {
  return new Buffer(size);
}

btrWrapper::IntMMapVector::IntMMapVector(Vector<int32_t>* vec) {
  data = vec;
}

IntMMapVector* new_int_mmapvector(const rust::Vec<int32_t>& vec) {
  size_t size = vec.size();
  auto* data = new Vector<int32_t>(size);

  for (size_t i = 0; i < size; ++i) {
    (*data)[i] = vec[i];
  }

  return new IntMMapVector(data);
}

btrWrapper::DoubleMMapVector::DoubleMMapVector(Vector<double>* vec) {
  data = vec;
}

DoubleMMapVector* new_double_mmapvector(const rust::Vec<double>& vec) {
  size_t size = vec.size();
  auto* data = new Vector<double>(size);

  for (size_t i = 0; i < size; ++i) {
    (*data)[i] = vec[i];
  }

  return new DoubleMMapVector(data);
}

void configure_btrblocks(uint32_t max_depth, uint32_t block_size) {
  BtrBlocksConfig::configure([&](BtrBlocksConfig& config) {
    config.block_size = block_size;
    config.integers.max_cascade_depth = max_depth;
    config.doubles.max_cascade_depth = max_depth;
    config.strings.max_cascade_depth = max_depth;
    config.doubles.schemes.enable(DoubleSchemeType::DOUBLE_BP);
  });
}

void set_log_level(int32_t value) {
  Log::level level = static_cast<Log::level>(value);
  Log::set_level(level);
}

Relation* new_relation() {
  return new Relation();
}

void relation_add_column_int(Relation* relation, rust::String column_name, IntMMapVector* btr_vec) {
  Column column(column_name.c_str(), std::move(*btr_vec->data));
  relation->addColumn(std::move(column));
}

void relation_add_column_double(Relation* relation,
                                rust::String column_name,
                                DoubleMMapVector* btr_vec) {
  Column column(column_name.c_str(), std::move(*btr_vec->data));
  relation->addColumn(std::move(column));
}

uint64_t relation_get_tuple_count(Relation* relation) {
  return relation->tuple_count;
}

uint64_t chunk_get_tuple_count(Chunk* chunk) {
  return chunk->tuple_count;
}

size_t chunk_size_bytes(Chunk* chunk) {
  return chunk->size_bytes();
}

Chunk* relation_get_chunk(Relation* relation, const rust::Vec<uint64_t>& ranges, size_t size) {
  std::vector<Range> std_vec_ranges;
  for (size_t i = 0; i < ranges.size(); i += 2) {
    std_vec_ranges.push_back({Range(ranges.at(i), ranges.at(i + 1))});
  }
  return new Chunk(relation->getChunk(std_vec_ranges, size));
}

Datablock* new_datablock(Relation* relation) {
  return new Datablock(*relation);
}

OutputBlockStats* datablock_compress(Datablock* datablock, Chunk* chunk, Buffer* buffer) {
  auto stats = datablock->compress(*chunk, buffer->data);
  return new OutputBlockStats(stats);
}

Chunk* datablock_decompress(Datablock* datablock, Buffer* buffer) {
  return new Chunk(datablock->decompress(buffer->data));
}

size_t stats_total_data_size(btrblocks::OutputBlockStats* stats) {
  return stats->total_data_size;
}
double stats_compression_ratio(btrblocks::OutputBlockStats* stats) {
  return stats->compression_ratio;
}

// FileMetadata
rust::Vec<uint32_t> get_file_metadata(const rust::Vec<uint8_t>& metadata_bytes) {
  std::vector<char> raw_file_metadata(metadata_bytes.begin(), metadata_bytes.end());
  FileMetadata* file_metadata;

  file_metadata = reinterpret_cast<FileMetadata*>(raw_file_metadata.data());

  rust::Vec<u32> v;
  v.push_back(file_metadata->num_columns);
  v.push_back(file_metadata->num_chunks);

  for (size_t i = 0; i < file_metadata->num_columns; i++) {
    const ColumnPartInfo& part = file_metadata->parts[i];
    v.push_back(static_cast<uint32_t>(part.type));
    v.push_back(part.num_parts);
  }

  return v;
}

rust::Vec<int32_t> decompress_column_i32(const rust::Vec<uint8_t>& column_part_bytes,
                                         const rust::Vec<size_t>& part_ending_indexes,
                                         uint32_t num_chunks) {
  return btrWrapper::decompress_column<int32_t>(column_part_bytes, part_ending_indexes, num_chunks);
}

rust::Vec<int32_t> decompress_column_part_i32(const rust::Vec<uint8_t>& part_bytes,
                                              const rust::Vec<uint8_t>& metadata_bytes,
                                              uint32_t column_index,
                                              uint32_t part_index) {
  return btrWrapper::decompress_column_part<int32_t>(part_bytes, metadata_bytes, column_index,
                                                     part_index);
}

rust::Vec<rust::String> decompress_column_string(const rust::Vec<uint8_t>& column_part_bytes,
                                                 const rust::Vec<size_t>& part_ending_indexes,
                                                 uint32_t num_chunks) {
  return btrWrapper::decompress_column<rust::String>(column_part_bytes, part_ending_indexes,
                                                     num_chunks);
}

rust::Vec<rust::String> decompress_column_part_string(const rust::Vec<uint8_t>& part_bytes,
                                                      const rust::Vec<uint8_t>& metadata_bytes,
                                                      uint32_t column_index,
                                                      uint32_t part_index) {
  return btrWrapper::decompress_column_part<rust::String>(part_bytes, metadata_bytes, column_index,
                                                          part_index);
}

rust::Vec<double> decompress_column_f64(const rust::Vec<uint8_t>& column_part_bytes,
                                        const rust::Vec<size_t>& part_ending_indexes,
                                        uint32_t num_chunks) {
  return btrWrapper::decompress_column<double>(column_part_bytes, part_ending_indexes, num_chunks);
}

rust::Vec<double> decompress_column_part_f64(const rust::Vec<uint8_t>& part_bytes,
                                             const rust::Vec<uint8_t>& metadata_bytes,
                                             uint32_t column_index,
                                             uint32_t part_index) {
  return btrWrapper::decompress_column_part<double>(part_bytes, metadata_bytes, column_index,
                                                    part_index);
}

void csv_to_btr(rust::String csv_path,
                rust::String btr_path,
                rust::String binary_path,
                rust::Vec<rust::String> columns_metadata_raw) {
  // This seems necessary to be
  SchemePool::refresh();

  std::filesystem::path csv_path_fs = csv_path.c_str();
  std::filesystem::path btr_path_fs = btr_path.c_str();
  std::filesystem::path binary_path_fs = binary_path.c_str();

  /*cout << "csv_fs:              " << csv_path_fs.string() <<  endl;*/
  /*cout << "btr_path_fs:         " << btr_path_fs.string() << endl;*/
  /*cout << "binary_path_fs:      " << binary_path_fs.string() << endl;*/

  // Init TBB TODO: is that actually still necessary ?
  tbb::task_scheduler_init init(8);

  vector<ColumnMetadata> columns;
  for (size_t i = 0; i < columns_metadata_raw.size(); i += 2) {
    columns.push_back(
        ColumnMetadata{columns_metadata_raw.at(i).data(), columns_metadata_raw.at(i + 1).data()});
  }

  // Load and parse CSV
  std::ifstream csv(csv_path_fs);
  if (!csv.good()) {
    throw Generic_Exception("Unable to open specified csv file");
  }

  // parse writes the binary files
  btrWrapper::convert_csv(csv_path_fs.string(), columns, binary_path_fs.string(), ",");

  // Create relation
  Relation relation = btrWrapper::read_directory(columns, binary_path_fs.string());
  /*relation.name = schema_yaml_path_fs.stem();*/

  // Prepare datastructures for btr compression
  auto ranges = relation.getRanges(SplitStrategy::SEQUENTIAL, 9999);
  assert(ranges.size() > 0);
  Datablock datablockV2(relation);
  std::filesystem::create_directory(btr_path.c_str());

  // These counter are for statistics that match the harbook.
  std::vector<std::atomic_size_t> sizes_uncompressed(relation.columns.size());
  std::vector<std::atomic_size_t> sizes_compressed(relation.columns.size());
  std::vector<u32> part_counters(relation.columns.size());
  std::vector<ColumnType> types(relation.columns.size());

  tbb::parallel_for(SIZE(0), relation.columns.size(), [&](SIZE column_i) {
    types[column_i] = relation.columns[column_i].type;

    std::vector<InputChunk> input_chunks;
    std::string path_prefix =
        btr_path_fs.string() + "/" + "column" + std::to_string(column_i) + "_part";
    ColumnPart part;
    for (SIZE chunk_i = 0; chunk_i < ranges.size(); chunk_i++) {
      auto input_chunk = relation.getInputChunk(ranges[chunk_i], chunk_i, column_i);
      std::vector<u8> data = Datablock::compress(input_chunk);
      sizes_uncompressed[column_i] += input_chunk.size;

      if (!part.canAdd(data.size())) {
        std::string filename = path_prefix + std::to_string(part_counters[column_i]);
        sizes_compressed[column_i] += part.writeToDisk(filename);
        part_counters[column_i]++;
        input_chunks.clear();
      }

      input_chunks.push_back(std::move(input_chunk));
      part.addCompressedChunk(std::move(data));
    }

    if (!part.chunks.empty()) {
      std::string filename = path_prefix + std::to_string(part_counters[column_i]);
      sizes_compressed[column_i] += part.writeToDisk(filename);
      part_counters[column_i]++;
      input_chunks.clear();
    }
  });

  Datablock::writeMetadata(btr_path_fs.string() + "/metadata", types, part_counters, ranges.size());
}

}  // namespace btrWrapper
