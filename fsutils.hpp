#pragma once
#include "tinydir/tinydir.h"
#include <functional>
#include "yasio/cxx17/string_view.hpp"

#if defined(_WIN32)
#  define posix_stat_st struct _stat64
#  define posix_stat _stati64
#else
#  define posix_stat_st struct stat
#  define posix_stat ::stat
#endif

#if defined(_WIN32)
#  define gmtime_r(tp, tr) gmtime_s(tr, tp)
inline FILE* sfopen(const char* filename, const char* mode)
{
  FILE* fp = nullptr;
  fopen_s(&fp, filename, mode);
  return fp;
}
#else
#  define sfopen fopen
#endif

namespace fsutils
{
bool is_dir_exists(cxx17::string_view path);
bool is_file_exists(cxx17::string_view path);
long long get_file_size(cxx17::string_view path, bool& isdir);
void to_styled_path(std::string& path);
void list_files(const std::string& dirPath, const std::function<void(tinydir_file&)>& callback, bool recursively = false);
} // namespace fsutils
