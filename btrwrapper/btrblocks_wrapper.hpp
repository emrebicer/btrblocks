#ifndef BTRBLOCKS_WRAPPER_HPP
#define BTRBLOCKS_WRAPPER_HPP

#include "btrblocks.hpp"
#include "cxx.h"

namespace btrWrapper {
using namespace btrblocks;

// Helper functions
bool compare_chunks(Relation* rel, Chunk* c1, Chunk* c2);

// Custom types
class Buffer {
 public:
  unique_ptr<uint8_t[]> data;
  Buffer(unique_ptr<uint8_t[]> data);
  Buffer(size_t size);
};
Buffer* new_buffer(size_t size);

class IntMMapVector {
 public:
  Vector<int32_t>* data;
  IntMMapVector(Vector<int32_t>* vec);
};
IntMMapVector* new_int_mmapvector(const rust::Vec<int32_t>& vec);

class DoubleMMapVector {
 public:
  Vector<double>* data;
  DoubleMMapVector(Vector<double>* vec);
};
DoubleMMapVector* new_double_mmapvector(const rust::Vec<double>& vec);

// Configuration
void configure_btrblocks(uint32_t max_depth, uint32_t block_size);

// Log
void set_log_level(int32_t value);

// Relation
Relation* new_relation();
void relation_add_column_int(Relation* relation, rust::String column_name, IntMMapVector* btr_vec);
void relation_add_column_double(Relation* relation,
                                rust::String column_name,
                                DoubleMMapVector* btr_vec);
uint64_t relation_get_tuple_count(Relation* relation);
Chunk* relation_get_chunk(Relation* relation, const rust::Vec<uint64_t>& ranges, size_t size);

// Chunk
uint64_t chunk_get_tuple_count(Chunk* chunk);
size_t chunk_size_bytes(Chunk* chunk);

// Datablock
Datablock* new_datablock(Relation* relation);
OutputBlockStats* datablock_compress(Datablock* datablock, Chunk* chunk, Buffer* buffer);
Chunk* datablock_decompress(Datablock* datablock, Buffer* buffer);

// OutputBlockStats
size_t stats_total_data_size(btrblocks::OutputBlockStats* stats);
double stats_compression_ratio(btrblocks::OutputBlockStats* stats);

// FileMetadata
rust::Vec<uint32_t> get_file_metadata(const rust::Vec<uint8_t>& metadata_bytes);

// Custom functions
rust::Vec<int32_t> decompress_column_i32(const rust::Vec<uint8_t>& column_part_bytes,
                                         const rust::Vec<size_t>& part_ending_indexes,
                                         uint32_t num_chunks);
rust::Vec<rust::String> decompress_column_string(const rust::Vec<uint8_t>& column_part_bytes,
                                                 const rust::Vec<size_t>& part_ending_indexes,
                                                 uint32_t num_chunks);
rust::Vec<double> decompress_column_f64(const rust::Vec<uint8_t>& column_part_bytes,
                                        const rust::Vec<size_t>& part_ending_indexes,
                                        uint32_t num_chunks);

rust::Vec<int32_t> decompress_column_part_i32(const rust::Vec<uint8_t>& part_bytes,
                                              const rust::Vec<uint8_t>& metadata_bytes,
                                              uint32_t column_index,
                                              uint32_t part_index);
rust::Vec<rust::String> decompress_column_part_string(const rust::Vec<uint8_t>& part_bytes,
                                                      const rust::Vec<uint8_t>& metadata_bytes,
                                                      uint32_t column_index,
                                                      uint32_t part_index);
rust::Vec<double> decompress_column_part_f64(const rust::Vec<uint8_t>& part_bytes,
                                             const rust::Vec<uint8_t>& metadata_bytes,
                                             uint32_t column_index,
                                             uint32_t part_index);

uint32_t compress_column_i32(rust::String btr_path,
                             const rust::Vec<int32_t>& data,
                             uint32_t column_index);
uint32_t compress_column_f64(rust::String btr_path,
                             const rust::Vec<double>& data,
                             uint32_t column_index);
uint32_t compress_column_string(rust::String btr_path,
                                const rust::Vec<rust::String>& data,
                                uint32_t column_index,
                                rust::String binary_path);
uint32_t get_num_chunks(uint64_t row_count);
rust::Vec<uint8_t> get_file_metadata_bytes(uint32_t num_columns,
                                           uint32_t num_chunks,
                                           rust::Vec<uint32_t> parts);

}  // namespace btrWrapper

#endif
