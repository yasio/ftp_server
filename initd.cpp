#ifndef _WIN32

#  include <stdio.h>
#  include <unistd.h>
#  include <signal.h>
#  include <sys/param.h>
#  include <sys/types.h>
#  include <sys/stat.h>

void sighandler(int signum)
{
  if (SIGUSR1 == signum)
    printf("%s", "catched external stop signal, now stop the server...\npost stop signals to "
                 "server sucessfully.\n");
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
  int ret = 0;
  if ((pid = fork()))
  {
    exit(0);
  }
  if (pid < 0)
  {
    exit(1);
  }

  setsid();

  if ((pid = fork()))
  {
    exit(0);
  }
  if (pid < 0)
  {
    exit(1);
  }

  for (i = 0; i < NOFILE; ++i)
  {
    close(i);
  }

  ret = chdir("/tmp");
  printf("chdir result=%d\n", ret);
  umask(0);
  return;
}

#endif
