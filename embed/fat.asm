[bits 16]
[org 0x7C00]
[cpu 8086]

entry:
    jmp short start
    nop

fat_oem: times 8 db 0
fat_bps dw 0
fat_spc db 0
fat_rs dw 0
fat_fc db 0
fat_rdec dw 0
fat_ts dw 0
fat_md db 0
fat_fs dw 0
fat_spt dw 0
fat_noh dw 0
fat_hd dd 0
fat_lts dd 0

fat_fs32 dd 0
fat_ef dw 0
fat_fv dw 0
fat_rc dd 0
fat_fis dw 0
fat_bb dw 0
times 12 db 0

fat_dn db 0

db 0
fat_bs db 0

fat_vi dd 0

fat_vl: times 11 db 0
fat_ft: times 8 db 0

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
    lidt [cs:.fallback + 1]
    int 3

[cpu 8086]
.fallback:
    jmp 0xFFFF:0x0000



not_bootable_msg db "This is not a bootable disk or partition.", 0
press_enter_to_restart_msg db 0x0D, 0x0A, "Press any key to restart... ", 0x0D, 0x0A, 0

times 510 - ($ - $$) db 0

dw 0xAA55
