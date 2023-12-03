
global brk_syscall
; without 'global' symbol declaration the linker doesn't
; see this and will not be able to find this.

section .text

; set RDI (first arg)
brk_syscall:
    push r11 ; gets clobbered
    push rcx
    sub rsp, 8 ; align stack to 16 bytes
    
    ; rdi is already correctly set
    ; as the argument of this function
    mov rax, 12 ; brk syscall
    syscall

    add rsp, 8
    pop rcx
    pop r11
    ret

section .note.GNU-stack