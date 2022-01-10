#include "fsutils.hpp"

namespace fsutils
{
bool is_absolute_path(cxx17::string_view path)
{
  const char* raw = path.data();
#if defined(_WIN32)
  // see also: https://docs.microsoft.com/en-us/windows/win32/fileio/naming-a-file?redirectedfrom=MSDN
  return ((path.length() > 2 && ((raw[0] >= 'a' && raw[0] <= 'z') || (raw[0] >= 'A' && raw[0] <= 'Z')) && raw[1] == ':') // Normal absolute path
          || cxx20::starts_with(path, R"(\\?\)")                                                                         // Win32 File Namespaces for Long Path
          || cxx20::starts_with(path, R"(\\.\)")                                                                         // Win32 Device Namespaces for device
          || (raw[0] == '/' || raw[0] == '\\')                                                                           // Current disk drive
  );
#else
  return (raw[0] == '/');
#endif
}
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
