[bits 16]
[org 0x0600]
[cpu 8086]

entry:
    cli
    cld

    xor ax, ax

    mov ss, ax
    mov sp, 0x7C00
    mov si, sp

    push di
    push es
    push dx

    mov ds, ax
    mov es, ax

    mov di, 0x0600
    mov cx, 512
    rep movsb

    jmp 0x0000:at_0600

found_partition:
;    mov si, 0x07BE
;    mov di, si
;    xor ax, ax
;    mov cx, 8
;    rep stosw
;
;    mov byte [si], 0x80
;    mov byte [si + 4], 0xEE
;
;    mov ax, [si + 0x34]
;    mov dx, [si + 0x36]
;    mov cx, [si + 0x38]
;    or cx, [si + 0x3A]
;    jz .start_ok
;    mov ax, 0xFFFF
;    mov dx, ax
;.start_ok:
;    mov [si + 8], ax
;    mov [si + 10], dx
;
one_offset equ $ + 1 - entry
    mov ax, 1
;    cwd
;    xor cx, cx
;    xor di, di
;    add ax, [si + 0x3C]
;    adc dx, [si + 0x3E]
;    adc cx, [si + 0x40]
;    adc di, [si + 0x42]
;    sub ax, [si + 0x34]
;    sbb dx, [si + 0x36]
;    sbb cx, [si + 0x38]
;    sbb di, [si + 0x3A]
;    or cx, di
;    jz .count_ok
;    mov ax, 0xFFFF
;    mov dx, ax
;.count_ok:
;    mov [si + 12], ax
;    mov [si + 14], dx
;
;
    mov sp, 0x7BFA
    pop dx
    pop es
    pop di
;    ; mov si, 0x07BE
;
;    pushf
;    pop ax
;    or ah, 0x80
;    push ax
;    popf
;    pushf
;    pop ax
;    test ah, 0x80
;    jz .go
;
;[cpu 386]
;    mov eax, 0x54504721 ; "!GPT"
;
;.go:
;[cpu 8086]
    sti
    jmp sp

at_0600:
    sti

check_disk:
    ; check lba
    mov ah, 0x41
    mov bx, 0x55AA
    ; dl is still set
    push dx
    stc
    int 0x13
    pop dx
    jc short .no_lba

    shr cl, 1
    jnc short .no_lba

.use_lba:
    mov byte [read_dispatch + 1], read_lba - read_chs
    mov si, drive_params
    mov word [si], 0x1A
    mov ah, 0x48
    ; dl is still set
    int 0x13
    mov ax, [drive_params + 0x18]
    jnc short .store

.no_lba:
    mov ax, 512

.store:
    push ax ; 0x7FB8
    sub sp, 6 ; read_return_address, buffer_end, partition_entry

.after:
    mov bp, 0x7C00

    xor ax, ax

    push ax
    push ax
    push ax

    inc ax
    push ax

read_header:
    mov bx, bp
    call read_sector

    jmp format_error

sector_buffer equ 0x7C00             ; up to 608 KiB
partition_entry_buffer equ 0x7E00    ; up to 607,5 KiB

    ; Maybe check CRC at sector_buffer + 16

    ; Maybe check if entry size is bigger than sector size
    ; TODO: Maybe check if entry size is bigger than 512,
    ;       because then it could overwrite important stuff

    cmp word [bp + 86], 0
    jnz format_error

    mov bx, [bp - 8]
    add bx, partition_entry_buffer
    mov [bp - 12], bx

    mov sp, sector_buffer + 72
.sector:
    mov bx, partition_entry_buffer
    call read_sector

    mov si, sector_buffer + 72
    mov cx, 4
.sector_loop:
    inc word [si]
    lodsw
    loopz .sector_loop

.sector_after_add:
    mov di, partition_entry_buffer
;    mov si, bp
;.entry:
;    mov ax, [si + 80]
;    or ax, [si + 82]
;    jz no_partition_found
;
;    push di
;    xor ax, ax
;    mov cx, 8
;    repe scasw
;    pop di
;    je .next
;
;    test byte [di + 48], 4
;    jnz .candidate
;
.next:
;    sub word [si + 80], 1
;    sbb word [si + 82], 0
;    add di, [si + 84]
;    cmp di, [bp - 12]
;    jb .entry
    jmp .sector

