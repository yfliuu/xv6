#include "defs.h"
#include "crosscall.h"
#include "mmu.h"
#include "syscall.h"
#include "proc.h"


struct proc *_proc_cc;
struct cpu *_cpu_cc;
char *shbuf;

static inline uint64
rcr3() 
{
    uint64 val;
    asm volatile("mov %%cr3,%0" : "=r" (val));
    return val;
}

void
save_stack(uint64 val)
{
    struct shared_info *info = (struct shared_info *)SHARED_MEM_ADDR;
    info->saved_stack = val;
}

uint64 
get_saved_stack()
{
    struct shared_info *info = (struct shared_info *)SHARED_MEM_ADDR;
    return info->saved_stack;
}

void
crosscall_early_init()
{
    clr_ccall_state();
}

void
crosscall_init()
{
    struct shared_info *info = (struct shared_info *)SHARED_MEM_ADDR;
    extern void *idt_global;
    extern char shared_memory_start[];

    cprintf("shared memory start from ld: 0x%x\n", shared_memory_start);
    cprintf("shared memory start: 0x%x\n", SHARED_MEM_ADDR);

#if CCALL_CALLEE
    cprintf("crosscall init\n");

    memset(info, 0, sizeof(struct shared_info));
    info->stub = ccall_stub_init();
    info->callee_idt = idt_global;
    info->callee_idt_size = PGSIZE;
    info->params.callee_id = 1;
    // TODO: Incompatible pointer types
    info->_cpu.scheduler = scheduler;
    cprintf("Callee idt: %x\n", info->callee_idt);
    cprintf("cpu: %x, cpu->ncli: %x\n", cpu(), cpu()->ncli);

    // Prepare shared memory allocation
    shbuf = (char *)SHARED_MEM_SHBUF;
    memset((void *)shbuf, 0, SHARED_MEM_SHBUF_SIZE);

    // Inform the caller we're ready
    info->magic = SHARED_MEM_MAGIC;
#endif
}

static uint64 *
fetcharg_addr(struct trapframe *tf, int n) {
    switch (n) {
        case 0: return &tf->rdi;
        case 1: return &tf->rsi;
        case 2: return &tf->rdx;
        case 3: return &tf->rcx;
        case 4: return &tf->r8;
        case 5: return &tf->r9;
    }
    return (uint64 *)0x0;
}

// Repoint const char string
void reloc_const_char(struct trapframe *tf, uint64 sdst, int ptr_id)
{
    int len;
    char *str;
    str = (char *)*fetcharg_addr(tf, ptr_id);
    len = strlen(str);
    strncpy((char *)sdst, str, len + 1);
    *fetcharg_addr(tf, ptr_id) = sdst;
}

// Repoint read buffers to shared memory, i.e., buffer passed to syscalls and filled
// Need to be used with writeback
void reloc_read(struct trapframe *tf, uint64 sdst, uint ssz, int buf_id, int sz_id)
{
    uint64 *bufp, *szp;
    bufp = fetcharg_addr(tf, buf_id);
    szp = fetcharg_addr(tf, sz_id);
    if (*szp > ssz) {
        // Send out warning
        *szp = ssz;
    }
    *bufp = sdst;
}

// Write back what syscalls written in a read buffer
void reloc_writeback(struct trapframe *otf, struct trapframe *tf, int buf_id, int sz_id)
{
    uint64 *bufp, *obufp, *szp;
    obufp = fetcharg_addr(otf, buf_id);
    bufp = fetcharg_addr(tf, buf_id);
    szp = fetcharg_addr(tf, sz_id);
    memmove((void *)*obufp, (void *)*bufp, *szp);
}

// Repoint the write buffer
void reloc_write(struct trapframe *tf, uint64 sdst, uint ssz, int buf_id, int sz_id)
{
    uint64 *bufp, *szp;
    bufp = fetcharg_addr(tf, buf_id);
    szp = fetcharg_addr(tf, sz_id);
    if (*szp > ssz) {
        // Send out warning
        *szp = ssz;
    }
    memmove((void *)sdst, (void *)*bufp, *szp);
    *bufp = sdst;
}


