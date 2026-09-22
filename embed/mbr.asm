[bits 16]
[org 0x0600]
[cpu 8086]

%define RETRIES 3

;
; Reusing code that will not be used again after initializing as space for variable
;

entry:

boot_drive: ; 1 byte
    cli

partition: ; 2 byte
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

init_check_loop:
    mov si, partition1
    mov cx, 4

.check_loop:
    cmp byte [si], 0x80
    jz short partition_found

    add si, 16
    loop .check_loop

    jmp near no_partition_found

partition_found:
    mov [partition], si

    ; check lba
    mov ah, 0x41
    mov bx, 0x55AA
    ; dl is still set
    int 0x13
    mov si, [partition] ; set si for use_lba / no_lba
    jc short .no_lba
    cmp bx, 0xAA55
    jne short .no_lba

    test cl, 1
    jz short .no_lba

.use_lba:
    xor ax, ax
    push ax
    push ax
    push word [si + 10]
    push word [si + 8]
    push ax
    mov bx, 0x7C00
    push bx
    inc ax
    push ax
    mov al, 0x10
    push ax

    mov cx, RETRIES

.lba_retry:
    mov si, sp
    mov byte [si + 2], 1
    push cx
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jnc short .lba_success

    xor ah, ah
    mov dl, [boot_drive]
    int 0x13
    pop cx
    loop .lba_retry

    ; jmp short .disk_error_short

.disk_error_short:
    jmp near disk_error

.lba_success:
    jmp near after_read

.no_lba:
    push es
    xor di, di
    mov es, di
    mov ah, 0x08
    mov dl, [boot_drive]
    int 0x13
    pop es
    mov si, [partition]
    jc short .disk_error_short

    and cx, 0x3F
    jz short .disk_error_short
    inc dh
    jz short .disk_error_short
    mov bx, cx
    mov cl, dh
    mov di, cx

    mov ax, [si + 10]
    xor dx, dx
    div bx
    xchg ax, cx
    mov ax, [si + 8]
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

    mov di, RETRIES
.chs_retry:
    push di
    push cx
    push dx

    mov bx, 0x7C00
    mov ax, 0x0201
    int 0x13
    jnc short after_read

    pop dx
    push dx
    xor ah, ah
    int 0x13

    pop dx
    pop cx
    pop di

    dec di

    jnz short .chs_retry
    ; jmp short disk_error

disk_error:
    mov si, disk_error_msg
    jmp short print_error

after_read:
    cmp word [0x7DFE], 0xAA55
    jne short partition_no_marker

    mov sp, 0x7C00

    mov dl, [boot_drive]
    mov si, [partition]
    jmp sp

no_partition_found:
    mov si, no_partition_found_msg
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

lba_too_high_error:
    mov si, lba_too_high_error_msg
    ; jmp short print_error

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


partition_no_marker:
    mov si, [partition]
    mov byte [si], 0x00

    jmp near init_check_loop


no_partition_found_msg db "No bootable partition found!", 0
disk_error_msg db "Disk error!", 0
lba_too_high_error_msg db "LBA too high!", 0
press_enter_to_restart_msg db 0x0D, 0x0A, "Press any key to restart...", 0x0D, 0x0A, 0

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
