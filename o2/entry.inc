; entry.inc -- boot, the joystick name picker, the appkey, and the result line.
;
; CARTRIDGE BANK 1, high half. This is the whole of bank 1: the console boots
; into it, and it hands off to bank 0's table_select when it has a name.
;
; It used to live in the low bank at $5EE, beside the show_*/input_get calls it
; is mostly made of. Moving it here is what freed 498 bytes of low bank for the
; resident set. The shift is exactly $300 -- a whole number of pages -- so every
; routine keeps its offset inside its own 256-byte page and not one of the
; page-relative conditional jumps in here had to be re-checked.
;
; The MB restores all flipped direction as a result. Calls into the mailbox are
; now intra-bank and carry nothing; calls down into fujilib are now cross-bank
; and carry a `sel mb1`. tools/checkmb.py checks both halves of that: a missing
; restore, and a stray one left behind by the move.

    org 08EEh

; ---------------------------------------------------------------------------
; boot: check the cartridge answers, get a name, then pick a table.
; ---------------------------------------------------------------------------
boot:
    mov a,#(s_title-0E00h)&0FFh ; start already blanked the quad lines
    call say
    sel mb1
    mov a,#FN_R_MAGIC0&0FFh     ; 'F' -- is a FujiNet cart even in the slot
    call fn_read
    xrl a,#'F'
    jnz bt_none                 ; the hop is two bytes; the JMP has no page limit
    jmp bt_cart
bt_none:
    mov a,#(s_nocart-0E00h)&0FFh
    call say
    sel mb1
bt_stop:
    jmp bt_stop

bt_cart:
    ; The name key (creator 1 / app 1 / key 0) is the username SHARED by every
    ; FujiNet client on this server, so it can hold anything at all -- a space,
    ; punctuation, another game's leftovers. It goes into a query string
    ; unescaped, and one stray reserved character there is what gets a request
    ; rejected with an error page that then gets read as a Game struct. This
    ; screen can only produce A-Z and 0-9, so anything else is untrusted.
    mov a,#P1_SEL_XRAM
    outl p1,a
    mov a,#AK_MODE_READ
    call ak_open
    jz bt_entry
    call ak_read_name           ; A = the stored length
    mov r0,#X_OK
    movx @r0,a
    jz bt_shut                  ; nothing stored: the picker, then
    call name_valid             ; before ak_close, whose fn_begin clears r3
    mov r0,#X_OK
    movx @r0,a
bt_shut:
    call ak_close
    mov r0,#X_OK
    movx a,@r0
    jnz bt_table
bt_entry:
    call name_entry
bt_table:
    mov a,#P1VDC(0)             ; table_select and the whole client live in
    jmp far_go                  ; bank 0; this is the hand-off

    org 0A25h
; name_valid: A = length, X_NAME holds the bytes, P1 = P1_SEL_XRAM.
; A = 1 only if every byte is A-Z or 0-9.
name_valid:
    mov r2,a
    mov r1,#X_NAME
nv_l:
    movx a,@r1
    mov r4,a
    add a,#0D0h                 ; -'0'
    jnc nv_bad
    add a,#0F6h                 ; -10: no carry means it was a digit
    jnc nv_ok
    mov a,r4
    add a,#0BFh                 ; -'A'
    jnc nv_bad
    add a,#0E6h                 ; -26: no carry means it was a letter
    jnc nv_ok
nv_bad:
    clr a
    ret
nv_ok:
    inc r1
    djnz r2,nv_l
    mov a,#1
    ret

    org 0A43h
; code_ascii: entry code -> ASCII. 0-25 are A-Z, 26-35 are 0-9, 36 is a blank.
; Uses r4 only, so a caller can walk two buffers with r0 and r1 across it.
code_ascii:
    mov r4,a
    add a,#0E6h                 ; -26
    jc ca_dig
    mov a,r4
    add a,#'A'
    ret
ca_dig:
    mov r4,a
    add a,#0F6h                 ; -10
    jc ca_blank
    mov a,r4
    add a,#'0'
    ret
ca_blank:
    mov a,#' '
    ret

    org 0A58h
