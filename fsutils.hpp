#pragma once
#include "tinydir/tinydir.h"
#include "yasio/cxx17/string_view.hpp"

namespace fsutils
{
bool is_dir_exists(cxx17::string_view path);
bool is_file_exists(cxx17::string_view path);
long long get_file_size(cxx17::string_view path, bool& isdir);
void to_styled_path(std::string& path);
} // namespace fsutils
