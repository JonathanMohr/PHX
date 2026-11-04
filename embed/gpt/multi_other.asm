[bits 16]
[org 0x0800]
[cpu 8086]

entry:
    pop word [pnp.offset]
    pop word [pnp.segment]
    pop word [print_error]
    pop word [read_sector_raw]

    cmp word [sector_size], 512
    jb short sector_size_too_low

    cmp word [sector_size], 32768
    ja short sector_size_too_high

    ; TODO: Change disk_error for read_sector_raw somehow

    jmp check_header

sector_size_too_low:
    call [print_error]
    dw 0x9104, 0x9ba2, 0xd490, 0xc934, 0x9b9c, 0xd2dc, 0xb6b5

sector_size_too_high:
    call [print_error]
    dw 0x9104, 0x9ba2, 0xd490, 0xc934, 0x9b9c, 0xd1d0, 0x31f5

format_error_alias:
    jmp near format_error

check_header:
    xor ax, ax
    mov ds, ax
    mov es, ax

    mov si, header_lba
    mov di, header_buffer

    call read_sector

.validate_header:
    cld

    ; signature
    mov si, header_buffer
    mov di, gpt_signature
    mov cx, 4
    repe cmpsw
    jne short format_error_alias

    ; revision
    cmp word [header_buffer + 8], 0x0000
    jne short format_error_alias

    cmp word [header_buffer + 10], 0x0001
    jne short format_error_alias

    ; header size
    cmp word [header_buffer + 14], 0
    jne short format_error_alias

    mov ax, [header_buffer + 12]

    cmp ax, 92
    jb short format_error_alias

    cmp ax, [sector_size]
    ja short format_error_alias

    ; TODO: CRC

    ; this lba
    mov si, header_buffer + 24
    mov di, header_lba
    mov cx, 4
    repe cmpsw
    jne short format_error_alias

    ; partition entry count
    mov ax, [header_buffer + 80]
    or ax, [header_buffer + 82]
    jz short format_error_alias

    ; partition entry size
    cmp word [header_buffer + 86], 0x0000
    jne short format_error_alias

    mov bx, [header_buffer + 84]

    cmp bx, 32768
    ja short format_error_alias

    cmp bx, 128
    jb short format_error_alias

    lea ax, [bx - 1]
    test ax, bx
    jnz short format_error_alias

    mov ax, [sector_size]
    cmp ax, bx
    jb short header_entry_larger

    xor dx, dx
    div bx
    test dx, dx
    jnz short sector_size_no_multiple_of_entry_size

    mov bp, ax
    mov cx, 1

    jmp short header_sizes_done

sector_size_no_multiple_of_entry_size:
    call word [print_error]
    ; "sector size no multiple of partition entry size"
    dw 0x9104, 0x9ba2, 0xd490, 0xc934, 0x6bb4, 0x6516, 0x9a1e, 0x5934, 0x7174, 0x7822, 0x9a26, 0x439a, 0xd11a, 0x9c70, 0xd490, 0xc935

entry_size_no_multiple_of_sector_size:
    call word [print_error]
    ; "entry size no multiple of partition sector size"
    dw 0x2366, 0x8e34, 0x9232, 0x269a, 0x7698, 0xa2e6, 0x43d6, 0x269c, 0x2e9e, 0x0466, 0x44d0, 0x7374, 0x9104, 0x9ba2, 0xd490, 0xc935

header_entry_larger:
    xchg ax, bx
    xor dx, dx
    div bx
    test dx, dx
    jnz short entry_size_no_multiple_of_sector_size

    mov bp, 1
    mov cx, ax

header_sizes_done:
    mov si, header_buffer + 72
    mov di, header_lba
    movsw
    movsw
    movsw
    movsw

    mov si, header_lba
    mov di, parray_buffer

.read_partition_entry_array:
    ; cx - sectors to read
    ; bp - entries per read
    ; si - current lba
    ; di - buffer

    push cx
    push bp
    push di

.read_block:
    pop di
    pop bp
    pop cx
    push cx
    push bp
    push di

.read_sector_loop:
    call read_sector

    add word [si], 1
    adc word [si + 2], 0
    adc word [si + 4], 0
    adc word [si + 6], 0
    jc short format_error

    add di, [sector_size]
    loop .read_sector_loop

    pop di
    push di

