[bits 16]
[org 0x0600]
[cpu 8086]

; TODO: Actually load partition

entry:
    cli

    xor ax, ax

    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    mov si, sp
    mov di, 0x0600
    mov cx, 512
    cld
    rep movsb

    jmp 0x0000:at_0600

at_0600:
    mov [boot_drive], dl
    sti

partition_search:

check_disk:
    ; check lba
    mov ah, 0x41
    mov bx, 0x55AA
    ; dl is still set
    int 0x13
    jc short .no_lba
    cmp bx, 0xAA55
    jne short .no_lba

    test cl, 1
    jz short .no_lba

.use_lba:
    mov si, drive_params
    mov word [si], 0x1A
    mov ah, 0x48
    mov dl, [boot_drive]
    int 0x13
    mov ax, 512
    jc short .store_lba
    mov bx, [drive_params + 0x18]
    cmp bx, 512
    jb short .store_lba
    mov ax, bx
.store_lba:
    mov word [bytes_per_sector], ax
    mov byte [lba], 1
    jmp short .after

.no_lba:
    mov word [bytes_per_sector], 512
    mov byte [lba], 0

.after:
    ; TODO

    jmp short disk_error



;
; 64-bit LBA on stack
;

%define RETRIES 3
read_sectors:
    pop word [read_return_address]
    cmp byte [lba], 1
    jz short read_lba

.chs:
    push es
    xor di, di
    mov es, di
    mov ah, 0x08
    mov dl, [boot_drive]
    int 0x13
    pop es
    mov si, sp
    jc short disk_error

    mov ax, [si + 4]
    or ax, [si + 6]
    jnz short lba_too_high_error

    and cx, 0x3F
    jz short disk_error
    inc dh
    jz short disk_error
    mov bx, cx
    mov cl, dh
    mov di, cx

    mov ax, [si + 2]
    xor dx, dx
    div bx
    xchg ax, cx
    mov ax, [si]
    div bx
    inc dx
    push dx

    cmp cx, di
    jae short lba_too_high_error
    mov dx, cx
    div di
    cmp ax, 1023
    ja short lba_too_high_error

    mov dh, dl
    mov dl, [boot_drive]
    pop cx
    mov ch, al
    ror ah, 1
    ror ah, 1
    or cl, ah

    mov bx, 0x7C00
    mov ax, 0x0201
    call read_int13_retry
    jmp word [read_return_address]

lba_too_high_error:
    mov si, lba_too_high_error_msg
    jmp short print_error
    
disk_error:
    mov si, disk_error_msg
    jmp short print_error

read_lba:
    xor ax, ax
    
    push ax ; segment

    mov bx, 0x7C00
    push bx ; offset

    inc ax
    push ax ; sector count

    mov ax, 0x0010
    push ax

    mov si, sp
    mov ah, 0x42
    call read_int13_retry
    add sp, 8
    jmp word [read_return_address]

read_int13_retry:
    mov di, RETRIES
.again:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    int 0x13
    jnc short read_ok
    mov dl, [boot_drive]
    xor ah, ah
    int 0x13
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    dec di
    jnz short .again

    ; jmp short disk_error
    mov si, disk_error_msg

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

read_ok:
    add sp, 10
    ret

print_char:
    int 10h
print_raw:
    mov ah, 0eh
    mov bx, 0x07
    lodsb
    test al, al
    jnz short print_char
    ret

disk_error_msg db "Disk error", 0
lba_too_high_error_msg db "LBA too high", 0
press_enter_to_restart_msg db '!', 0x0D, 0x0A, "Press any key to restart... ", 0x0D, 0x0A, 0

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


; byte
lba equ 0x900

; word
read_return_address equ 0x900
bytes_per_sector equ 0x902
boot_drive equ 0x904

; other
drive_params equ 0xA00  ; 26 byte
