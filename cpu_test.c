#ifdef CS333_P2
#include "types.h"
#include "user.h"


int
main(int argc, char **argv)
{
  printf(2, "PRESS CONTROL-P\n");
  sleep(5*TPS);  //press control-p
  printf(2, "LOOPING...\n");
  int counter = 0;
  int max = 1000000000;
  for(int i = 0; i < max; ++i)
  {
    if(counter < max) ++counter;
  }
  printf(2, "PRESS CONTROL-P\n");
  sleep(5*TPS);  //press control-p
  exit();
}
#endif
