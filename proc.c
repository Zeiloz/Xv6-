#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

#ifdef CS333_P2
#include "uproc.h"
#endif

static char *states[] = {
[UNUSED]    "unused",
[EMBRYO]    "embryo",
[SLEEPING]  "sleep ",
[RUNNABLE]  "runble",
[RUNNING]   "run   ",
[ZOMBIE]    "zombie"
};

#ifdef CS333_P3
struct ptrs {
  struct proc* head;
  struct proc* tail;
};
#endif

static struct {
  struct spinlock lock;
  struct proc proc[NPROC];
#ifdef CS333_P3
  struct ptrs list[statecount];
#endif

#ifdef CS333_P4
  struct ptrs ready[MAXPRIO+1];
  uint PromoteAtTime;
#endif
} ptable;

static struct proc *initproc;

uint nextpid = 1;
extern void forkret(void);
extern void trapret(void);
static void wakeup1(void* chan);

#ifdef CS333_P3
static void initProcessLists(void);
static void initFreeList(void);
static void assertState(struct proc*, enum procstate);
static void stateListAdd(struct ptrs*, struct proc*);
static int  stateListRemove(struct ptrs*, struct proc* p);
#endif



void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;

  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");

  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid) {
      return &cpus[i];
    }
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
#ifdef CS333_P3
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  p = ptable.list[UNUSED].head;
  if(!p)
    return 0;

  acquire(&ptable.lock);
  if(stateListRemove(&ptable.list[p->state], p) == -1)
    panic("Empty list or process not in list");
  assertState(p, UNUSED);
  p->state = EMBRYO;
  p->pid = nextpid++;
  stateListAdd(&ptable.list[p->state], p);
  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    acquire(&ptable.lock);
    if(stateListRemove(&ptable.list[p->state], p) == -1)
      panic("Empty list or process not in list");
    assertState(p, EMBRYO);
    p->state = UNUSED;
    stateListAdd(&ptable.list[p->state], p);
    release(&ptable.lock);
    return 0;
  }

  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  //initialized start_ticks to global kernel variable ticks
#ifdef CS333_p1
  p->start_ticks = ticks;
#endif

#ifdef CS333_P2
  p->cpu_ticks_total = 0;
  p->cpu_ticks_in = 0;
#endif

#ifdef CS333_P4
  p->budget = BUDGET;
  p->priority = MAXPRIO;
#endif

  return p;
}

#else
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);
  int found = 0;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED) {
      found = 1;
      break;
    }
  if (!found) {
    release(&ptable.lock);
    return 0;
  }
  p->state = EMBRYO;
  p->pid = nextpid++;
  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  //initialized start_ticks to global kernel variable ticks
#ifdef CS333_P1
  p->start_ticks = ticks;
#endif

#ifdef CS333_P2
  p->cpu_ticks_total = 0;
  p->cpu_ticks_in = 0;
#endif

  return p;
}
#endif

//PAGEBREAK: 32
// Set up first user process.
#ifdef CS333_P3
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  acquire(&ptable.lock);
  {
    initProcessLists();
    initFreeList();
  }
  release(&ptable.lock);

  p = allocproc();

  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);
  if(stateListRemove(&ptable.list[p->state], p) == -1)
    panic("Empty list or process not in list");
  assertState(p, EMBRYO);
  p->state = RUNNABLE;
  p->uid = DEFAULT_UID;
  p->gid = DEFAULT_GID;
#ifdef CS333_P4
  ptable.PromoteAtTime = ticks + TICKS_TO_PROMOTE;
  stateListAdd(&ptable.ready[p->priority], p);
#else
  stateListAdd(&ptable.list[p->state], p);
#endif
  release(&ptable.lock);
}

#else
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();

  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);
  p->state = RUNNABLE;
#ifdef CS333_P2
  p->uid = DEFAULT_UID;
  p->gid = DEFAULT_GID;
#endif
  release(&ptable.lock);
}
#endif

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
#ifdef CS333_P3
int
fork(void)
{
  int i;
  uint pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;

    acquire(&ptable.lock);
    if(stateListRemove(&ptable.list[np->state], np) == -1)
      panic("Empty List or process not in list");
    assertState(np, EMBRYO);
    np->state = UNUSED;
    stateListAdd(&ptable.list[np->state], np);
    release(&ptable.lock);
    return -1;
  }

