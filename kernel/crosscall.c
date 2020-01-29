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
            uint64 *bufp, *szp;

            bufp = fetcharg_addr(&info->tf, 1);
            szp = fetcharg_addr(&info->tf, 2);

            if (*szp > SHARED_MEM_SHBUF_SIZE) {
                // Send out warning, we can't return now
                *szp = SHARED_MEM_SHBUF_SIZE;
            }

            // Read: point te buffer to shared memory buffer
            *bufp = (uint64)SHARED_MEM_SHBUF;
            break;
        }
        case SYS_write: {
            uint64 *bufp, *szp;
            char *smem = (char *)SHARED_MEM_SHBUF;

            bufp = fetcharg_addr(&info->tf, 1);
            szp = fetcharg_addr(&info->tf, 2);

            if (*szp > SHARED_MEM_SHBUF_SIZE) {
                // Send out warning, we can't return now
                *szp = SHARED_MEM_SHBUF_SIZE;
            }

            // Write: copy buffer to shared memory
            memmove(smem, bufp, *szp); 
            break;
        }
        default: {
            // Print error messages
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
            uint64 *bufp, *szp, *ubufp;

            bufp = fetcharg_addr(&info->tf, 1);
            ubufp = fetcharg_addr(&info->otf, 1);
            szp = fetcharg_addr(&info->tf, 2);

            if (*szp > SHARED_MEM_SHBUF_SIZE) {
                // Send out warning, we can't return now
                *szp = SHARED_MEM_SHBUF_SIZE;
            }

            memmove(ubufp, bufp, *szp);
            break;
        }
        case SYS_write: {
        }
        default: {
            // Print error messages
        }
    }

    return info->params.ret;
}

// Check if we're the owner, or the "trespasser"
int
is_ccall_state(void) {
  return read_gs() == (uint64)CCALL_STATE_VAL;
}
