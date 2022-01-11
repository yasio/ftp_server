#include "fsutils.hpp"

namespace fsutils
{
#if defined(_WIN32)
#  define posix_stat_st struct _stat64
#  define posix_stat _stati64
#else
#  define posix_stat_st struct stat
#  define posix_stat ::stat
#endif
bool is_dir_exists(cxx17::string_view path)
{
  posix_stat_st st;
  if (0 == posix_stat(path.data(), &st))
    return st.st_mode & S_IFDIR;
  return false;
}
bool is_file_exists(cxx17::string_view path)
{
  posix_stat_st st;
  if (0 == posix_stat(path.data(), &st))
    return st.st_mode & S_IFREG;
  return false;
}
long long get_file_size(cxx17::string_view path, bool& isdir)
{
  posix_stat_st st;
  if (0 == posix_stat(path.data(), &st))
    if (st.st_mode & S_IFREG)
      return st.st_size;
    else
      isdir = true;
  return -1;
}
#if defined(_WIN32)
void to_styled_path(std::string& path) { std::replace(path.begin(), path.end(), '/', '\\'); }
#else
void to_styled_path(std::string& /*path*/) {}
#endif
} // namespace fsutils
