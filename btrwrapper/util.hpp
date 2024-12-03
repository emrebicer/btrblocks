#include "btrblocks.hpp"
#include "cxx.h"
#include "compression/BtrReader.hpp"

namespace btrWrapper {
using namespace btrblocks;

struct ColumnMetadata {
  std::string name;
  std::string type;
};

void output_chunk_to_file(std::ofstream& output_stream,
                          u32 tuple_count,
                          const std::pair<u32, u32>& counter,
                          const std::vector<u8>& decompressed_column,
                          std::vector<BtrReader>& readers,
                          bool requires_copy);

bool reader_is_null(BtrReader& reader, u32 index, size_t row);

void convert_csv(const string csv_path,
                 vector<ColumnMetadata> columns,
                 const string& out_dir,
                 const string& csv_separator);
Relation read_directory(vector<ColumnMetadata> columns, const string& columns_dir);

}  // namespace btrWrapper
