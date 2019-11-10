#include "ftp_server.hpp"

int main(int argc, char** argv)
{
  if (argc < 2)
    return EINVAL;

  cxx17::string_view wwwroot = argv[1];
  if (!fsutils::is_dir_exists(wwwroot))
    return ENOENT;

#if !defined(_WIN32) // runas daemon at linux platform.
  extern void sinitd(void);
  sinitd();
#endif
  ftp_server server(wwwroot);

  server.run();

  return 0;
}
