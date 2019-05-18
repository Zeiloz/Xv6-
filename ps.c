#ifdef CS333_P2
#include "types.h"
#include "user.h"
#include "uproc.h"

int
main(int argc, char *argv[])
{
  int max = 16;
  int elapse_time;
  int elapse_remain;
  int cpu_time;
  int cpu_remain;

  if(max < 0)
  {
    printf(2, "Error: max value is less than 0");
    exit();
  }

  struct uproc* table = malloc(max * sizeof(struct uproc));
  if (!table) {
    printf(2, "Error: malloc() call failed.");
    exit();
  }

  int num_procs = getprocs(max, table);

  if(num_procs < 0)
  {
    printf(2, "\nInvalid max or table");
    exit();
  }

  printf(2, "\nPID\tName\t\tUID\tGID\tPPID");
#ifdef CS333_P4
  printf(2, "\tPrio");
#endif
  printf(2, "\tElapsed\t\tCPU Time\tState\tSize\n");

  if(num_procs >= 0)
  {
    for(int i = 0; i < num_procs; ++i)
    {
      printf(2,"%d\t", table[i].pid);
      // check strlen of process name to prettify the printed columns.
      if(strlen(table[i].name) < 8)
        printf(2,"%s\t\t%d\t%d\t%d\t", table[i].name, table[i].uid, table[i].gid, table[i].ppid);
      if(strlen(table[i].name) >= 8)
        printf(2,"%s\t%d\t%d\t%d\t", table[i].name, table[i].uid, table[i].gid, table[i].ppid);

#ifdef CS333_P4
      printf(2, "%d\t", table[i].priority);
#endif
      elapse_time = table[i].elapsed_ticks/1000;
      elapse_remain= table[i].elapsed_ticks%1000;
      if(elapse_remain == 0)
        printf(2, "%d\t\t", elapse_time);
      else if(elapse_remain < 10)
        printf(2,"%d.00%d\t\t", elapse_time, elapse_remain);
      else if(elapse_remain < 100)
        printf(2, "%d.0%d\t\t", elapse_time, elapse_remain);
      else
        printf(2, "%d.%d\t\t", elapse_time, elapse_remain);

      cpu_time = table[i].CPU_total_ticks/1000;
      cpu_remain = table[i].CPU_total_ticks%1000;
      if(cpu_remain == 0)
        printf(2, "%d\t\t", cpu_time);
      else if(cpu_remain < 10)
        printf(2,"%d.00%d\t\t", cpu_time, cpu_remain);
      else if(cpu_remain < 100)
        printf(2, "%d.0%d\t\t", cpu_time, cpu_remain);
      else
        printf(2, "%d.%d\t\t", cpu_time, cpu_remain);
      printf(2, "%s\t%d\n", table[i].state, table[i].size);
    }
  }

  free(table);

  exit();
}
#endif