// Prepare cross call: only executed in caller
// Return target (callee) VM id to be used by crosscall assembly routine
int
ccall_pre(void)
{
    struct shared_info *info = (struct shared_info *)SHARED_MEM_ADDR;
    extern void *idt_global;
    extern char spml4_root[];
    void *callee_scheduler;
    int i;

    info->caller_idt = idt_global;
    info->caller_idt_size = PGSIZE;
    info->params.caller_id = 2;

    // Copy cpu structure, keep the scheduler as is
    // the _cpu.scheduler is initialized by callee
    callee_scheduler = info->_cpu.scheduler;
    memmove(&info->_cpu, cpu(), sizeof(struct cpu));
    info->_cpu.scheduler = callee_scheduler;
    // TODO: Allocate a page of local storage

    lidt(info->callee_idt, info->callee_idt_size);

    // Adjust the trapframe to pop the callee_id and syscall_id
    for (i = 0; i < 4; i++)
        *fetcharg_addr(&info->otf, i) = *fetcharg_addr(&info->otf, i + 2);

    memmove(&info->tf, &info->otf, sizeof(struct trapframe));

    // Buffer replacing
    switch(info->params.syscall_id) {
        case SYS_read: {
            reloc_read(&info->tf, (uint64)SHARED_MEM_SHBUF, SHARED_MEM_SHBUF_SIZE, 1, 2);
            break;
        }
        case SYS_write: {
            reloc_write(&info->tf, (uint64)SHARED_MEM_SHBUF, SHARED_MEM_SHBUF_SIZE, 1, 2); 
            break;
        }
        case SYS_open: {
            reloc_const_char(&info->tf, (uint64)SHARED_MEM_SHBUF, 0);
            break;
        }
    }

    info->saved_cr3 = rcr3();
    lcr3(v2p(spml4_root));

    set_ccall_state();
    return info->params.callee_id;
}

// This function should only be executed in callee
// Return source (caller) VM id to be used by crosscall assembly routine
int 
ccall_post(uint64 old_rsp)
{
    struct shared_info *info = (struct shared_info *)SHARED_MEM_ADDR;
    struct trapframe *_bak;
    int ret;
    uintp _sz;

    info->saved_stack = old_rsp;

    _proc_cc = info->stub;
    _cpu_cc = &info->_cpu;

    // Need to set this to high value otherwise
    // fetch args methods will reject the shared
    // memory buffer
    _sz = _proc_cc->sz;
    _proc_cc->sz = P2V(PHYSTOP);

    // Save original process trapframe
    _bak = _proc_cc->tf;

    // Change to caller process trapframe
    _proc_cc->tf = &info->tf;

    // Call actual system call
    ret = syscall_api(info->params.syscall_id);
    info->params.ret = ret;

    _proc_cc->tf = _bak;
    _proc_cc->sz = _sz;

    return info->params.caller_id;
}

// This function should only be executed in caller
// Return the return value of the executed system call
int
ccall_ret(void)
{
    struct shared_info *info = (struct shared_info *)SHARED_MEM_ADDR;
    clr_ccall_state();

    lidt(info->caller_idt, info->caller_idt_size);


    // Reload cr3
    lcr3(info->saved_cr3);

    // Buffer restore
    switch(info->params.syscall_id) {
        case SYS_read: {
            reloc_writeback(&info->otf, &info->tf, 1, 2);
            break;
        }
    }

    return info->params.ret;
}

static inline uint64
read_dr0(void) {
  uint64 val;
  asm volatile("mov %%dr0,%0" : "=r" (val));
  return val;
}

static inline void
write_dr0(uint64 val) {
  asm volatile("mov %0,%%dr0" : : "r" (val));
}

void
set_ccall_state(void) {
  write_dr0(CCALL_STATE_VAL);
}

void
clr_ccall_state(void) {
  write_dr0(0x0);
}
// Check if we're the owner, or the "trespasser"
int
is_ccall_state(void) {
  return read_dr0() == (uint64)CCALL_STATE_VAL;
}
