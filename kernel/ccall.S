.section .text.cross
.globl crosscall
crosscall:
    push %rbp
    mov %rsp, %rbp

    cli

    # ccall_pre will return target VM id
    call ccall_pre

    # Save context
    pushf
    push %r15
    push %r14
    push %r13
    push %r12
    push %r11
    push %r10
    push %r9 
    push %r8 
    push %rdi
    push %rsi
    push %rbp
    push %rdx
    push %rcx
    push %rbx
    push %rax

    # Switch stack to shared stack
    # rdi will be saved to shared memory by ccall_post
    mov %rsp, %rdi
    mov $0xffffffff80240000, %rsp

    # Flush TLB
    mov %cr3, %ebx
    mov %ebx, %cr3

    # Flush cache
    wbinvd

    mov %eax, %ecx
    mov $0, %eax
    vmfunc

    # Now in another universe
.globl out_of_universe
out_of_universe:

    call ccall_post

    # Flush TLB
    mov %cr3, %rbx
    mov %rbx, %cr3
    wbinvd

    # ccall_post will return caller VM id
    mov %eax, %ecx
    mov $0, %eax
    vmfunc

    # Now back to our universe

    # Flush TLB and cache again
    mov %cr3, %rbx
    mov %rbx, %cr3
    wbinvd

    # Switch stack back
    call get_saved_stack
    mov %rax, %rsp

    # Restore context
    pop %rax
    pop %rbx
    pop %rcx
    pop %rdx
    pop %rbp
    pop %rsi
    pop %rdi
    pop %r8 
    pop %r9 
    pop %r10
    pop %r11
    pop %r12
    pop %r13
    pop %r14
    pop %r15 
    popf

    call ccall_ret

    sti

    # return value same as ccall_ret
    pop %rbp 
    ret
