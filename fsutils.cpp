#include "fsutils.hpp"
#include <algorithm>

namespace fsutils
{
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

void list_files(const std::string& dirPath, const std::function<void(tinydir_file&)>& callback, bool recursively)
{
  if (fsutils::is_dir_exists(dirPath))
  {
    tinydir_dir dir;
    std::string fullpathstr = dirPath;

    if (tinydir_open(&dir, &fullpathstr[0]) != -1)
    {
      while (dir.has_next)
      {
        tinydir_file file;
        if (tinydir_readfile(&dir, &file) == -1)
        {
          // Error getting file
          break;
        }
        std::string fileName = file.name;

        if (fileName != "." && fileName != "..")
        {
          callback(file);
          if (file.is_dir && recursively)
            list_files(file.path, callback, recursively);
        }

        if (tinydir_next(&dir) == -1)
        {
          // Error getting next file
          break;
        }
      }
    }
    tinydir_close(&dir);
  }
}
} // namespace fsutils
