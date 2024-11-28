// ------------------------------------------------------------------------------
#include "btrblocks_wrapper.hpp"
#include <filesystem>
#include "btrblocks.hpp"
#include "common/Log.hpp"
#include "common/Utils.hpp"
#include "compression/BtrReader.hpp"
#include "cxx.h"
#include "scheme/SchemePool.hpp"
// ------------------------------------------------------------------------------
namespace btrblocksWrapper {
using namespace btrblocks;

template <typename T>
bool validateData(size_t size, T* input, T* output) {
  for (auto i = 0u; i != size; ++i) {
    if (input[i] != output[i]) {
      std::cout << "value @" << i << " does not match; in " << input[i] << " vs out" << output[i]
                << std::endl;
      return false;
    }
  }
  return true;
}

bool compare_chunks(Relation* rel, Chunk* c1, Chunk* c2) {
  int size = rel->columns.at(0).size();
  bool check;
  for (auto col = 0u; col != rel->columns.size(); ++col) {
    auto& orig = c1->columns[col];
    auto& decomp = c2->columns[col];
    switch (rel->columns[col].type) {
      case btrblocks::ColumnType::INTEGER:
        check = validateData(size, reinterpret_cast<int32_t*>(orig.get()),
                             reinterpret_cast<int32_t*>(decomp.get()));
        break;
      case btrblocks::ColumnType::DOUBLE:
        check = validateData(size, reinterpret_cast<double*>(orig.get()),
                             reinterpret_cast<double*>(decomp.get()));
        break;
      default:
        UNREACHABLE();
    }
  }
  return check;
}

btrblocksWrapper::Buffer::Buffer(unique_ptr<uint8_t[]> buffer) {
  data = std::move(buffer);
}

btrblocksWrapper::Buffer::Buffer(size_t size) {
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[size]);
  data = std::move(buffer);
}

Buffer* new_buffer(size_t size) {
  return new Buffer(size);
}

btrblocksWrapper::IntMMapVector::IntMMapVector(Vector<int32_t>* vec) {
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

btrblocksWrapper::DoubleMMapVector::DoubleMMapVector(Vector<double>* vec) {
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

void configure_btrblocks(uint32_t max_depth) {
  BtrBlocksConfig::configure([&](BtrBlocksConfig& config) {
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

void relation_add_column_int(Relation* relation,
                             const rust::String& column_name,
                             IntMMapVector* btr_vec) {
  Column column(column_name.data(), std::move(*btr_vec->data));
  relation->addColumn(std::move(column));
}

void relation_add_column_double(Relation* relation,
                                const rust::String& column_name,
                                DoubleMMapVector* btr_vec) {
  Column column(column_name.data(), std::move(*btr_vec->data));
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
rust::Vec<uint32_t> get_file_metadata(rust::String metadata_path) {
  std::vector<char> raw_file_metadata;
  FileMetadata* file_metadata;

  Utils::readFileToMemory(metadata_path.c_str(), raw_file_metadata);
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

bool outputChunk(std::ofstream& csvstream,
                 u32 tuple_count,
                 const std::pair<u32, u32>& counter,
                 const std::vector<u8>& decompressed_column,
                 std::vector<BtrReader>& readers,
                 bool requires_copy) {
  for (size_t row = 0; row < tuple_count; row++) {
    BtrReader& reader = readers[counter.first];
    BitmapWrapper* nullmap = reader.getBitmap(counter.second - 1);

    bool is_null;
    if (nullmap->type() == BitmapType::ALLZEROS) {
      is_null = true;
    } else if (nullmap->type() == BitmapType::ALLONES) {
      is_null = false;
    } else {
      is_null = !(nullmap->get_bitset()->test(row));
    }

    if (!is_null) {
      switch (reader.getColumnType()) {
        case ColumnType::INTEGER: {
          auto int_array = reinterpret_cast<const INTEGER*>(decompressed_column.data());
          csvstream << int_array[row];
          break;
        }
        case ColumnType::DOUBLE: {
          auto double_array = reinterpret_cast<const DOUBLE*>(decompressed_column.data());
          csvstream << double_array[row];
          break;
        }
        case ColumnType::STRING: {
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
          csvstream << data;
          break;
        }
        default: {
          cout << "Error: Type" << ConvertTypeToString(reader.getColumnType())
               << " is not supported" << endl;
          return false;
        }
      }
    } else {
      csvstream << "null";
    }

    csvstream << "\n";
  }

  return true;
}

bool decompress_column_into_file(rust::String btr_dir_path,
                                 uint32_t column_index,
                                 rust::String output_path) {
  // For unknown reasons, this is necessary...
  SchemePool::refresh();

  // Get the metadata to read the part counts
  std::vector<char> raw_file_metadata;

  std::filesystem::path btr_dir = btr_dir_path.c_str();
  std::filesystem::path metadata_path = btr_dir / "metadata";

  Utils::readFileToMemory(metadata_path.string(), raw_file_metadata);
  FileMetadata* file_metadata = reinterpret_cast<FileMetadata*>(raw_file_metadata.data());

  // Open output file
  auto file = std::ofstream(output_path.data());
  file << std::setprecision(32);

  // Check if the column exists
  if (file_metadata->num_columns < column_index) {
    Log::error("column index ", column_index, " is non-existent");
    return false;
  }

  // Read the number of parts
  uint32_t num_parts = file_metadata->parts[column_index].num_parts;

  // Open output file
  auto output_stream = std::ofstream(output_path.c_str());
  output_stream << std::setprecision(32);

  // Prepare the readers
  std::vector<BtrReader> readers;
  std::vector<std::vector<char>> compressed_data(num_parts);

  // Read files to the memory
  for (u32 part_i = 0; part_i < num_parts; part_i++) {
    auto path =
        btr_dir / ("column" + std::to_string(column_index) + "_part" + std::to_string(part_i));
    Utils::readFileToMemory(path.string(), compressed_data[part_i]);
    readers.emplace_back(compressed_data[part_i].data());
  }

  // Counter contains a pair of <current_part_i, current_chunk_within_part_i>
  std::pair<u32, u32> counter = {0, 0};

  for (u32 chunk_i = 0; chunk_i < file_metadata->num_chunks; chunk_i++) {
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
    if (!outputChunk(output_stream, tuple_count, counter, output, readers, requires_copy)) {
      return false;
    }
  }

  return true;
}

}  // namespace btrblocksWrapper
