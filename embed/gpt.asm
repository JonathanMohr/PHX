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
    mov sp, 0x0600

    mov si, 0x7C00
    mov di, 0x0600
    mov cx, 512
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
;     xor ax, ax
;     push ax
;     push ax
;     push ax
;     inc ax
;     push ax
; 
;     mov word [read_target_address], sector_buffer
;     call read_sectors
; 
; .validate_header:
;     cld
;     mov si, sector_buffer
;     mov di, header_signature
;     mov cx, 2
;     repe cmpsd


    jmp short disk_error



;
; 64-bit LBA on stack
; target offset in read_target_address
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
    pop cx
    mov ch, al
    ror ah, 1
    ror ah, 1
    or cl, ah

    mov bx, [read_target_address]
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

    mov bx, [read_target_address]
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
    mov dl, [boot_drive]
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

    xor ah, ah
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
    add sp, 12
    ret



;
; one 16 bit word:
;   5 bit: first char
;   5 bit: second char
;   5 bit: third char
;   1 bit: end marker
;

print_raw:
    cld
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

spec: db ' ', '.', '!', '?', 0x0A, 0x0D

;
; print_char:
;     int 10h
; print_raw:
;     mov ah, 0eh
;     mov bx, 0x07
;     lodsb
;     test al, al
;     jnz short print_char
;     ret
;

; "no partition found."
; no_partition_found_msg dw 0x6bb4, 0x0b9c, 0x9802, 0x5934, 0x7822, 0x9a26, 0x439a, 0xd15c, 0xa346, 0xdeb5
; "disk error!"
disk_error_msg dw 0x1a24, 0x5688, 0x8c5c, 0x8f35
; "format error!"
; format_error_msg dw 0x2ba2, 0x6026, 0xd122, 0x8ba2, 0xe6b5
; "lba too high!"
lba_too_high_error_msg dw 0x5840, 0xd4dc, 0x768e, 0x418e, 0xe6b5
; "\r\npress any key to restart...\r\n"
press_enter_to_restart_msg dw 0xff9e, 0x8924, 0x9680, 0x6e34, 0x5130, 0xd4dc, 0xd448, 0x94c0, 0x8cf6, 0xdef4, 0xd7fd




header_signature db "EFI PART"



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
lba equ 0x800
boot_drive equ 0x801

; word
read_return_address equ 0x900
bytes_per_sector equ 0x902
read_target_address equ 0x904

; other
drive_params equ 0xA00  ; 26 byte

sector_buffer equ 0x1000  ; up to 24576 byte
