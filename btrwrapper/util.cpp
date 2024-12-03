// ------------------------------------------------------------------------------
#include <string>
#include <filesystem>

#include "btrblocks.hpp"

#include <csv-parser/parser.hpp>
#include "../btrfiles/Trim.hpp"
#include "common/Utils.hpp"
#include "compression/BtrReader.hpp"
#include "cxx.h"
#include "scheme/SchemePool.hpp"
#include "util.hpp"
// ------------------------------------------------------------------------------
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

void output_chunk_to_file(std::ofstream& output_stream,
                          u32 tuple_count,
                          const std::pair<u32, u32>& counter,
                          const std::vector<u8>& decompressed_column,
                          std::vector<BtrReader>& readers,
                          bool requires_copy) {
  for (size_t row = 0; row < tuple_count; row++) {
    BtrReader& reader = readers[counter.first];
    bool is_null = reader_is_null(reader, counter.second - 1, row);

    if (!is_null) {
      switch (reader.getColumnType()) {
        case ColumnType::INTEGER: {
          auto int_array = reinterpret_cast<const INTEGER*>(decompressed_column.data());
          output_stream << int_array[row];
          break;
        }
        case ColumnType::DOUBLE: {
          auto double_array = reinterpret_cast<const DOUBLE*>(decompressed_column.data());
          output_stream << double_array[row];
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
          output_stream << data;
          break;
        }
        default: {
          throw Generic_Exception("Type " + ConvertTypeToString(reader.getColumnType()) +
                                  " not supported");
        }
      }
    } else {
      output_stream << "null";
    }
    output_stream << "\n";
  }
}


