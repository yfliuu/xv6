#include "defs.h"
#include "crosscall.h"
#include "mmu.h"
#include "syscall.h"
#include "proc.h"


struct proc *_proc_cc;
char *shbuf;

void
crosscall_init()
{
#if CCALL_CALLEE
    struct shared_info *info = (struct shared_info *)SHARED_MEM_ADDR;
    extern void *idt_global;

    memset(info, 0, sizeof(struct shared_info));
    info->stub = ccall_stub_init();
    info->callee_idt = idt_global;
    info->callee_idt_size = PGSIZE;

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
    uint64 *ptrp;
    int len;
    ptrp = fetcharg_addr(tf, ptr_id);
    len = strlen((char *)*ptrp);
    memmove((void *)sdst, (void *)*ptrp, len);
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
    int i;

    info->caller_idt = idt_global;
    info->caller_idt_size = PGSIZE;

    // TODO: Probably not going to be useful since we disable interrupts
    // lidt(info->callee_idt, info->callee_idt_size);

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
            reloc_read(&info->tf, (uint64)SHARED_MEM_SHBUF, SHARED_MEM_SHBUF_SIZE, 1, 2);
            break;
        }
    }

    set_ccall_state();

    return info->params.callee_id;
}

// This function should only be executed in callee
// Return source (caller) VM id to be used by crosscall assembly routine
int 
ccall_post(void)
{
    struct shared_info *info = (struct shared_info *)SHARED_MEM_ADDR;
    struct trapframe _bak;
    int ret;

    _proc_cc = info->stub;

    // Save original process trapframe
    memmove(&_bak, _proc_cc->tf, sizeof(struct trapframe));

    // Change to caller process trapframe
    memmove(_proc_cc->tf, &info->tf, sizeof(struct trapframe));

    // Call actual system call
    // The _proc_cc->tf must be adjusted to pop the callee_id and syscall_id
    ret = syscall_api(info->params.syscall_id);
    info->params.ret = ret;

    memmove(&_proc_cc->tf, &_bak, sizeof(struct trapframe));

    return info->params.caller_id;
}

// This function should only be executed in caller
// Return the return value of the executed system call
int
ccall_ret(void)
{
    struct shared_info *info = (struct shared_info *)SHARED_MEM_ADDR;

    clr_ccall_state();

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
