#ifndef BTRBLOCKS_WRAPPER_HPP
#define BTRBLOCKS_WRAPPER_HPP

#include "btrblocks.hpp"
#include "cxx.h"

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
void configure_btrblocks(uint32_t max_depth);

// Log
void set_log_level(int32_t value);

// Relation
Relation* new_relation();
void relation_add_column_int(Relation* relation,
                             const rust::String& column_name,
                             IntMMapVector* btr_vec);
void relation_add_column_double(Relation* relation,
                                const rust::String& column_name,
                                DoubleMMapVector* btr_vec);
uint64_t relation_get_tuple_count(Relation* relation);
Chunk* relation_get_chunk(Relation* relation,
                          const rust::Vec<uint64_t>& ranges,
                          size_t size);

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

}  // namespace btrblocksWrapper

#endif
