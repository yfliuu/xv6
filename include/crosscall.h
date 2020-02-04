#ifndef _CROSSCALL_H_
#define _CROSSCALL_H_

#include "types.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"

#define CCALL_STATE_VAL     0x4
#define SHARED_MEM_ADDR     (P2V(0x200000))
#define SHARED_MEM_SHBUF    (SHARED_MEM_ADDR + 0x1000)
#define SHARED_MEM_SHBUF_SIZE    0x2000
#define SHARED_MEM_STACK    (SHARED_MEM_ADDR + 0x4000)
#define SHARED_MEM_MAGIC    0xBADBEEF

struct callparams {
  int caller_id;
  int callee_id;
  int syscall_id;
  int args[8];
  int ret;
};

// This is the struct that will be saved
// into shared memory. 
struct shared_info {
  int magic;                    // Written by callee to inform the caller that callee is ready
  void *caller_idt;
  uint64 caller_idt_size;
  void *callee_idt;
  uint64 callee_idt_size;
  uint64 saved_cr3;
  uint64 saved_stack;
  struct proc *stub;            // Ptr to stub PCB. Set by callee during initialization
  struct callparams params;     // Calling parameters
  struct trapframe otf;         // Original trapframe
  struct trapframe tf;          // Buffer adjusted trapframe
  struct cpu _cpu;              // We can't share other's cpu struct
};

void set_ccall_state(void);
void clr_ccall_state(void);
int is_ccall_state(void);

extern struct proc *_proc_cc;
extern struct cpu *_cpu_cc;
#endif