// ------------------------------------------------------------------------------
#include "btrblocks_wrapper.hpp"
#include "btrblocks.hpp"
#include "common/Log.hpp"
#include "common/Utils.hpp"
#include "cxx.h"
#include <filesystem>
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
rust::Vec<uint32_t> get_file_metadata(const rust::String& metadata_path) {
  std::vector<char> raw_file_metadata;
  FileMetadata* file_metadata;

  std::filesystem::path fs_path = metadata_path.data();
  Utils::readFileToMemory(fs_path, raw_file_metadata);
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

}  // namespace btrblocksWrapper