; ===========================================================================
; name_entry: the 8 characters go on the TOP quad line, with a caret on the
; singles row underneath.
;
; That pairing is not arbitrary. quad0 at x=24 and quad1 at x=88 are 64 apart,
; exactly one group's span, so the eight glyphs sit on an even 16-pixel pitch
; and glyph i is at x = 24 + 16*i for all eight of them. A single character
; carries its own X register, so the caret goes at that same x one row down and
; lines up exactly.
;
; Up/down cycle the character, left/right move the caret, the button accepts.
; ===========================================================================
name_entry:
    mov a,#P1_SEL_XRAM
    outl p1,a
    mov r1,#X_NEBUF
    mov r2,#8
ne_init:
    mov a,#36                   ; blank
    movx @r1,a
    inc r1
    djnz r2,ne_init
    mov r1,#X_NECUR
    clr a
    movx @r1,a
    mov r1,#V_BOT
    mov r2,#QUADLEN
    mov a,#(s_name-0E00h)&0FFh
    call str_fill
    sel mb1

ne_loop:
    call ne_draw
ne_wait:
    call wait_frame
    sel mb1
    call input_get
    sel mb1
    mov r5,a
    jz ne_wait
    xrl a,#J_FIRE
    jz ne_accept
    mov a,#P1_SEL_XRAM
    outl p1,a
    mov a,r5
    add a,#0FDh                 ; J_LEFT and J_RIGHT fall out as 0 and 1, which
    jc ne_cursor                ; is exactly cycle's direction argument
    mov r1,#X_NECUR             ; up or down: the character under the caret
    movx a,@r1
    add a,#X_NEBUF
    mov r1,a
    mov r2,#37                  ; A-Z, 0-9 and a blank
    mov r3,#0                   ; J_UP steps back through the alphabet
    mov a,r5
    xrl a,#J_UP
    jnz ne_ch
    mov r3,#1
ne_ch:
    movx a,@r1
    call cycle
    sel mb1
    movx @r1,a
    jmp ne_click
ne_cursor:
    mov r3,a                    ; 0 = left, 1 = right
    mov r1,#X_NECUR
    movx a,@r1
    mov r2,#8
    call cycle
    sel mb1
    movx @r1,a
ne_click:
    mov a,#P1_SEL_VDC
    outl p1,a
    mov a,#(cue_cursor-0E00h)&0FFh
    call snd
    jmp ne_loop

    org 0BB5h
; ne_draw: the eight characters on the top line, the caret beneath the cursor.
ne_draw:
    mov a,#P1_SEL_XRAM
    outl p1,a
    mov r1,#X_NEBUF
    mov r0,#V_TOP
    mov r2,#8
ned_l:
    movx a,@r1
    call code_ascii
    mov @r0,a
    inc r0
    inc r1
    djnz r2,ned_l
    mov r1,#X_NECUR
    movx a,@r1
    mov r5,a
    mov a,#P1_SEL_VDC
    outl p1,a
    call gfx_off
    sel mb1
    mov r0,#vdc_char0           ; a zeroed character record draws nothing, so
    mov r2,#48                  ; this clears the whole singles row at once
    call vfill
    sel mb1
    call show_quads
    sel mb1
    mov a,r5                    ; caret at x = QLX + 16*cursor
    swap a
    anl a,#0F0h
    add a,#QLX
    mov r3,a
    mov r0,#vdc_char0
    mov r6,#CARDY
    mov r7,#CARDB
    mov a,#'-'
    call show_char
    sel mb1
    call gfx_on
    sel mb1
    ret

    org 0AEBh
ne_accept:
    mov a,#P1_SEL_XRAM
    outl p1,a
    mov r1,#X_NEBUF+7           ; trim trailing blanks
    mov r2,#8
nea_l:
    movx a,@r1
    xrl a,#36
    jnz nea_have
    dec r1
    djnz r2,nea_l
    jmp ne_loop                 ; all blank is not a name
nea_have:
    mov a,r2                    ; r2 counted down to the used length
    mov r1,#X_NECUR
    movx @r1,a
    mov r3,a
    mov r1,#X_NEBUF
    mov r0,#X_NAME
    mov a,r3
    mov r2,a
nea_c:
    movx a,@r1
    call code_ascii
    movx @r0,a
    inc r0
    inc r1
    djnz r2,nea_c
    clr a
    movx @r0,a                  ; NUL-terminate; u_xstr stops there

    mov a,#AK_MODE_WRITE
    call ak_open
    jz nea_done                 ; no appkey storage: play on, just do not
    mov r1,#X_NECUR             ; expect the name back on the next boot
    movx a,@r1
    call ak_write_name
    call ak_close
