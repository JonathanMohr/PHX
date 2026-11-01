[bits 16]
[org 0x0600]
[cpu 8086]

entry:
    cli

    mov [base_data + pnp], es
    mov [base_data + pnp + 2], di

    xor ax, ax

    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    mov si, sp
    mov di, sp
    mov cx, 512
    rep movsb

    jmp 0x0000:at_0600

at_0600:
    xchg ax, dx
    stosb
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

    shr cl, 1
    jnc short .no_lba

.use_lba:
    mov byte [read_dispatch + 1], read_lba - read_chs
    mov si, base_data + drive_params
    mov word [si], 0x1A
    mov ah, 0x48
    mov dl, [base_data + boot_drive]
    int 0x13
    jc short .no_lba
    mov ax, [base_data + drive_params + 0x18]
    cmp ah, 2
    jae short .store

.no_lba:
    mov ax, 512

.store:
    mov di, base_data + bytes_per_sector
    stosw

.after:
    xor ax, ax
    push ax
    push ax
    push ax
    inc ax
    push ax


read_header:
    mov bp, base_data

    mov bx, sector_buffer
    call read_sector

    ; Maybe check CRC at sector_buffer + 16

    ; Maybe check if entry size is bigger than sector size
    ; TODO: Maybe check if entry size is bigger than 512,
    ;       because then it could overwrite important stuff

    cmp word [sector_buffer + 86], 0
    jnz format_error

    mov bx, [bp + bytes_per_sector]
    add bx, partition_entry_buffer
    mov [bp + buffer_end], bx

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
    mov si, sector_buffer
.entry:
    mov ax, [si + 80]
    or ax, [si + 82]
    jz no_partition_found

    push di
    xor ax, ax
    mov cx, 8
    repe scasw
    pop di
    je .next

    test byte [di + 48], 4
    jnz .candidate

.next:
    sub word [si + 80], 1
    sbb word [si + 82], 0
    add di, [si + 84]
    cmp di, [bp + buffer_end]
    jb .entry
    jmp .sector

.candidate:
    mov si, di
    mov [bp + partition_entry], di
    mov di, 0x07D2
    mov cx, [sector_buffer + 84]
    mov [0x07CE], cx
    mov word [0x07D0], 0
    rep movsb
    mov di, [bp + partition_entry]

    mov bx, 0x7C00
    lea sp, [di + 32]
    call read_sector

    cmp word [0x7DFE], 0xAA55
    je .found

    mov sp, sector_buffer + 72
    jmp .next

.found:
    mov si, 0x07BE
    mov di, si
    xor ax, ax
    mov cx, 8
    rep stosw

    mov byte [si], 0x80
    mov byte [si + 4], 0xEE

    mov ax, [si + 0x34]
    mov dx, [si + 0x36]
    mov cx, [si + 0x38]
    or cx, [si + 0x3A]
    jz .start_ok
    mov ax, 0xFFFF
    mov dx, ax
.start_ok:
    mov [si + 8], ax
    mov [si + 10], dx

    mov ax, [si + 0x3C]
    sub ax, [si + 0x34]
    mov dx, [si + 0x3E]
    sbb dx, [si + 0x36]
    mov cx, [si + 0x40]
    sbb cx, [si + 0x38]
    mov di, [si + 0x42]
    sbb di, [si + 0x3A]
    inc ax
    jnz short .start_ok_done
    inc dx
    jnz short .start_ok_done
    inc cx
    jnz short .start_ok_done
    inc di
.start_ok_done:
    or cx, di
    jz .count_ok
    mov ax, 0xFFFF
    mov dx, ax
.count_ok:
    mov [si + 12], ax
    mov [si + 14], dx


    mov es, [base_data + pnp]
    mov di, [base_data + pnp + 2]
    ; mov si, 0x07BE
    mov sp, 0x7C00
    mov dl, [base_data + boot_drive]

    pushf
    pop ax
    or ah, 0x80
    push ax
    popf
    pushf
    pop ax
    test ah, 0x80
    jz .go

[cpu 386]
    mov eax, 0x54504721 ; "!GPT"

.go:
[cpu 8086]
    sti
    jmp sp


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


lba_too_high_error:
    mov si, lba_too_high_error_msg
    jmp short print_error

disk_error:
    mov si, disk_error_msg
    jmp short print_error


;
; 64-bit LBA on stack
; target offset in bx
;

%define RETRIES 3
read_sector:
    push di
    push bp
    pop word [bp + read_return_address]
read_dispatch:
    jz short read_chs

read_chs:
    mov ah, 0x08
    mov dl, [bp + boot_drive]
    push bx
    int 0x13
    pop bx
    mov si, sp
    jc short disk_error

    mov ax, [si + 4]
    or ax, [si + 6]
    jnz short lba_too_high_error

    and cx, 0x3F
    jz short disk_error
    inc dh
    jz short disk_error
    mov bp, cx
    mov cl, dh
    mov di, cx

    cwd
    mov ax, [si + 2]
    div bp
    xchg ax, cx
    mov ax, [si]
    div bp
    xchg cx, dx
    inc cx

    cmp dx, di
    jae short lba_too_high_error
    div di
    cmp ax, 1023
    ja short lba_too_high_error

    mov dh, dl
    mov ch, al
    ror ah, 1
    ror ah, 1
    or cl, ah

    mov ax, 0x0201
    sub sp, 8
    ; jmp short read_int13_retry

read_int13_retry:
    mov di, RETRIES
.again:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    mov dl, [base_data + boot_drive]
    int 0x13
    jnc short read_ok
    mov dl, [base_data + boot_drive]
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

    jmp short disk_error

read_ok:
    add sp, 20
    pop bp
    pop di
    jmp word [bp + read_return_address]

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
    cld
.word:
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
    jns .word
    ret

spec db ' ', '.', '!', '?', 0x0A, 0x0D

; "no partition found."
no_partition_found_msg dw 0x6bb4, 0x0b9c, 0x9802, 0x5934, 0x7822, 0x9a26, 0x439a, 0xd15c, 0xa346, 0xdeb5
; "disk error!"
disk_error_msg dw 0x1a24, 0x5688, 0x8c5c, 0x8f35
; "format error!"
format_error_msg dw 0x2ba2, 0x6026, 0xd122, 0x8ba2, 0xe6b5
; "lba too high!"
lba_too_high_error_msg dw 0x5840, 0xd4dc, 0x768e, 0x418e, 0xe6b5
; "\r\npress any key to restart...\r\n"
press_enter_to_restart_msg dw 0xff9e, 0x8924, 0x9680, 0x6e34, 0x5130, 0xd4dc, 0xd448, 0x94c0, 0x8cf6, 0xdef4, 0xd7fd


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
base_data equ 0x500
boot_drive equ 0x0          ; byte
pnp equ 0x1                 ; 2 words
bytes_per_sector equ 0x5    ; word
read_return_address equ 0x7 ; word
buffer_end equ 0x9          ; word
partition_entry equ 0xB     ; word
drive_params equ 0xD        ; 26 byte


; other
sector_buffer equ 0x7E00             ; up to 607,5 KiB
partition_entry_buffer equ 0x8000    ; up to 607 KiB

