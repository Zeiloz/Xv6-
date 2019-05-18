#ifdef CS333_P2
#include "types.h"
#include "user.h"

static int
exec_helper(int current,char ** ptr)
{
  int ret = fork();
  if(ret < 0)
  {
    printf(2, "fork() failed\n");
    return -2;
  }
  if(ret == 0)
  {
    exec(ptr[current], ptr+current);
    return -1;
  }
  else
  {
  wait();
  }
  return 0;
}

int
main(int argc, char *argv[])
{
  int start, end;
  start = uptime();
  int res = exec_helper(1, argv);
  if(res == -2)
  {
    printf(2, "fork() failed. Unable to execute.", argv[1]);
    exit();
  }

  if(res == -1)
  {
    exit();
  }

  end = uptime();
  int total_time = (end-start)/1000;
  int total_remain = (end-start)%1000;
  if(total_remain == 0)
    printf(2, "%s ran in %d seconds\n", argv[1], total_time);
  else if(total_remain < 10)
    printf(2, "%s ran in %d.00%d seconds\n", argv[1], total_time, total_remain);
  else if(total_remain < 100)
    printf(2, "%s ran in %d.0%d seconds\n", argv[1], total_time, total_remain);
  else
    printf(2, "%s ran in %d.%d seconds\n", argv[1], total_time, total_remain);
  exit();
}
#endif
