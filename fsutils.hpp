#pragma once
#include "tinydir/tinydir.h"
#include "yasio/cxx17/string_view.hpp"

namespace fsutils
{
#if defined(_WIN32)
static bool is_dir_exists(cxx17::string_view path)
{
  WIN32_FILE_ATTRIBUTE_DATA attrs = {0};
  return GetFileAttributesExA(path.data(), GetFileExInfoStandard, &attrs) &&
         !!(attrs.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
}

static bool is_file_exists(cxx17::string_view path)
{
  WIN32_FILE_ATTRIBUTE_DATA attrs = {0};
  return GetFileAttributesExA(path.data(), GetFileExInfoStandard, &attrs) &&
         !(attrs.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
}
static long long get_file_size(cxx17::string_view path)
{
  if (path.empty())
    return -1;
  WIN32_FILE_ATTRIBUTE_DATA attrs = {0};
  if (GetFileAttributesExA(path.data(), GetFileExInfoStandard, &attrs) &&
      !(attrs.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
    return static_cast<long long>(attrs.nFileSizeHigh) << 32 |
           static_cast<unsigned long long>(attrs.nFileSizeLow);
  return -1;
}
#else
static bool is_dir_exists(cxx17::string_view path)
{
  struct stat st;
  if (0 == ::stat(path.data(), &st))
    return st.st_mode & S_IFDIR;
  return false;
}

static bool is_file_exists(cxx17::string_view path)
{
  struct stat st;
  if (0 == ::stat(path.data(), &st))
    return st.st_mode & S_IFREG;
  return false;
}
static long long get_file_size(cxx17::string_view path)
{
  struct stat st;
  if (0 == ::stat(path.data(), &st))
    return st.st_mode & S_IFREG ? st.st_size : 0;
  return -1;
}
#endif
} // namespace fsutils
