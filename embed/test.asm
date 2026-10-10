[bits 16]
[org 0x7C00]
[cpu 8086]

start:
    cli

    xor ax, ax

    mov ds, ax
    mov es, ax

    mov ss, ax
    mov sp, 0x7C00

    xor di, di

    jmp 0x0000:.after

.after:
    sti

    mov si, msg
    jmp short print_raw

    cbw
    int 16h
    jmp short restart


print_char:
    int 10h
print_raw:
    mov ah, 0eh
    mov bx, 0x07
    lodsb
    test al, al
    jnz short print_char
    ret


restart:
    cli
    xor cx, cx

.wait:
    in al, 0x64
    test al, 2
    loopnz .wait

    mov al, 0xFE
    out 0x64, al

    xor cx, cx
.delay:
    in al, 0x64
    loop .delay

    pushf
    pop ax
    test ah, ah
    js .fallback
[cpu 286]
    lidt [triple_fault_idt]
    int 3

[cpu 8086]
.fallback:
    jmp 0xFFFF:0x0000


triple_fault_idt:
    times 6 db 0

msg db "If you see this, you have booted from a test disk hex file for PHX", 0x0D, 0x0A, "Press any key to restart... ", 0x0D, 0x0A, 0

times 510 - ($ - $$) db 0

dw 0xAA55