#ifdef CS333_P2
  np->uid = curproc->uid;
  np->gid = curproc->gid;
#endif

  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;


  acquire(&ptable.lock);
  if(stateListRemove(&ptable.list[np->state], np) == -1)
    panic("Empty list or process not in list");
  assertState(np, EMBRYO);
  np->state = RUNNABLE;
#ifdef CS333_P4
  stateListAdd(&ptable.ready[MAXPRIO], np);
#else
  stateListAdd(&ptable.list[np->state], np);
#endif
  release(&ptable.lock);

  return pid;
}

#else
int
fork(void)
{
  int i;
  uint pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;

    np->state = UNUSED;
    return -1;
  }

#ifdef CS333_P2
  np->uid = curproc->uid;
  np->gid = curproc->gid;
#endif

  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);
  np->state = RUNNABLE;
  release(&ptable.lock);

  return pid;
}
#endif

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
#ifdef CS333_P3
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);


  // Pass abandoned children to init.
  for(int i = 1; i < statecount; ++i){
#ifdef CS333_P4
    if(i == RUNNABLE) {
      if(ptable.list[i].head)
        panic("The Runnable list should be empty");
      for(int j = MAXPRIO; j >= 0; --j) {
        p = ptable.ready[j].head;
        while(p) {
          if(p->parent == curproc) {
            p->parent = initproc;
          }
          p = p->next;
        }
      }
      continue;
    }
#endif
    p = ptable.list[i].head;
    while(p) {
      if(p->parent == curproc){
        p->parent = initproc;
        if(p->state == ZOMBIE)
          wakeup1(initproc);
      }
      p = p->next;
    }
  }

  // Jump into the scheduler, never to return.
  if(stateListRemove(&ptable.list[curproc->state], curproc) == -1)
    panic("Empty list or process not in list");
  assertState(curproc, RUNNING);
  curproc->state = ZOMBIE;
  stateListAdd(&ptable.list[curproc->state], curproc);
  sched();
  panic("zombie exit");
}