void convert_csv(const string csv_path,
                 vector<ColumnMetadata> columns_metadata,
                 const string& out_dir,
                 const string& csv_separator) {
  vector<ColumnDescriptor> columns;

  // vector of vector for each type
  vector<vector<s32>> integer_vectors;
  vector<vector<double>> double_vectors;
  vector<vector<string>> string_vectors;

  if (mkdir(out_dir.c_str(), S_IRWXU | S_IRWXG) && errno != EEXIST) {
    cerr << "creating output directory failed, status = " << errno << endl;
  }
  // read meta data and prepare columns
  {
    for (const auto& column : columns_metadata) {
      const string column_name = column.name;
      const string column_type = column.type;

      ColumnType type = ColumnType::UNDEFINED;
      u32 vector_offset = 0;

      // ugly code
      if (column_type == "integer" || column_type == "smallint") {
        type = ColumnType::INTEGER;
        integer_vectors.push_back({});
        vector_offset = integer_vectors.size() - 1;
      } else if (column_type == "double" || column_type == "float") {
        type = ColumnType::DOUBLE;
        double_vectors.push_back({});
        vector_offset = double_vectors.size() - 1;
      } else if (column_type == "string") {
        type = ColumnType::STRING;
        string_vectors.push_back({});
        vector_offset = string_vectors.size() - 1;
      } else {
        type = ColumnType::SKIP;
      }
      columns.push_back({column_name, type, vector_offset, {}});
    }
  }
  // read csv data and fill the columns
  {
    std::ifstream csv_stream(csv_path.c_str());
    aria::csv::CsvParser parser =
        aria::csv::CsvParser(csv_stream).delimiter(*csv_separator.c_str()).terminator('\n');

    u32 tuple_i = 0;
    // u32 column_count = parser.begin()->size();
    for (auto& tuple : parser) {
      tuple_i++;
      u32 col_i = 0;
      string column_debug_str;  // for debugging
      try {
        for (auto& column_csv_str : tuple) {
          auto& column_descriptor = columns[col_i++];
          if (column_descriptor.column_type == ColumnType::SKIP)  // TODO: Reset
            continue;

          string column_str;
          if (column_descriptor.column_type != ColumnType::STRING) {
            column_str = trim_copy(column_csv_str);
            if (column_str.size() != column_csv_str.size()) {
              cout << "WARNING: trimmed col = '" << column_str << "' and untrimmed = '"
                   << column_csv_str << "'" << endl;
            }
          } else {
            column_str = column_csv_str;
          }
          column_debug_str = column_str;
          switch (column_descriptor.column_type) {
            case ColumnType::INTEGER: {
              const bool is_set = (column_str.size() == 0 || column_str == "null") ? 0 : 1;
              column_descriptor.set_bitmap.push_back(is_set);
              // -------------------------------------------------------------------------------------
              const INTEGER value = (is_set ? std::stoi(column_str) : NULL_CODE);
              die_if(is_set || value == NULL_CODE);
              integer_vectors[column_descriptor.vector_offset].push_back(value);
              // -------------------------------------------------------------------------------------
              // Update stats
              column_descriptor.null_count += !is_set;
              column_descriptor.empty_count += (value == 0) ? 1 : 0;
              break;
            }
            case ColumnType::DOUBLE: {
              const bool is_set = (column_str.size() == 0 || column_str == "null") ? 0 : 1;
              column_descriptor.set_bitmap.push_back(is_set);
              // -------------------------------------------------------------------------------------
              DOUBLE value = (is_set ? std::stod(column_str) : CD(NULL_CODE));
              double_vectors[column_descriptor.vector_offset].push_back(value);
              // -------------------------------------------------------------------------------------
              // Update stats
              column_descriptor.null_count += !is_set;
              column_descriptor.empty_count += (value == 0) ? 1 : 0;
              break;
            }
            case ColumnType::STRING: {
              const bool is_set = (column_str == "null") ? 0 : 1;
              column_descriptor.set_bitmap.push_back(is_set);
              // -------------------------------------------------------------------------------------
              string_vectors[column_descriptor.vector_offset].push_back(is_set ? column_str : "");
              // -------------------------------------------------------------------------------------
              // Update stats
              column_descriptor.null_count += !is_set;
              column_descriptor.empty_count += (column_str.size() == 0) ? 1 : 0;
              break;
            }
            default:
              UNREACHABLE();
          }
        }
      } catch (std::exception& e) {
        cout << "exception thrown during parsing tuple_i = " << tuple_i
             << " column col_i = " << col_i << " name =" << columns[col_i].name
             << " type = " << ConvertTypeToString(columns[col_i].column_type)
             << " with column = " << column_debug_str << endl;
        cout << "DUMP TUPLE" << endl;
        for (u32 i = 0; i < tuple.size(); i++) {
          cout << i << " : " << tuple[i] << endl;
        }
        throw Generic_Exception("failed to parse tuple:" + std::string(e.what()));
      }
    }
  }
  // write data to binary
  {
    for (u32 col_i = 0; col_i < columns.size(); col_i++) {
      auto& column_descriptor = columns[col_i];
      if (column_descriptor.column_type == ColumnType::SKIP)
        continue;
      string output_column_file =
          out_dir + std::to_string(col_i + 1) + "_" + column_descriptor.name;
      // -------------------------------------------------------------------------------------
      // Write Bitmap
      const string output_column_bitmap_file = output_column_file + ".bitmap";
      writeBinary(output_column_bitmap_file.c_str(), column_descriptor.set_bitmap);
      // -------------------------------------------------------------------------------------
      switch (column_descriptor.column_type) {
        case ColumnType::INTEGER: {
          output_column_file += ".integer";
          die_if(integer_vectors[column_descriptor.vector_offset].size() ==
                 column_descriptor.set_bitmap.size());
          writeBinary(output_column_file.c_str(), integer_vectors[column_descriptor.vector_offset]);
          break;
        }
        case ColumnType::DOUBLE: {
          output_column_file += ".double";
          writeBinary(output_column_file.c_str(), double_vectors[column_descriptor.vector_offset]);
          break;
        }
        case ColumnType::STRING: {
          output_column_file += ".string";
          writeBinary(output_column_file.c_str(), string_vectors[column_descriptor.vector_offset]);
          break;
        }
        default:
          UNREACHABLE();
      }
      // -------------------------------------------------------------------------------------
      // Write Stats
      output_column_file += ".stats";
      std::ofstream stats_file(output_column_file);
      stats_file << "null_count|empty_count\n";
      stats_file << column_descriptor.null_count << "|" << column_descriptor.empty_count << endl;
      stats_file.close();
    }
  }
}

Relation read_directory(vector<ColumnMetadata> columns, const string& columns_dir) {
  die_if(columns_dir.back() == '/');
  Relation result;
  result.columns.reserve(columns.size());
  for (u32 column_i = 0; column_i < columns.size(); column_i++) {
    const auto& column = columns[column_i];
    const auto column_name = column.name;
    auto column_type = column.type;
    if (column_type == "smallint") {
      column_type = "integer";
    } else if (column_type == "float") {
      column_type = "double";
    }
    const string column_file_prefix =
        columns_dir + std::to_string(column_i + 1) + "_" + column_name;
    const string column_file_path = column_file_prefix + "." + column_type;
    if (column_type == "integer" || column_type == "double" || column_type == "string") {
      result.addColumn(column_file_path);
    }
  }
  return result;
}

}  // namespace btrWrapper
