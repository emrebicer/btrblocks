#include "btrblocks.hpp"
#include "compression/BtrReader.hpp"
#include "cxx.h"

namespace btrWrapper {
using namespace btrblocks;

struct ColumnDescriptor {
  string name;
  ColumnType column_type;
  u32 vector_offset;
  vector<BITMAP> set_bitmap;
  u32 null_count = 0;   // when 'null' comes in the input
  u32 empty_count = 0;  // 0 by double and integers, '' by strings
};

struct ColumnMetadata {
  std::string name;
  std::string type;
};

bool reader_is_null(BtrReader& reader, u32 index, size_t row);

}  // namespace btrWrapper
