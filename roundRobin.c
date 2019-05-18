//got this from forktest.c
#ifdef CS333_P3
#include "types.h"
#include "user.h"

int
main(void)
{
  int pid = 0;
  for(int i = 0; i < 50; ++ i) {
    pid = fork();
    if (pid == 0)
      while(1) i++;
  }
  exit();
}
#endif

