#pragma once
#include "tinydir/tinydir.h"
#include <functional>
#include "yasio/string_view.hpp"
#include <sys/stat.h>
#if defined(_WIN32) && !defined(__MINGW64__) && !defined(__MINGW32__)
#  include "ntcvt/ntcvt.hpp"
#  define posix_stat_st struct _stat64
#  define posix_stat _stat64
#  define posix_ustat _wstat64
#  define posix_upath(path) ntcvt::from_chars(path, CP_UTF8)
#  define posix_fopen(path, mode) _wfopen(ntcvt::from_chars(path, CP_UTF8).c_str(), TEXT(mode))
#else
#  define posix_stat_st struct stat
#  define posix_stat ::stat
#  define posix_ustat posix_stat
#  define posix_upath(path) path
#  define posix_fopen(path, mode) fopen(path.c_str(), mode)
#endif

namespace fsutils
{
bool is_dir_exists(cxx17::string_view path);
bool is_file_exists(cxx17::string_view path);
#if defined(_WIN32) && !defined(__MINGW64__) && !defined(__MINGW32__)
bool is_dir_exists(cxx17::wstring_view path);
bool is_file_exists(cxx17::wstring_view path);
#endif
long long get_file_size(cxx17::string_view path, bool& isdir);
void to_styled_path(std::string& path);
void list_files(const std::string& dirPath, const std::function<void(tinydir_file&)>& callback,
                bool recursively = false);
} // namespace fsutils
