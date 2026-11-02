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

;
; 64-bit LBA on stack
; target offset in bx
;

%define RETRIES 3
read_sector:
    pop word [bp - 10]

    xor ax, ax

    push ax ; segment
    push bx ; offset

    inc ax
    push ax ; sector count

    mov al, 0x10
    push ax

    mov cx, RETRIES
.again:
    mov si, sp
    push cx
    mov ah, 0x42
    mov dl, [0x7C00 - 6]
    int 0x13
    mov bp, 0x7C00
    jc short .read_not_ok

    push ds
    pop es
    pop ax
    pop ax
    pop ax
    pop di
    pop ax
    jmp word [bp - 10]

.read_not_ok:
    mov dl, [bp - 6]
    xor ah, ah
    int 0x13
    pop cx
    loop .again

    ; jmp short disk_error

disk_error:
    call print_error
    ; "disk error"
    disk_error_msg dw 0x1a24, 0x5688, 0x8c5c, 0x8eb5

at_0600:
    sti

check_disk:
    mov ah, 0x41
    mov bx, 0x55AA
    ; dl is still set
    push dx
    stc
    int 0x13
    pop dx
    jc short disk_error
    shr cl, 1
    jnc short disk_error

    mov si, drive_params
    mov word [si], 0x1A
    mov ah, 0x48
    ; dl is still set
    int 0x13
    jc short disk_error
    mov ax, [drive_params + 0x18]

    add ax, partition_entry_buffer
    push ax ; 0x7FB8
    push ds ; read_return_address

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

sector_buffer equ 0x7C00             ; up to 33 KiB sectors
partition_entry_buffer equ 0x7E00    ; up to 32.5 KiB sectors

    ; Maybe check CRC at sector_buffer + 16

    ; Maybe check if entry size is bigger than sector size
    ; TODO: Maybe check if entry size is bigger than 512,
    ;       because then it could overwrite important stuff

    mov ax, [bp + 82]
    or ax, [bp + 86]
    jnz short disk_error ; TODO: Maybe format_error

.sector:
    mov sp, sector_buffer + 72
    mov bx, partition_entry_buffer
    call read_sector

    mov si, sp
    mov cx, 4
.sector_loop:
    inc word [si]
    lodsw
    loopz .sector_loop
.entry:
    push di
    xor ax, ax
    mov cx, 8
    repe scasw
    pop di
    je short .next

    test byte [di + 48], 4
    jnz short found

.next:
    dec word [bp + 80]
    jz short no_partition_found
    add di, [bp + 84]
    cmp di, [bp - 8]
    jb short .entry
    jmp short .sector

no_partition_found:
    call print_error
    ; "no bootable partition"
    dw 0x6bb4, 0x0b9c, 0x9802, 0x5934, 0x7822, 0x9a26, 0x439b

; format_error:
;     call print_error
;     ; "format error"
;     dw 0x2ba2, 0x6026, 0xd122, 0x8ba3

; "!\r\npress any key to restart..."
press_enter_to_restart_msg dw 0xe7ba, 0x7c48, 0x94b4, 0x0370, 0xd288, 0xc6a6, 0x76a2, 0x24a6, 0x0466, 0xdef7
print_error:
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

found:
    push di
    push di
    lea si, [bp + 84]
    mov di, 0x07CE
    lodsw
    stosw
    xchg ax, cx
    movsw
    pop si
    rep movsb
    pop di

    mov bx, bp
    lea sp, [di + 32]
    call read_sector


    ; TODO: CHS
    mov di, 0x07BE
    mov ax, 0x80
    stosw
    xor ax, ax
    stosw
    mov al, 0xED ; synthetic entry
    stosw
    xor ax, ax
    stosw

    mov si, 0x07F2
    movsw
    movsw
    lodsw
    or ax, [si]
    jz short .start_ok
    mov ax, 0xFFFF
    sub di, 4
    stosw
    stosw
.start_ok:
    mov ax, 1
    cwd
    xor cx, cx
    xor bx, bx
    add ax, [si + 2]
    adc dx, [si + 4]
    adc cx, [si + 6]
    adc bx, [si + 8]
    sub ax, [si - 6]
    sbb dx, [si - 4]
    sbb cx, [si - 2]
    sbb bx, [si]
    or cx, bx
    jz short .count_ok
    mov ax, 0xFFFF
    cwd
.count_ok:
    stosw
    xchg ax, dx
    stosw


    lea sp, [bp - 6]
    pop dx
    pop es
    pop di
    mov si, 0x07BE

    pushf
    mov ah, 0x70
    push ax
    popf
    pushf
    pop ax
    popf
    cmp ah, 0x70
    jne short .go

[cpu 386]
    mov eax, 0x54504721 ; "!GPT"

.go:
[cpu 8086]
    jmp sp

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
