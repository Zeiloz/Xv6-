#ifdef CS333_P4
#include "types.h"
#include "user.h"

int
main(int argc, char *argv[])
{
  setpriority(10, 0);
  setpriority(11, 0);
  setpriority(12, 0);
  setpriority(13, 0);
  setpriority(14, 0);
  setpriority(10, 0);
  exit();
}
#endif
