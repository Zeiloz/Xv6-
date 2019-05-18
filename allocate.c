#ifdef CS333_P3
#include "types.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  int rc = fork();
  if(rc == 0)
  {
    printf(2,"Do Ctrl-S and Ctrl-F....\n");
    sleep(3*TPS);
    exit();
  }
  wait();
  sleep(3*TPS);
  exit();
}
#endif
