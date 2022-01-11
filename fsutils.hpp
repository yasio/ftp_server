#pragma once
#include "tinydir/tinydir.h"
#include <functional>
#include "yasio/cxx17/string_view.hpp"
#if defined(_WIN32)
#  include "ntcvt/ntcvt.hpp"
#endif

#if defined(_WIN32)
#  define gmtime_r(tp, tr) gmtime_s(tr, tp)
#  define posix_stat_st struct _stat64
#  define posix_stat _stati64
#  define posix_ustat _wstati64
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
#if defined(_WIN32)
bool is_dir_exists(cxx17::wstring_view path);
bool is_file_exists(cxx17::wstring_view path);
#endif
long long get_file_size(cxx17::string_view path, bool& isdir);
void to_styled_path(std::string& path);
void list_files(const std::string& dirPath, const std::function<void(tinydir_file&)>& callback, bool recursively = false);
} // namespace fsutils
