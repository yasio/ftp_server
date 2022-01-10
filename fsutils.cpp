#include "fsutils.hpp"

namespace fsutils
{
#if defined(_WIN32)
bool is_dir_exists(cxx17::string_view path)
{
  WIN32_FILE_ATTRIBUTE_DATA attrs = {0};
  return GetFileAttributesExA(path.data(), GetFileExInfoStandard, &attrs) &&
         !!(attrs.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
}
bool is_file_exists(cxx17::string_view path)
{
  WIN32_FILE_ATTRIBUTE_DATA attrs = {0};
  return GetFileAttributesExA(path.data(), GetFileExInfoStandard, &attrs) &&
         !(attrs.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
}
long long get_file_size(cxx17::string_view path)
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
bool is_dir_exists(cxx17::string_view path)
{
  struct stat st;
  if (0 == ::stat(path.data(), &st))
    return st.st_mode & S_IFDIR;
  return false;
}
bool is_file_exists(cxx17::string_view path)
{
  struct stat st;
  if (0 == ::stat(path.data(), &st))
    return st.st_mode & S_IFREG;
  return false;
}
long long get_file_size(cxx17::string_view path)
{
  struct stat st;
  if (0 == ::stat(path.data(), &st))
    return st.st_mode & S_IFREG ? st.st_size : 0;
  return -1;
}
#endif
} // namespace fsutils
