[bits 16]
[org 0x0600]
[cpu 8086]

entry:
    cli

    xor ax, ax

    mov [pnp], es
    mov [pnp + 2], di

    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x0600

    mov si, 0x7C00
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
    mov si, drive_params
    mov word [si], 0x1A
    mov ah, 0x48
    mov dl, [boot_drive]
    int 0x13
    jc short .no_lba
    mov ax, [drive_params + 0x18]
    cmp ah, 2
    jae short .store

.no_lba:
    mov ax, 512

.store:
    mov di, bytes_per_sector
    stosw

.after:
    xor ax, ax
    push ax
    push ax
    push ax
    inc ax
    push ax


read_header:
    mov word [read_target_address], sector_buffer
    call read_sector

.validate_header:
    cld
    mov si, sector_buffer
    mov di, header_signature
    mov cx, 4
    repe cmpsw
    jne format_error

    ; Maybe check CRC at sector_buffer + 16

    ; Maybe check if entry size is bigger than sector size

    cmp word [sector_buffer + 86], 0
    jnz format_error

    mov bx, [bytes_per_sector]
    add bx, partition_entry_buffer
    mov [buffer_end], bx

    mov sp, sector_buffer + 72
.sector:
    mov word [read_target_address], partition_entry_buffer
    call read_sector

    add word [sector_buffer + 72], 1
    adc word [sector_buffer + 74], 0
    adc word [sector_buffer + 76], 0
    adc word [sector_buffer + 78], 0

    mov di, partition_entry_buffer
.entry:
    mov ax, [sector_buffer + 80]
    or ax, [sector_buffer + 82]
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
    sub word [sector_buffer + 80], 1
    sbb word [sector_buffer + 82], 0
    add di, [sector_buffer + 84]
    cmp di, [buffer_end]
    jb .entry
    jmp .sector

.candidate:
    mov si, di
    mov [partition_entry], di
    mov di, 0x07D2
    mov cx, [sector_buffer + 84]
    mov [0x07CE], cx
    mov word [0x07D0], 0
    rep movsb
    mov di, [partition_entry]

    mov word [read_target_address], 0x7C00
    lea sp, [di + 32]
    call read_sector

    mov di, [partition_entry]

    cmp word [0x7DFE], 0xAA55
    je .found

    mov sp, sector_buffer + 72
    jmp .next

.found:
    mov di, 0x07BE
    xor ax, ax
    mov cx, 8
    rep stosw

    mov byte [0x07BE], 0x80
    mov byte [0x07C2], 0xEE

    mov ax, [0x07F2]
    mov dx, [0x07F4]
    mov cx, [0x07F6]
    or cx, [0x07F8]
    jz .start_ok
    mov ax, 0xFFFF
    mov dx, ax
.start_ok:
    mov [0x07C6], ax
    mov [0x07C8], dx

    mov ax, [0x07FA]
    sub ax, [0x07F2]
    mov dx, [0x07FC]
    sbb dx, [0x07F4]
    mov cx, [0x07FE]
    sbb cx, [0x07F6]
    mov di, [0x0800]
    sbb di, [0x07F8]
    add ax, 1
    adc dx, 0
    adc cx, 0
    adc di, 0
    or cx, di
    jz .count_ok
    mov ax, 0xFFFF
    mov dx, ax
.count_ok:
    mov [0x07CA], ax
    mov [0x07CC], dx    


    mov es, [pnp]
    mov di, [pnp + 2]
    mov si, 0x07BE
    mov sp, 0x7C00
    mov dl, [boot_drive]

    xor ax, ax
    push ax
    popf
    pushf
    pop ax
    and ah, 0xF0
    cmp ah, 0xF0
    je .go

    mov ax, 0x7000
    push ax
    popf
    pushf
    pop ax
    and ah, 0x70
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
print_error_alias:
    jmp short print_error


;
; 64-bit LBA on stack
; target offset in read_target_address
;

%define RETRIES 3
read_sector:
    pop word [read_return_address]
read_dispatch:
    jz short read_chs

read_chs:
    mov ah, 0x08
    mov dl, [boot_drive]
    int 0x13
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

    cwd
    mov ax, [si + 2]
    div bx
    xchg ax, cx
    mov ax, [si]
    div bx
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

    mov bx, [read_target_address]
    mov ax, 0x0201
    sub sp, 8
    jmp short read_int13_retry

disk_error:
    mov si, disk_error_msg
    jmp short print_error_alias

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

    jmp short disk_error

read_ok:
    add sp, 20
    jmp word [read_return_address]

read_lba:
    xor ax, ax
    
    push ax ; segment

    push word [read_target_address]

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
boot_drive equ 0x500                ; byte
pnp equ 0x501                       ; 2 words
bytes_per_sector equ 0x505          ; word
read_return_address equ 0x507       ; word
read_target_address equ 0x509       ; word
buffer_end equ 0x50B                ; word
partition_entry equ 0x50D           ; word
drive_params equ 0x50F              ; 26 byte


; other
sector_buffer equ 0xA00             ; up to 29184 byte
partition_entry_buffer equ 0xB00    ; up to 28928 byte
