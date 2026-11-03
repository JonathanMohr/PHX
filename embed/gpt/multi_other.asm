[bits 16]
[org 0x0800]
[cpu 8086]

entry:
    pop word [pnp.offset]
    pop word [pnp.segment]
    pop word [print_error]
    pop word [read_sector_raw]

    ; TODO

    call [print_error]
    dw 0x99d0, 0x9690, 0x96a6, 0x3934, 0x99c8, 0xd348, 0xbcf4, 0x94c0, 0x3134, 0x7174, 0x99c8, 0xd05c, 0x74e4, 0x20a6, 0x7475


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


pnp:
    .segment dw 0
    .offset dw 0

read_sector_raw dw 0
print_error dw 0

times 512 - ($ - $$) db 0