.check_entry:
    xor ax, ax
    mov cx, 8
    push di
    repe scasw
    pop di
    je short .next

    test byte [di + 48], 4 ; legacy bios boot attribute
    jnz short found_partition

.next:
    sub word [header_buffer + 80], 1
    sbb word [header_buffer + 82], 0
    mov ax, [header_buffer + 80]
    or ax, [header_buffer + 82]
    jz short no_partition_found

    add di, [header_buffer + 84]
    dec bp
    jnz short .check_entry

    jmp short .read_block

format_error:
    dec word [header_lba]
    jnz short .direct

    jmp near use_alternative_header

.direct:
    call [print_error]
    dw 0x2ba2, 0x6026, 0xd122, 0x8ba3

no_partition_found:
    call [print_error]
    ; "no bootable partition"
    dw 0x6bb4, 0x0b9c, 0x9802, 0x5934, 0x7822, 0x9a26, 0x439b

found_partition:
    add sp, 6
    mov bp, di

    mov ax, 0x1000
    mov es, ax

    ; copy partition entry
    mov si, di
    mov di, 0x14
    mov cx, [header_buffer + 84]
    rep movsb

    ; TODO: CHS
    xor di, di
    mov ax, 0x80
    stosw
    xor ax, ax
    stosw
    mov al, 0xED
    stosw
    xor ax, ax
    stosw

    lea si, [bp + 32]
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

    mov si, header_buffer + 84
    movsw
    movsw


    xor ax, ax
    mov es, ax
    lea si, [bp + 32]
    mov di, 0x7C00
    call read_sector


    mov dl, [boot_drive]
    mov es, [pnp.segment]
    mov di, [pnp.offset]

    mov ax, 0x1000
    mov ds, ax
    xor si, si

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


disk_error:
    dec word [header_lba]
    jnz short direct_disk_error

    jmp short use_alternative_header

direct_disk_error:
    call word [print_error]
    ; "disk error"
    dw 0x1a24, 0x5688, 0x8c5c, 0x8eb5

use_alternative_header:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    mov ah, 0x41
    mov bx, 0x55AA
    mov dl, [boot_drive]
    push dx
    int 0x13
    pop dx

    jc short .no_lba
    
    cmp bx, 0xAA55
    jne short .no_lba

    shr cl, 1
    jnc short .no_lba

.lba:
    mov si, 0x500
    mov word [si], 0x1A
    mov ah, 0x48
    ; dl is still set

    int 0x13
    jc short direct_disk_error

    cmp word [si], 0x1A
    jb short direct_disk_error

    mov ax, [0x510]
    mov dx, [0x512]
    mov cx, [0x514]
    mov bx, [0x516]

    jmp short .found

.no_lba:
    mov ah, 0x08
    xor di, di
    mov es, di
    ; dl is still set

    int 0x13
.direct_disk_error_alias:
    jc short direct_disk_error

    mov al, cl
    and ax, 0x003F
    mov bx, ax

    mov al, cl
    mov cl, 6
    shr al, cl
    mov ah, al
    mov al, ch
    inc ax
    mov si, ax

    mov cl, dh
    xor ch, ch
    inc cx

    mov ax, bx
    mul cx
    mul si

    xor cx, cx
    xor bx, bx

.found:
    sub ax, 1
    sbb dx, 0
    sbb cx, 0
    sbb bx, 0
    jc short .direct_disk_error_alias

    mov [header_lba], ax
    mov [header_lba + 2], dx
    mov [header_lba + 4], cx
    mov [header_lba + 6], bx

    jmp near check_header

;
; ds:si - Address to 64-bit LBA
; es:di - Address to read into
;

read_sector:
    pushf
    push ax
    push bx
    push cx
    push dx
    push bp
    
    call word [read_sector_raw]

    pop bp
    pop dx
    pop cx
    pop bx
    pop ax
    popf

    ret


header_lba dq 1


header_buffer equ 0x7C00
parray_buffer equ 0x7E00


pnp:
    .segment dw 0
    .offset dw 0

boot_drive equ 0x0600
sector_size equ 0x0601

read_sector_raw dw 0
print_error dw 0

gpt_signature db "EFI PART"

times 1024 - ($ - $$) db 0
