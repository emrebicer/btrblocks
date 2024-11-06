#ifndef BTRBLOCKS_WRAPPER_HPP
#define BTRBLOCKS_WRAPPER_HPP

#include "btrblocks.hpp"

namespace btrblocksWrapper {
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

  // Configuration
  void configure_btrblocks(uint32_t max_depth);

  // Log
  void set_log_level(int32_t value);

  // Relation
  Relation* new_relation();
  void relation_add_column_int_random(Relation* relation,
                                      uint32_t size,
                                      uint32_t unique,
                                      uint32_t runlength,
                                      int32_t seed);
  void relation_add_column_double_random(Relation* relation,
                                         uint32_t size,
                                         uint32_t unique,
                                         uint32_t runlength,
                                         int32_t seed);
  uint64_t relation_get_tuple_count(Relation* relation);
  Chunk* relation_get_chunk(Relation* relation, uint64_t range_start, uint64_t range_end, size_t size);

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
  
}

#endif
