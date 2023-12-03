
global write_out

section .text

; rdx arg3 len
; rsi arg2 str
; rdi arg1 stream
; write_out(int stream, char* str, int len)
write_out:
    push r11 ; gets clobbered
    push rcx
    sub rsp, 8 ; align stack to 16 bytes

    mov rax, 1
    syscall

    add rsp, 8
    pop rcx
    pop r11
    ret

section .note.GNU-stack