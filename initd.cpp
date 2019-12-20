#ifndef _WIN32

#  include <iostream>
#  include <unistd.h>
#  include <signal.h>
#  include <sys/param.h>
#  include <sys/types.h>
#  include <sys/stat.h>

void sighandler(int signum)
{
  if (SIGUSR1 == signum)
  {
    std::cout << "catched external stop signal, now stop the server...\n";

    std::cout << "post stop signals to server sucessfully.\n";
  }
}

void sinitd(void)
{
  signal(SIGHUP, sighandler);
  signal(SIGUSR1, sighandler);
}

void pinitd(void)
{
  int pid;
  int i;
  if (pid = fork())
  {
    exit(0);
  }
  else if (pid < 0)
  {
    exit(1);
  }

  setsid();

  if (pid = fork())
  {
    exit(0);
  }
  else if (pid < 0)
  {
    exit(1);
  }

  for (i = 0; i < NOFILE; ++i)
  {
    close(i);
  }

  (int)chdir("/tmp");
  umask(0);
  return;
}

#endif
