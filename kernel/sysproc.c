#include "types.h"
#include "x86.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "crosscall.h"

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return proc()->pid;
}

uintp
sys_sbrk(void)
{
  uintp addr;
  uintp n;

  if(arguintp(0, &n) < 0)
    return -1;
  addr = proc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;
  
  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(proc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

int
sys_pwoff(void)
{
  outb(0xf4, 0);
  outw(0x604, 0x0 | 0x2000);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;
  
  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

int
sys_ccall(void)
{
  struct shared_info *smem = (struct shared_info *)SHARED_MEM_ADDR;
  int ret; 
  int callee_id, syscall_id;

  argint(0, &callee_id);
  argint(1, &syscall_id);

  // TODO: Access control && Buffer change

  // Wait for callee ready
  while (smem->magic != SHARED_MEM_MAGIC)
    ;

  smem->params.callee_id = callee_id;
  smem->params.syscall_id = syscall_id;

  memmove(&smem->otf, proc()->tf, sizeof(struct trapframe));

  ret = crosscall();

  return ret;
}
