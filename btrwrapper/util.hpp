
#include "btrblocks.hpp"
#include "yaml-cpp/yaml.h"

namespace btrWrapper {
using namespace btrblocks;

void convert_csv(const string csv_path, const YAML::Node &schema, const string &out_dir, const string &csv_separator);
Relation read_directory(const YAML::Node& schema, const string& columns_dir);

}  // namespace btrWrapper