#else
void
exit(void)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}
#endif

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
#ifdef CS333_P3
int
wait(void)
{
  struct proc *p;
  int havekids;
  uint pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;

    for(int i = 1; i < statecount; ++i){
#ifdef CS333_P4
      if(i == RUNNABLE) {
        if(ptable.list[i].head)
          panic("The Runnable list should be empty");
        for(int j = MAXPRIO; j >= 0; --j) {
          p = ptable.ready[j].head;
          while(p && p->parent != curproc)
            p = p->next;
          if(!p)
            continue;
          havekids = 1;
        }
        continue;
      }
#endif
      p = ptable.list[i].head;
      while(p && p->parent != curproc)
        p = p->next;
      if(!p)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        if(stateListRemove(&ptable.list[p->state], p) == -1)
          panic("Empty List or process not in list");
        assertState(p, ZOMBIE);
        p->state = UNUSED;
        stateListAdd(&ptable.list[p->state], p);
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

#else
int
wait(void)
{
  struct proc *p;
  int havekids;
  uint pid;
  struct proc *curproc = myproc();

  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}
#endif

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.

#ifdef CS333_P3
void
scheduler(void)
{
  struct proc *p;
  struct proc *tmp;
  struct cpu *c = mycpu();
  c->proc = 0;
#ifdef PDX_XV6
  int idle;  // for checking if processor is idle
#endif // PDX_XV6

  for(;;){
    // Enable interrupts on this processor.
    sti();

#ifdef PDX_XV6
    idle = 1;  // assume idle unless we schedule a process
#endif // PDX_XV6

    acquire(&ptable.lock);
#ifdef CS333_P4
    //PromotionAll
    if(ticks >= ptable.PromoteAtTime) {
      for(int i = SLEEPING; i <= RUNNING; ++i) {
        if(i == RUNNABLE) {
          if(ptable.list[i].head)
            panic("Runnable list should be empty");
          for(int j = MAXPRIO-1; j >= 0; --j) {
            p = ptable.ready[j].head;
            while(p) {
              tmp = p->next;
              p->budget = BUDGET;
              if(p->priority < MAXPRIO) {
                if(stateListRemove(&ptable.ready[p->priority], p) == -1)
                  panic("Empty List or process is not in list");
                assertState(p, RUNNABLE);
                if(p->priority != j)
                  panic("p->priority not same as in ready list");
                stateListAdd(&ptable.ready[p->priority+1], p);
                ++p->priority;
              }
              p = tmp;
            }
          }
          continue;
        }
        p = ptable.list[i].head;
        while(p) {
          p->budget = BUDGET;
          if(p->priority < MAXPRIO) {
            ++p->priority;
          }
          p = p->next;
        }
      }
      ptable.PromoteAtTime = ticks + TICKS_TO_PROMOTE;
    }

    for(int i = MAXPRIO; i >= 0; --i) {
      p = ptable.ready[i].head;
      if(p) {
#ifdef PDX_XV6
        idle = 0;  // not idle this timeslice
#endif // PDX_XV6

        c->proc = p;
        switchuvm(p);
        if(stateListRemove(&ptable.ready[i], p) == -1)
          panic("Empty List or process is not in list");
        assertState(p, RUNNABLE);
        p->state = RUNNING;
        stateListAdd(&ptable.list[p->state], p);
        p->cpu_ticks_in = ticks;
        swtch(&(c->scheduler), p->context);
        switchkvm();
        c->proc = 0;
        break;
      }
    }

#else
    p = ptable.list[RUNNABLE].head;
    if(p) {
#ifdef PDX_XV6
      idle = 0;  // not idle this timeslice
#endif // PDX_XV6

      c->proc = p;
      switchuvm(p);

      if(stateListRemove(&ptable.list[p->state], p) == -1)
        panic("Empty List or process is not in list");
      assertState(p, RUNNABLE);
      p->state = RUNNING;
      stateListAdd(&ptable.list[p->state], p);


#ifdef CS333_P2
      p->cpu_ticks_in = ticks;
#endif

      swtch(&(c->scheduler), p->context);
      switchkvm();
      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }

#endif //CS333_P4
    release(&ptable.lock);

#ifdef PDX_XV6
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
#endif // PDX_XV6
  }
}

#else
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  c->proc = 0;
#ifdef PDX_XV6
  int idle;  // for checking if processor is idle
#endif // PDX_XV6

  for(;;){
    // Enable interrupts on this processor.
    sti();

#ifdef PDX_XV6
    idle = 1;  // assume idle unless we schedule a process
#endif // PDX_XV6
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->state != RUNNABLE)
        continue;

      // Switch to chosen process.  It is the process's job
      // to release ptable.lock and then reacquire it
      // before jumping back to us.
#ifdef PDX_XV6
      idle = 0;  // not idle this timeslice
#endif // PDX_XV6
#ifdef CS333_P2
      p->cpu_ticks_in = ticks;
#endif
      c->proc = p;
      switchuvm(p);
      p->state = RUNNING;
      swtch(&(c->scheduler), p->context);
      switchkvm();
      // Process is done running for now.
      // It should have changed its p->state before coming back.
      c->proc = 0;
    }
    release(&ptable.lock);
#ifdef PDX_XV6
    // if idle, wait for next interrupt
    if (idle) {
      sti();
      hlt();
    }
#endif // PDX_XV6
  }
}
#endif

// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
#ifdef CS333_P2
  p->cpu_ticks_total += (ticks - p->cpu_ticks_in);
#endif


  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
#ifdef CS333_P3
void
yield(void)
{
  struct proc *curproc = myproc();
  acquire(&ptable.lock);  //DOC: yieldlock
#ifdef CS333_P4
  curproc->budget -= (ticks - curproc->cpu_ticks_in);
  if(curproc->budget <= 0) {
    if(curproc->priority > 0)
      --curproc->priority;
    curproc->budget = BUDGET;
  }
#endif

  if(stateListRemove(&ptable.list[curproc->state], curproc) == -1)
    panic("Empty List or process not in list");
  assertState(curproc, RUNNING);
  curproc->state = RUNNABLE;
#ifdef CS333_P4
  stateListAdd(&ptable.ready[curproc->priority], curproc);
#else
  stateListAdd(&ptable.list[curproc->state], curproc);
#endif
  sched();
  release(&ptable.lock);
}

