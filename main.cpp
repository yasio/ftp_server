/*
* usage:
*  ftp_server wwwroot <wanip>
*/
#include "ftp_server.hpp"

#include "initd.h"

#if defined(_HAVE_INITD)
extern void sinitd(void);
#endif

int main(int argc, char** argv)
{
  if (argc < 2) {
    fprintf(stderr, "%s", "Usage: ftp_server wwwroot <wanip>\n");
    return EINVAL;
  }

  cxx17::string_view wwwroot = argv[1];
  if (!fsutils::is_dir_exists(wwwroot))
    return ENOENT;

  cxx17::string_view wanip;
  if (argc >= 3)
  {
    wanip = argv[2];
  }

#if defined(_HAVE_INITD) // runas daemon at unix platform.
  sinitd();
#endif
  ftp_server server(wwwroot, wanip);

  server.run(20);

  return 0;
}
