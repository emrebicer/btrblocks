// ------------------------------------------------------------------------------
#include <filesystem>
#include <string>

#include "btrblocks.hpp"

#include "../btrfiles/Trim.hpp"
#include "common/Utils.hpp"
#include "compression/BtrReader.hpp"
#include "cxx.h"
#include "scheme/SchemePool.hpp"
#include "util.hpp"
// ------------------------------------------------------------------------------
namespace btrWrapper {
using namespace btrblocks;

bool reader_is_null(BtrReader& reader, u32 index, size_t row) {
  BitmapWrapper* nullmap = reader.getBitmap(index);
  bool is_null;
  if (nullmap->type() == BitmapType::ALLZEROS) {
    is_null = true;
  } else if (nullmap->type() == BitmapType::ALLONES) {
    is_null = false;
  } else {
    is_null = !(nullmap->get_bitset()->test(row));
  }

  return is_null;
}



}  // namespace btrWrapper