#else
void
yield(void)
{
  struct proc *curproc = myproc();

  acquire(&ptable.lock);  //DOC: yieldlock
  curproc->state = RUNNABLE;
  sched();
  release(&ptable.lock);
}
#endif

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
#ifdef CS333_P3
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  if(p == 0)
    panic("sleep");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    if (lk) release(lk);
  }
  // Go to sleep.
  if(stateListRemove(&ptable.list[p->state], p) == -1)
    panic("Empty list or process not in list");
  assertState(p, RUNNING);
  p->chan = chan;
  p->state = SLEEPING;
  stateListAdd(&ptable.list[p->state], p);

#ifdef CS333_P4
  p->budget -= (ticks - p->cpu_ticks_in);
  if(p->budget <= 0) {
    if(p->priority > 0)
      --p->priority;
    p->budget = BUDGET;
  }
#endif
  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    if (lk) acquire(lk);
  }
}

#else
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();

  if(p == 0)
    panic("sleep");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    if (lk) release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    if (lk) acquire(lk);
  }
}
#endif

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
#ifdef CS333_P3
static void
wakeup1(void *chan)
{
  struct proc *p;
  struct proc *temp;
  p = ptable.list[SLEEPING].head;
  while(p)
  {
    temp = p->next;
    if(p->chan == chan)
    {
      if(stateListRemove(&ptable.list[p->state], p) == -1)
        panic("Empty list or process not in list");
      assertState(p, SLEEPING);
      p->state = RUNNABLE;
#ifdef CS333_P4
      stateListAdd(&ptable.ready[p->priority], p);
#else
      stateListAdd(&ptable.list[p->state], p);
#endif
    }
    p = temp;
  }
}
#else
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan)
      p->state = RUNNABLE;
}
#endif

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
#ifdef CS333_P3
int
kill(int pid)
{
  struct proc *p;
  //struct proc* temp;

  acquire(&ptable.lock);
  for(int i = 1; i < statecount; ++i)
  {
#ifdef CS333_P4
    if(i == RUNNABLE) {
      if(ptable.list[i].head)
        panic("Runnable list should be empty");
      for(int j = MAXPRIO; j >= 0; --j) {
        p = ptable.ready[j].head;
        if(!p)
          continue;
        while(p) {
          if(p->pid == pid) {
            p->killed = 1;
            release(&ptable.lock);
            return 0;
          }
          p = p->next;
        }
      }
      continue;
    }
#endif
    p = ptable.list[i].head;
    if(!p)
      continue;
    while(p)
    {
      //temp = p->next;
      if(p->pid == pid){
        p->killed = 1;
        //Wake process from sleep if necessary.
        if(p->state == SLEEPING)
        {
          if(stateListRemove(&ptable.list[p->state], p) == -1)
            panic("Empty list or process not in list");
          assertState(p, SLEEPING);
          p->state = RUNNABLE;
#ifdef CS333_P4
          stateListAdd(&ptable.ready[p->priority], p);

#else
          stateListAdd(&ptable.list[p->state], p);
#endif
        }
        release(&ptable.lock);
        return 0;
      }
      p = p->next;
    }
  }
  release(&ptable.lock);
  return -1;
}

#else
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}
#endif

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.

#if defined(CS333_P4)
static void
procdumpP4(struct proc *p, char *state)
{
  int time_integer = (ticks - p->start_ticks)/1000;
  int time_remainder = (ticks - p->start_ticks)%1000;
  int cpu_integer = p->cpu_ticks_total/1000;
  int cpu_remainder = p->cpu_ticks_total%1000;

  cprintf("%d\t",p->pid);
  // check strlen of process name to prettify the printed columns.
  if(strlen(p->name) < 8)
    cprintf("%s\t\t%d\t%d\t",p->name, p->uid, p->gid);
  if(strlen(p->name) >= 8)
    cprintf("%s\t%d\t%d\t",p->name, p->uid, p->gid);
  if(!p->parent)
    cprintf("%d\t", p->pid);
  else
    cprintf("%d\t", p->parent->pid);

  cprintf("%d\t", p->priority);

  if(time_remainder == 0)
    cprintf("%d\t\t", time_integer);
  else if(time_remainder < 10)
    cprintf("%d.00%d\t\t", time_integer, time_remainder);
  else if(time_remainder < 100)
    cprintf("%d.0%d\t\t", time_integer, time_remainder);
  else if(time_remainder < 1000)
    cprintf("%d.%d\t\t", time_integer, time_remainder);

  if(cpu_remainder == 0)
    cprintf("%d\t", cpu_integer);
  else if(cpu_remainder < 10)
    cprintf("%d.00%d\t", cpu_integer, cpu_remainder);
  else if(cpu_remainder < 100)
    cprintf("%d.0%d\t", cpu_integer, cpu_remainder);
  else if(cpu_remainder < 1000)
    cprintf("%d.%d\t", cpu_integer, cpu_remainder);

  cprintf("%s\t%d\t", state, p->sz);
}


