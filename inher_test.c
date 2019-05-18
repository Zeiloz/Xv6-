//Code is taken from testuidcid.c
#ifdef CS333_P2
#include "types.h"
#include "user.h"

static void
forkTest(uint nval)
{
  uint uid, gid;
  int pid;

  printf(1, "Setting UID to %d and GID to %d before fork(). Value"
                  " should be inherited\n", nval, nval);

  if (setuid(nval) < 0)
    printf(2, "Error. Invalid UID: %d\n", nval);
  if (setgid(nval) < 0)
    printf(2, "Error. Invalid UID: %d\n", nval);

  printf(1, "Before fork(), UID = %d, GID = %d\n", getuid(), getgid());
  pid = fork();
  if (pid == 0) {  // child
    uid = getuid();
    gid = getgid();
    printf(1, "Child: UID is: %d, GID is: %d\n", uid, gid);
    sleep(5 * TPS);  // now type control-p
    exit();
  }
  else
    sleep(10 * TPS); // wait for child to exit before proceeding

}

int
main()
{
  uint nval;
  nval = 111;
  forkTest(nval);
  exit();
}
#endif

