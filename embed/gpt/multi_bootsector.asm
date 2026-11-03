[bits 16]
[org 0x0600]
[cpu 8086]

entry:

boot_drive:
    cli
    cld

sector_size:
    xor ax, ax

    mov ss, ax

    mov sp, 0x7C00
    mov si, sp

    mov bx, read_sector
    push bx
    mov bx, print_error
    push bx
    push es
    push di

    mov ds, ax
    mov es, ax

    mov di, 0x0600
    mov cx, 512
    rep movsb

    jmp 0x0000:at_0600

at_0600:
    mov [boot_drive], dl
    sti

check_disk:
    ; check lba
    mov ah, 0x41
    mov bx, 0x55AA
    ; dl is still set
    push dx
    int 0x13
    pop dx
    jc short .no_lba
    cmp bx, 0xAA55
    jne short .no_lba
    shr cl, 1
    jnc short .no_lba

.use_lba:
    mov si, 0x500
    mov word [si], 0x1A
    mov ah, 0x48
    ; dl is still set
    int 0x13
    jc short disk_error
    cmp word [si], 0x1A
    jb short disk_error

    mov byte [read_dispatch + 1], ah

    mov ax, [0x500 + 0x18]
    jmp short .store

.no_lba:
    mov ax, 512

.store:
    mov [sector_size], ax

    mov si, next_stage_lba
    mov bx, [si + 8 + 2]
    or bx, [si + 8 + 4]
    or bx, [si + 8 + 6]
    mul word [si + 8]
    or dx, bx
    jnz short next_stage_too_big

    dec ax
    cmp ax, 0x6000 - 0x800 - 1
    ja short next_stage_too_big

    mov di, 0x800
.read_loop:
    call read_sector
    add di, [sector_size]
    mov bx, si
    mov cx, 4
.carry:
    inc word [bx]
    jnz short .next
    inc bx
    inc bx
    loop .carry
.next:
    dec word [si + 8]
    jnz .read_loop

.done:
    jmp near 0x800


next_stage_too_big:
    call print_error
    ; "n.stage too big"
    dw 0x6ee4, 0x980c, 0x26a6, 0x73b4, 0x0a0d

disk_error:
    call print_error
    ; "disk error"
    dw 0x1a24, 0x5688, 0x8c5c, 0x8eb5

;
; ds:si - Points to 64-bit LBA
; es:di - Address to read into
; does not destroy cs, ds, ss, es, si and di
;

%define RETRIES 3
read_sector:
    push ds
    push si
    push di
    push word [si + 6]
    push word [si + 4]
    push word [si + 2]
    push word [si]
    mov dl, [cs:boot_drive]
read_dispatch:
    jmp short read_chs

read_lba:
    push es ; segment
    push di ; offset

    mov ax, 1
    push ax ; sector count

    mov al, 0x10
    push ax

    mov si, sp
    push ss
    pop ds

    mov ah, 0x42
    jmp short read_int13_retry

lba_too_high_error:
    call print_error
    ; "chs error"
    dw 0x11e4, 0xd122, 0x8ba3

read_chs:
    push es
    push di
    xor di, di
    mov es, di
    mov ah, 0x08
    int 0x13
    pop bx
    pop es
    jc short disk_error

    pop si
    pop di
    pop ax
    pop bp
    or ax, bp
    jnz short lba_too_high_error

    and cx, 0x3F
disk_error_alias_zf:
    jz short disk_error
    mov al, dh
    inc ax
    cwd
    xchg ax, di
    div cx
    xchg ax, si
    div cx
    xchg dx, si
    cmp dx, di
    jae short lba_too_high_error
    div di
    cmp ax, 1023
    ja short lba_too_high_error

    mov dh, dl
    xchg al, ah
    ror al, 1
    ror al, 1
    inc si
    or ax, si
    xchg ax, cx
    mov dl, [cs:boot_drive]
    mov ax, 0x0201
    sub sp, 16
    ; jmp short read_int13_retry

read_int13_retry:
    mov di, RETRIES
.again:
    push ax
    push bx
    push cx
    push si
    push di
    push dx
    int 0x13
    jc short .read_not_ok

    add sp, 28
    pop di
    pop si
    pop ds
    ret

.read_not_ok:
    pop dx
    push dx
    xor ah, ah
    int 0x13
    pop dx
    pop di
    pop si
    pop cx
    pop bx
    pop ax
    dec di
    jnz short .again

    jmp short disk_error_alias_zf

; "!\r\npress any key to restart..."
press_enter_to_restart_msg dw 0xe7ba, 0x7c48, 0x94b4, 0x0370, 0xd288, 0xc6a6, 0x76a2, 0x24a6, 0x0466, 0xdef7
print_error:
    push cs
    pop ds

    pop si
    call print_raw
    mov si, press_enter_to_restart_msg
    call print_raw

    xor ah, ah
    int 16h
    ; jmp short restart

restart:
    mov al, 0xFE
    out 0x64, al
    jmp 0xFFFF:0x0000

;
; one 16 bit word:
;   5 bit: first char
;   5 bit: second char
;   5 bit: third char
;   1 bit: end marker
;

print_raw:
    lodsw
    mov cx, 0x0305
.char:
    rol ax, cl
    push ax
    and al, 0x1F
    add al, 'a'
    cmp al, 'z' + 1
    jb short .out
    mov bx, spec - ('z' + 1)
    xlat
.out:
    mov ah, 0x0E
    mov bx, 0x07
    push cx
    push si
    int 0x10
    pop si
    pop cx
    pop ax
    dec ch
    jnz short .char
    test ax, ax
    jns short print_raw
    ret

spec db ' ', '.', '!', 0x0A, 0x0D

times 424 - ($ - $$) db 0

next_stage_lba dq 0
next_stage_count dq 0

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