#elif defined(CS333_P2)
static void
procdumpP2(struct proc *p, char *state)
{
  int time_integer = (ticks - p->start_ticks)/1000;
  int time_remainder = (ticks - p->start_ticks)%1000;
  int cpu_integer = p->cpu_ticks_total/1000;
  int cpu_remainder = p->cpu_ticks_total%1000;

  cprintf("%d\t",p->pid);
  // check strlen of process name to prettify the printed columns.
  if(strlen(p->name) < 8)
    cprintf("%s\t\t%d\t%d\t",p->name, p->uid, p->gid);
  if(strlen(p->name) >= 8)
    cprintf("%s\t%d\t%d\t",p->name, p->uid, p->gid);
  if(!p->parent)
    cprintf("%d\t", p->pid);
  else
    cprintf("%d\t", p->parent->pid);

  if(time_remainder == 0)
    cprintf("%d\t\t", time_integer);
  else if(time_remainder < 10)
    cprintf("%d.00%d\t\t", time_integer, time_remainder);
  else if(time_remainder < 100)
    cprintf("%d.0%d\t\t", time_integer, time_remainder);
  else if(time_remainder < 1000)
    cprintf("%d.%d\t\t", time_integer, time_remainder);

  if(cpu_remainder == 0)
    cprintf("%d\t", cpu_integer);
  else if(cpu_remainder < 10)
    cprintf("%d.00%d\t", cpu_integer, cpu_remainder);
  else if(cpu_remainder < 100)
    cprintf("%d.0%d\t", cpu_integer, cpu_remainder);
  else if(cpu_remainder < 1000)
    cprintf("%d.%d\t", cpu_integer, cpu_remainder);

  cprintf("%s\t%d\t", state, p->sz);
}

#elif defined(CS333_P1)
static void
procdumpP1(struct proc *p, char *state)
{
  int time_integer = (ticks - p->start_ticks)/1000;
  int time_remainder = (ticks - p->start_ticks)%1000;

  cprintf("%d\t",p->pid);
  if(strlen(p->name) < 8)
    cprintf("%s\t\t",p->name);
  if(strlen(p->name) >= 8)
    cprintf("%s\t", p->name);

  if(time_remainder == 0)
    cprintf("%d\t\t", time_integer);
  else if(time_remainder < 10)
    cprintf("%d.00%d\t\t", time_integer, time_remainder);
  else if(time_remainder < 100)
    cprintf("%d.0%d\t\t", time_integer, time_remainder);
  else if(time_remainder < 1000)
    cprintf("%d.%d\t\t", time_integer, time_remainder);


  cprintf("%s\t%d\t", state, p->sz);
}
#endif


