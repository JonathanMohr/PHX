[bits 16]
[org 0x7C00]
[cpu 8086]

; TODO: Actually load partition

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

    mov si, not_bootable_msg
    jmp short print_error


print_char:
    int 10h
print_raw:
    mov ah, 0eh
    mov bx, 0x07
    lodsb
    test al, al
    jnz short print_char
    ret

print_error:
    call print_raw
    mov si, press_enter_to_restart_msg
    call print_raw

    cbw
    int 16h
    ; jmp short restart


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

not_bootable_msg db "This is not a legacy bootable GPT disk.", 0
press_enter_to_restart_msg db 0x0D, 0x0A, "Press any key to restart... ", 0x0D, 0x0A, 0

times 440 - ($ - $$) db 0

disk_signature dd 0
disk_reserved dw 0

partition1:
    times 16 db 0
partition2:
    times 16 db 0
partition3:
    times 16 db 0
partition4:
    times 16 db 0



dw 0xAA55
