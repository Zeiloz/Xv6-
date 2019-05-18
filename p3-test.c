//Project test code, credit to Cole Christian
#ifdef CS333_P3
#include "types.h"
#include "user.h"

void
controlFTest(void)
{
  int n;
  int pid;
  int times;

  times = 4;

  for(n=1; n < times; ++n){
    printf(1,"\nFork Call # %d\n",n);
    pid = fork();
    if(pid < 0){
      printf(1,"Failure in %s %s line %d",__FILE__, __FUNCTION__,__LINE__);
    }
    if(pid == 0){
      printf(1, "Child Process %d is now sleeping for %d seconds. Use Control-p followed by Control-f within 5 sec\n", n, times*5);
      sleep(5*(times)*TPS);
      exit();
    }
    else{
      sleep(5*TPS);
    }
  }
  if(pid != 0){
    for(n=1; n < times; ++n){
      wait();
      printf(1,"Child Process %d has exited.\n", n);
    }
  }
}

void
controlSTest(void)
{
  int n;
  int pid;
  int times;

  times = 4;

  for(n=1; n < times; ++n){
    printf(1,"\nFork Call # %d\n",n);
    pid = fork();
    if(pid < 0){
      printf(1,"Failure in %s %s line %d",__FILE__, __FUNCTION__,__LINE__);
    }
    if(pid == 0){
      printf(1, "Child Process %d is now sleeping for %d seconds. Use Control-p followed by Control-s within 5 sec\n", n, times*5);
      sleep(5*(times)*TPS);
      exit();
    }
    else{
      sleep(5*TPS);
    }
  }
  if(pid != 0){
    for(n=1; n < times; ++n){
      wait();
      printf(1,"Child Process %d has exited.\n", n);
    }
  }
}

void
controlZTest(void)
{
  int n;
  int pid;
  int times;

  times = 4;

  for(n=1; n < times; ++n){
    printf(1,"\nFork Call # %d\n",n);
    pid = fork();
    if(pid < 0){
      printf(1,"Failure in %s %s line %d",__FILE__, __FUNCTION__,__LINE__);
    }
    if(pid == 0){
      printf(1, "Child Process %d has exited. Use Control-p followed by Control-z within 5 sec\n", n);
      exit();
    }
    else{
      sleep(5*TPS);
    }
  }
  if(pid != 0){
    sleep(5*TPS);
    for(n=1; n < times; ++n){
      wait();
      printf(1,"Wait() has been called on Child Process %d.\n", n);
    }
    printf(1, "USE Control-p followed by Control-z to show zombie list is empty after calling wait");
    sleep(5*TPS);
  }
}

void
controlRTest(void)
{
  int n;
  int pid;
  int times;

  times = 5;

  for(n=1; n < times; ++n){
    printf(1,"\nFork Call # %d\n",n);
    pid = fork();
    if(pid < 0){
      printf(1,"Failure in %s %s line %d",__FILE__, __FUNCTION__,__LINE__);
    }
    if(pid == 0){
      printf(1, "Child Process %d is running for %d seconds. Use Control-p followed by Control-r within 5 sec\n", n, 5*times);
      int i;
      i = uptime();
      while((uptime()-i) < (1000*5*times)){
      }
      exit();
    }
    else{
      sleep(5*TPS);
    }
  }
  if(pid != 0){
    for(n=1; n < times; ++n){
      wait();
      printf(1,"Wait() has been called on Child Process %d.\n", n);
    }
  }
}

void
killTest(void)
{
  int pid;
  pid = fork();
    if(pid < 0){
      printf(1,"Failure in %s %s line %d",__FILE__, __FUNCTION__,__LINE__);
    }
  if(pid == 0)
  {
    printf(1, "Child Process is running forever. Use Control-p and z to show this.\nAfter 5 seconds, the process will be killed.\n");
    while(1){}
  }
  else{
    sleep(8*TPS);
    kill(pid);
    printf(1, "Child Process %d has been killed. Use control-p and z to show that its on the zombie list. You have 5 sec\n", pid);
    sleep(5*TPS);
    wait();
    printf(1,"Wait() has been called on Child Process %d. Use control-p, z, f to show that is removed from zombie list and added to unused.\nYou have 10 sec\n",pid);
    sleep(10*TPS);
  }
}

void
exitTest(void)
{
  int pid;
  pid = fork();
    if(pid < 0){
      printf(1,"Failure in %s %s line %d",__FILE__, __FUNCTION__,__LINE__);
    }
  if(pid == 0)
  {
    exit();
  }
  else{
    printf(1, "Child Process has been exited. It should be in the Zombie list, use control-p, z, f to show this. YOU HAVE 5 SECONDS\n");
    sleep(5*TPS);
    wait();
    printf(1, "Wait is called on child process. Use control-p, z, f to show child process is not in Zombie list, but moved to free list.\n");
    sleep(5*TPS);
  }
  //if(pid >= 0){
  //}
}

int
main(int argc, char *argv[])
{
  //Takes aguments from the command line
  int test;
  test = *argv[1];

  switch(test){
    case '1':
      printf(1,"----------- TEST 1 Control-f ----------\n");
      controlFTest();
      printf(1,"\n---------- TEST 1 COMPLETE ----------\n");
      break;
    case '2':
      printf(1,"\n----------- TEST 2 Control-s ----------\n");
      controlSTest();
      printf(1,"\n---------- TEST 2 COMPLETE ----------\n");
      break;
    case '3':
      printf(1,"\n----------- TEST 3 Control-z ----------\n");
      controlZTest();
      printf(1,"\n---------- TEST 3 COMPLETE ----------\n");
      break;
    case '4':
      printf(1,"\n----------- TEST 4 Control-r ----------\n");
      controlRTest();
      printf(1,"\n---------- TEST 4 COMPLETE ----------\n");
      break;
    case '5':
      printf(1,"\n----------- TEST 5 Kill() Zombie List ----------\n");
      killTest();
      printf(1,"\n---------- TEST 5 COMPLETE ----------\n");
      break;
    case '6':
      printf(1,"\n----------- TEST 6 exit() Zombie List ----------\n");
      exitTest();
      printf(1,"\n---------- TEST 6 COMPLETE ----------\n");
      break;
    default:
      break;
  }
  exit();

}
#endif
