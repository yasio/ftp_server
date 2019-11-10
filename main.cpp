#include "ftp_server.hpp"

int main(int argc, char** argv)
{
  if (argc < 2)
    return EINVAL;

  cxx17::string_view wwwroot = argv[1];
  if (!fsutils::is_dir_exists(wwwroot))
    return ENOENT;

  ftp_server server(wwwroot);

  server.run();

  return 0;
}