void
procdump(void)
{
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

#if defined(CS333_P4)
#define HEADER "\nPID\tName\t\tUID\tGID\tPPID\tPrio\tElapsed\tCPU\tState\tSize\t PCs\n"
#elif defined(CS333_P2)
#define HEADER "\nPID\tName\t\tUID\tGID\tPPID\tElapsed\t\tCPU\tState\tSize\t PCs\n"
#elif defined(CS333_P1)
#define HEADER "\nPID\tName\t\tElapsed\tState\tSize\t PCs\n"
#else
#define HEADER "\n"
#endif

  cprintf(HEADER);

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;

    if(p->state > UNUSED && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";

#if defined(CS333_P4)
    procdumpP4(p, state);
#elif defined(CS333_P2)
    procdumpP2(p, state);
#elif defined(CS333_P1)
    procdumpP1(p, state);
#else
    cprintf("%d\t%s\t%s\t", p->pid, p->name, state);
#endif
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
  release(&ptable.lock);
}

#ifdef CS333_P2
int
getprocs(uint max, struct uproc* table)
{
  int counter = 0;
  struct proc *p;
  char* state;
  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
  {
    if(p->state == UNUSED || p->state ==  EMBRYO)
    {
      continue;
    }
    if(counter < max)
    {
      if(p->state > EMBRYO && p->state < NELEM(states) && states[p->state])
        state = states[p->state];
      else
        state = "???";

      table[counter].pid = p->pid;
      table[counter].uid = p->uid;
      table[counter].gid = p->gid;
      if(!p->parent)
      {
        table[counter].ppid = p->pid;
      }
      else
      {
        table[counter].ppid = p->parent->pid;
      }
#ifdef CS333_P4
      table[counter].priority = p->priority;
#endif
      table[counter].elapsed_ticks = ticks - p->start_ticks;
      table[counter].CPU_total_ticks = p->cpu_ticks_total;
      safestrcpy(table[counter].state, state, sizeof(table[counter].state));
      table[counter].size = p->sz;
      safestrcpy(table[counter].name, p->name, sizeof(table[counter].name));
      ++counter;
    }
    else
    {
      break;
    }
  }
  release(&ptable.lock);
  return counter;
}
#endif

#ifdef CS333_P3
// list management helper functions
static void
initFreeList(void)
{
  struct proc* p;

  for(p = ptable.proc; p < ptable.proc + NPROC; ++p){
    p->state = UNUSED;
    stateListAdd(&ptable.list[UNUSED], p);
  }
}

static void
initProcessLists()
{
  int i;

  for (i = UNUSED; i <= ZOMBIE; i++) {
    ptable.list[i].head = NULL;
    ptable.list[i].tail = NULL;
  }
#ifdef CS333_P4
  for (i = 0; i <= MAXPRIO; i++) {
    ptable.ready[i].head = NULL;
    ptable.ready[i].tail = NULL;
  }
#endif
}

static void
stateListAdd(struct ptrs* list, struct proc* p)
{
  if((*list).head == NULL){
    (*list).head = p;
    (*list).tail = p;
    p->next = NULL;
  } else{
    ((*list).tail)->next = p;
    (*list).tail = ((*list).tail)->next;
    ((*list).tail)->next = NULL;
  }
}

static int
stateListRemove(struct ptrs* list, struct proc* p)
{
  if((*list).head == NULL || (*list).tail == NULL || p == NULL){
    return -1;
  }

  struct proc* current = (*list).head;
  struct proc* previous = 0;

  if(current == p){
    (*list).head = ((*list).head)->next;
    // prevent tail remaining assigned when we've removed the only item
    // on the list
    if((*list).tail == p){
      (*list).tail = NULL;
    }
    return 0;
  }

  while(current){
    if(current == p){
      break;
    }

    previous = current;
    current = current->next;
  }

  // Process not found. return error
  if(current == NULL){
    return -1;
  }

  // Process found.
  if(current == (*list).tail){
    (*list).tail = previous;
    ((*list).tail)->next = NULL;
  } else{
    previous->next = current->next;
  }

  // Make sure p->next doesn't point into the list.
  p->next = NULL;

  return 0;
}

static void
assertState(struct proc* p, enum procstate state)
{
  if(p->state != state)
    panic("process state not same as asserted state.");
}

void
getSleepList(void)
{
  struct proc* p;
  acquire(&ptable.lock);
  p = ptable.list[SLEEPING].head;
  if(!p){
    cprintf("No Processes Sleepin\n");
    release(&ptable.lock);
    return;
  }
  cprintf("Sleep List Processes: \n");
  while(p)
  {
    if(!p->next)
      cprintf("%d\n", p->pid);
    else
      cprintf("%d -> ", p->pid);
    p = p->next;
  }
  release(&ptable.lock);
}

void
getFreeList(void)
{
  int count = 0;
  struct proc* p;
  acquire(&ptable.lock);
  p = ptable.list[UNUSED].head;
  while(p)
  {
    ++count;
    p = p->next;
  }
  cprintf("Free List Size: %d processes\n", count);
  release(&ptable.lock);
}


void
getZombieList(void)
{
  struct proc* p;
  acquire(&ptable.lock);
  p = ptable.list[ZOMBIE].head;
  if(!p){
    cprintf("No Processes in ZOMBIE\n");
    release(&ptable.lock);
    return;
  }
  cprintf("Zombie List Processes: \n");
  while(p)
  {
    if(!p->next)
      cprintf("(%d, %d)\n", p->pid, p->parent->pid);
    else
      cprintf("(%d, %d) -> ", p->pid, p->parent->pid);
    p = p->next;
  }
  release(&ptable.lock);
}
#endif

#ifdef CS333_P4
void
getReadyList(void)
{
  int counter = 0;
  acquire(&ptable.lock);
  struct proc* p;
  for(int i = MAXPRIO; i >= 0; --i) {
    p = ptable.ready[i].head;
    if(i == MAXPRIO)
      cprintf("MAXPRIO: ");
    else if(i > 0)
      cprintf("MAXPRIO-%d: ", counter);
    else
      cprintf("0: ");
    if(!p) {
      cprintf("none. \n\n");
      ++counter;
      continue;
    }
    while(p)
    {
      if(!p->next)
        cprintf("(%d, %d)\n\n", p->pid, p->budget);
      else
        cprintf("(%d, %d) -> ", p->pid, p->budget);
      p = p->next;
    }
    ++counter;
  }
  release(&ptable.lock);
}

#elif CS333_P3
void
getReadyList(void)
{
  acquire(&ptable.lock);
  struct proc* p;
  p = ptable.list[RUNNABLE].head;
  if(!p){
    cprintf("No Processes in RUNNABLE\n");
    release(&ptable.lock);
    return;
  }
  cprintf("RUNNABLE List Processes: \n");
  while(p)
  {
    if(!p->next)
      cprintf("%d\n", p->pid);
    else
      cprintf("%d -> ", p->pid);
    p = p->next;
  }
  release(&ptable.lock);
}
#endif

#ifdef CS333_P4
int
setpriority(int pid, int priority)
{
  struct proc* p;
  acquire(&ptable.lock);
  if(pid < 0 || pid > 32767) {
    release(&ptable.lock);
    return RETURN_FAILURE;
  }
  if(priority < 0 || priority > MAXPRIO) {
    release(&ptable.lock);
    return RETURN_FAILURE;
  }

  for(int i = SLEEPING; i <= RUNNING; ++i) {
    if(i == RUNNABLE) {
      if(ptable.list[i].head)
        panic("Runnable list should be empty");
      for(int j = MAXPRIO; j >= 0; --j) {
        p = ptable.ready[j].head;
        while(p) {
          if(p->pid == pid) {
            if(p->priority != priority) {
              if(stateListRemove(&ptable.ready[p->priority], p) == -1)
                panic("Empty list or process not in list");
              assertState(p, RUNNABLE);
              if(p->priority != j)
                panic("p->priority not same as in ready list");
              p->priority = priority;
              p->budget = BUDGET;
              stateListAdd(&ptable.ready[p->priority], p);
            }
            release(&ptable.lock);
            return RETURN_SUCCESS;
          }
          p = p->next;
        }
      }
      continue;
    }
    p = ptable.list[i].head;
    while(p) {
      if(p->pid == pid) {
        if(p->priority != priority) {
          p->priority = priority;
          p->budget = BUDGET;
        }
        release(&ptable.lock);
        return RETURN_SUCCESS;
      }
      p = p->next;
    }
  }
  release(&ptable.lock);
  return RETURN_FAILURE;
}

int
getpriority(int pid)
{
  struct proc* p;
  acquire(&ptable.lock);
  if(pid < 0 || pid > 32767) {
    release(&ptable.lock);
    return RETURN_FAILURE;
  }
  for(int i = SLEEPING; i <= ZOMBIE; ++i) {
    if(i == RUNNABLE) {
      if(ptable.list[i].head)
        panic("Runnable list should be empty");
      for(int j = MAXPRIO; j >= 0; --j) {
        p = ptable.ready[j].head;
        while(p) {
          if(p->pid == pid) {
            release(&ptable.lock);
            return p->priority;
          }
          p = p->next;
        }
      }
      continue;
    }
    p = ptable.list[i].head;
    while(p) {
      if(p->pid == pid) {
        release(&ptable.lock);
        return p->priority;
      }
      p = p->next;
    }
  }
  release(&ptable.lock);
  return RETURN_FAILURE;
}
#endif