nea_done:
    ret

    org 0C1Fh
; ---------------------------------------------------------------------------
; AppKey. Callers must have P1 = P1_SEL_XRAM.
;
; The wire struct (fujiDevice.h's packed `struct appkey`) is SIX bytes:
; creator_lo, creator_hi, app, key, mode, reserved. Sending only five leaves the
; firmware's transaction_get() waiting for a byte that never arrives, which
; reads back as a timeout rather than as a protocol error -- easy to misdiagnose,
; and exactly what happened on the Intellivision.
; ---------------------------------------------------------------------------

; ak_open: A = mode (0 read, 1 write) for the name key. Returns A = 1 on ACK.
ak_open:
    mov r3,a
    mov r6,#FUJI_DEVICEID
    mov r2,#0
    mov a,#FUJICMD_OPEN_APPKEY
    call fn_begin
    mov a,#AK_CREATOR_LO
    call fn_push
    mov a,#AK_CREATOR_HI
    call fn_push
    mov a,#AK_APP
    call fn_push
    mov a,#AK_KEY_NAME
    call fn_push
    mov a,r3
    call fn_push
    clr a                       ; reserved -- omitting it hangs the firmware
    call fn_push
    mov a,#6
    mov r5,#0
    call fn_txlen
    call fn_fire
    call fn_ack
    ret

ak_close:
    mov r6,#FUJI_DEVICEID
    mov r2,#0
    mov r3,#0
    mov a,#FUJICMD_CLOSE_APPKEY
    call fn_begin
    call fn_fire
    ret

    org 0C6Bh
; ak_read_name: read the name key into X_NAME, at most 8 bytes plus a NUL.
; Returns A = the number of bytes stored.
;
; This transport prepends a 2-byte little-endian length ahead of the data:
; unlike a network READ, an appkey READ takes no length parameter, so the
; firmware makes the reply self-describing instead (rs232Fuji::
; fujicore_read_app_key). RXLEN covers the prefix and the data together, so the
; embedded prefix is the real length and the data starts two bytes in.
ak_read_name:
    mov r6,#FUJI_DEVICEID
    mov r2,#0
    mov r3,#0
    mov a,#FUJICMD_READ_APPKEY
    call fn_begin
    call fn_fire
    call fn_ack
    jnz akr_ok
    clr a
    ret
akr_ok:
    mov a,#FN_R_DATA&0FFh
    call fn_read
    mov r3,a
    mov a,#(FN_R_DATA+1)&0FFh   ; any high byte at all is far past the 8 bytes
    call fn_read                ; this buffer holds
    jz akr_clamp
    mov r3,#8
    jmp akr_go
akr_clamp:
    mov a,r3
    add a,#0F8h                 ; -8: carry means 8 or more
    jnc akr_go
    mov r3,#8
akr_go:
    mov a,r3
    jnz akr_copy
    ret                         ; empty key: A is already 0
akr_copy:
    mov a,r3
    mov r2,a
    mov r1,#X_NAME
    mov r4,#(FN_R_DATA+2)&0FFh  ; skip the length prefix
akr_l:
    mov a,r4
    call fn_read
    mov r5,a                    ; the byte; r1 is the destination
    mov a,r1
    mov r0,a
    mov a,r5
    movx @r0,a
    inc r1
    inc r4
    djnz r2,akr_l
    mov a,r1                    ; NUL-terminate at the stored length
    mov r0,a
    clr a
    movx @r0,a
    mov a,r3
    ret

    org 0CB9h
; ak_write_name: write the first A bytes of X_NAME as the key payload.
ak_write_name:
    mov r3,a
    mov r6,#FUJI_DEVICEID
    mov r2,#0
    mov a,#FUJICMD_WRITE_APPKEY
    call fn_begin
    mov r1,#X_NAME
    mov a,r3
    mov r2,a
awn_l:
    mov a,r1
    mov r0,a
    movx a,@r0
    call fn_push
    inc r1
    djnz r2,awn_l
    mov a,r3
    mov r5,#0
    call fn_txlen
    call fn_fire
    call fn_ack
    ret
