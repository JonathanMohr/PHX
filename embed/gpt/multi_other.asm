[bits 16]
[org 0x0800]
[cpu 8086]

entry:
    pop word [pnp.offset]
    pop word [pnp.segment]
    pop word [print_error]
    pop word [read_sector_raw]

    ; TODO: Change disk_error for read_sector_raw somehow

check_header:
    xor ax, ax
    mov ds, ax
    mov es, ax

    mov si, header_lba
    mov di, header_buffer

    call read_sector

.validate_header:
    cld
    mov si, header_buffer
    mov di, gpt_signature
    mov cx, 4
    repe cmpsw
    jne short format_error

    ; also check CRC32 and stuff


    call [print_error]
    dw 0x99d0, 0x9690, 0x96a6, 0x3934, 0x99c8, 0xd348, 0xbcf4, 0x94c0, 0x3134, 0x7174, 0x99c8, 0xd05c, 0x74e4, 0x20a6, 0x7475

format_error:
    dec word [header_lba]
    jnz short direct_disk_error

    jmp near use_alternative_header

    call [print_error]
    dw 0x2ba2, 0x6026, 0xd122, 0x8ba3

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

times 512 - ($ - $$) db 0