.candidate:
    mov si, di
    mov [bp - 14], di
    mov di, 0x07D2
    mov cx, [bp + 84]
    mov [di - 4], cx
    mov word [di - 2], 0
    rep movsb
    mov di, [bp - 14]

    mov bx, bp
    lea sp, [di + 32]
    call read_sector

    cmp word [0x7DFE], 0xAA55
    je found_partition

    mov sp, sector_buffer + 72
    jmp .next


no_partition_found:
    mov si, no_partition_found_msg
    jmp short print_error

format_error:
    mov si, format_error_msg
    ; jmp short print_error

print_error:
    call print_raw
    mov si, press_enter_to_restart_msg
    call print_raw

    xor ah, ah
    int 16h
    ; jmp short restart

restart:
    xor cx, cx

.delay:
    in al, 0x64
    loop .delay

    pushf
    pop ax
    test ah, ah
    js .fallback
[cpu 286]
    lidt [.fallback + 1]
    int3

[cpu 8086]
.fallback:
    jmp 0xFFFF:0x0000


lba_too_high_error:
    mov si, lba_too_high_error_msg
print_error_alias:
    jmp short print_error


;
; 64-bit LBA on stack
; target offset in bx
;

%define RETRIES 3
read_sector:
    pop word [bp - 10]
    push di
    push bp
read_dispatch:
    jmp short read_chs
read_chs:
    jmp word [bp - 10]
    mov ah, 0x08
    mov dl, [bp - 6]
    push bx
    int 0x13
    pop bx
    jc short disk_error

    pop si
    pop di
    pop ax
    pop bp
    or ax, bp
    jnz short lba_too_high_error

    and cx, 0x3F
    jz short disk_error
    mov al, dh
    inc ax
    xchg ax, bp
    cwd
    xchg ax, di
    div cx
    xchg ax, si
    div cx
    xchg dx, si
    cmp dx, bp
    jae short lba_too_high_error
    div bp
    cmp ax, 1023
    ja short lba_too_high_error

    mov dh, dl
    inc si
    mov cx, si
    mov ch, al
    ror ah, 1
    ror ah, 1
    or cl, ah

    mov ax, 0x0201
    sub sp, 16
    ; jmp short read_int13_retry

read_int13_retry:
    mov dl, [bp - 6]
    mov di, RETRIES
.again:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    int 0x13
    jc short .read_not_ok

    add sp, 20
    pop bp
    pop di
    jmp word [bp - 10]

.read_not_ok:
    mov dl, [0x7BFA]
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

disk_error:
    mov si, disk_error_msg
    jmp short print_error_alias

read_lba:
    xor ax, ax

    push ax ; segment
    push bx ; offset

    inc ax
    push ax ; sector count

    mov al, 0x10
    push ax

    mov si, sp
    mov ah, 0x42
    jmp short read_int13_retry


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
    jb .out
    mov bx, spec - ('z' + 1)
    xlat
.out:
    mov ah, 0x0E
    mov bx, 0x07
    int 0x10
    pop ax
    dec ch
    jnz .char
    test ax, ax
    jns print_raw
    ret

spec db ' ', '.', '!', 0x0A, 0x0D

; ; "no bootable partition found"
; no_partition_found_msg dw 0x6bb4, 0x0b9c, 0x9802, 0x5934, 0x7822, 0x9a26, 0x439a, 0xd15c, 0xa347
; ; "disk error"
; disk_error_msg dw 0x1a24, 0x5688, 0x8c5c, 0x8eb5
; ; "format error"
; format_error_msg dw 0x2ba2, 0x6026, 0xd122, 0x8ba3
; ; "lba too high"
; lba_too_high_error_msg dw 0x5840, 0xd4dc, 0x768e, 0x418f
; ; "!\r\npress any key to restart...\r\n"
; press_enter_to_restart_msg dw 0xe7ba, 0x7c48, 0x94b4, 0x0370, 0xd288, 0xc6a6, 0x76a2, 0x24a6, 0x0466, 0xdef6, 0xd7bb

no_partition_found_msg dw 0x7823
disk_error_msg dw 0x1a25
format_error_msg dw 0x2ba3
lba_too_high_error_msg dw 0x5841
press_enter_to_restart_msg dw 0xf777

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

drive_params equ 0x500 ; 26 byte
