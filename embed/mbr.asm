[bits 16]
[org 0x0600]
[cpu 8086]

%define RETRIES 3

entry:

boot_drive: ; Reusing code that will not be used again after initializing as space for variable
    cli

partition: ; Reusing code that will not be used again after initializing as space for variable
    xor ax, ax

    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    mov si, 0x7C00
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
    mov ax, [si + 8]
    mov dx, [si + 10]

    mov [dap_lba], ax
    mov [dap_lba + 2], dx

    mov cx, RETRIES

.lba_retry:
    push cx
    mov si, dap
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jnc short .lba_success

    mov ah, 0x00
    mov dl, [boot_drive]
    int 0x13
    pop cx
    loop .lba_retry

    jmp near disk_error

.lba_success:
    pop cx
    jmp near after_read

.disk_error_short:
    jmp near disk_error

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

    cmp dh, 0xFF
    je short .disk_error_short

    and cl, 0x3F
    jz short .disk_error_short
    mov [spt], cl
    inc dh
    mov [heads], dh

    mov ax, [si + 8]
    mov dx, [si + 10]

    mov bx, ax
    mov ax, dx
    xor dx, dx
    div word [spt]
    mov cx, ax
    mov ax, bx
    div word [spt]

    inc dx
    mov [sector_result], dl

    mov bx, ax
    mov ax, cx
    xor dx, dx
    div word [heads]
    mov cx, ax
    mov ax, bx
    div word [heads]

    mov [head_result], dl
    mov [cyl_result], ax

    or cx, cx
    jnz short lba_too_high_error
    cmp ax, 1023
    ja short lba_too_high_error

    mov ch, [cyl_result]
    mov dl, [sector_result]
    mov al, [cyl_result + 1]
    mov cl, 6
    shl al, cl
    or dl, al

    mov cl, dl
    mov dh, [head_result]
    call do_read_chs

    ; jmp short after_read

after_read:
    cmp word [0x7DFE], 0xAA55
    jne short partition_no_marker

    mov dl, [boot_drive]
    mov si, [partition]
    jmp 0x0000:0x7C00


read_chs_success:
    add sp, 6
    ret

do_read_chs:
    mov dl, [boot_drive]
    mov bx, 0x7C00

    push bx
    push cx
    push dx

    mov ah, 0x02
    mov al, 1
    int 0x13
    jnc short read_chs_success

    mov ah, 0x00
    mov dl, [boot_drive]
    int 0x13

    pop dx
    pop cx
    pop bx

    dec byte [retry_count]
    jnz short do_read_chs
    ; jmp short disk_error


disk_error:
    mov si, disk_error_msg
    jmp short print_error

no_partition_found:
    mov si, no_partition_found_msg
    jmp short print_error

lba_too_high_error:
    mov si, lba_too_high_error_msg
    ; jmp short print_error

print_error:
    lodsb
    cmp al, 0x10
    je short .done
    mov ah, 0eh
    mov bh, 0
    mov bl, 0x07
    int 10h
    jmp short print_error

.done:
    cli
.halt:
    hlt
    jmp short .halt


partition_no_marker:
    mov byte [retry_count], RETRIES

    mov si, [partition]
    mov byte [si], 0x00

    jmp near init_check_loop


retry_count db RETRIES

;
; Overlap between dap and chs conversion storage because only one is used
;
; dap_lba: ; 8 bytes
;     dd 0
;     dd 0
;
; spt dw 0
; heads dw 0
; sector_result db 0
; head_result db 0
; cyl_result dw 0
;

no_partition_found_msg db "No bootable partition found", 0x10
disk_error_msg db "Disk error", 0x10
lba_too_high_error_msg db "Partition LBA too high", ; using the 0x10 of the DAP, because if this error message is used, we're using CHS, which means the DAP has not been used

dap:
    db 0x10
    db 0
    dw 1
    dw 0x7C00
    dw 0x0000
dap_lba: ; dap: 8 bytes
spt: ; chs: 2 bytes
    db 0
    db 0
heads: ; chs: 2 bytes
    db 0
    db 0
sector_result: ; chs: 1 byte
    db 0
head_result: ; chs: 1 byte
    db 0
cyl_result: ; chs: 2 bytes
    db 0
    db 0



times 446 - ($ - $$) db 0



partition1:
    times 16 db 0
partition2:
    times 16 db 0
partition3:
    times 16 db 0
partition4:
    times 16 db 0



dw 0xAA55
