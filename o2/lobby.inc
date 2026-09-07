; lobby.inc -- CARTRIDGE BANK 0. Picking a table.
;
; Split out of game.inc when bank 0 ran out of room: the lobby and the game loop
; both need the whole network stack under them, and together with it they did not
; fit in one 3K bank. They are also the two halves of the program that never run
; at the same time, so they were the obvious cut.
;
; Bank 0 is therefore url.inc + net.inc + this, and bank 3 is url.inc + net.inc +
; render.inc + the game loop. The duplicated network stack is what a bank is for.

    org 0B65h

; ===========================================================================
; table_select: one table at a time -- its name on the singles, its "cur / max"
; on the bottom line. Up/down move, the button joins.
;
; One at a time because there is no other option: 12 single characters is one
; line of text and the quad line next to it is 8 characters at double pitch.
; The FujiNet browser cart reached the same conclusion about directories.
; ===========================================================================
table_select:
    mov a,#(s_pick-0E00h)&0FFh
    call say
    sel mb1
    mov r1,#V_TOP
    mov r2,#2*QUADLEN
    mov a,#(s_blank-0E00h)&0FFh
    call str_fill
    sel mb1
    mov r5,#0
    call draw
    sel mb1

ts_fetch:
    mov r3,#0                   ; /tables?bin=1
    call api_call
    jnz ts_got
    jmp ts_err
ts_got:
    call rx_home
    call rx_next
    mov r3,a
    jnz ts_cnt
    jmp ts_err
ts_cnt:
    add a,#0F6h                 ; -10: more tables than this screen will walk
    jnc ts_store
    mov r3,#10
ts_store:
    mov r0,#X_TBLCNT
    mov a,r3
    movx @r0,a
    ; Validate the length against what the count implies, exactly as the game
    ; loop does. An error page's first byte would otherwise be believed as a
    ; table count and walked as records.
    mov a,r3
    mov r2,a
    mov r3,#0
    mov r4,#1
ts_want:
    mov a,r4
    add a,#TABLE_STRIDE
    mov r4,a
    jnc ts_w1
    mov a,r3
    inc a
    mov r3,a
ts_w1:
    djnz r2,ts_want
    call rx_atleast
    jnz ts_ok
    jmp ts_err
ts_ok:
    mov r0,#X_TBLSEL
    clr a
    movx @r0,a

    org 0BB1h
ts_show:
    call ts_load
    mov r5,#0
    call draw
    sel mb1
ts_wait:
    call wait_frame
    sel mb1
    call input_get
    sel mb1
    mov r5,a
    jnz ts_key
    jmp ts_wait
ts_key:
    xrl a,#J_FIRE
    jz ts_join
    mov r3,#0                   ; up steps back through the list
    mov a,r5
    xrl a,#J_UP
    jz ts_step
    mov r3,#1
    mov a,r5
    xrl a,#J_DOWN
    jz ts_step
    jmp ts_wait
ts_step:
    mov a,#P1_SEL_XRAM
    outl p1,a
    mov r0,#X_TBLCNT
    movx a,@r0
    mov r2,a
    mov r0,#X_TBLSEL
    movx a,@r0
    call cycle
    sel mb1
    mov r0,#X_TBLSEL
    movx @r0,a
    mov a,#P1_SEL_VDC
    outl p1,a
    mov a,#(cue_cursor-0E00h)&0FFh
    call snd
    jmp ts_show

    org 0BF6h
ts_join:
    mov a,#P1_SEL_XRAM
    outl p1,a
    call ts_seek
    mov r1,#V_TOP               ; rx_copy writes internal RAM and the table id
    mov r2,#8                   ; has to end up in external -- stage it
    call rx_copy
    mov r1,#V_TOP
    mov r0,#X_TABLE
    mov r2,#8
tj_l:
    mov a,@r1
    movx @r0,a
    inc r0
    inc r1
    djnz r2,tj_l
    clr a
    movx @r0,a                  ; NUL-terminate; u_xstr stops there
    mov a,#P1_SEL_VDC
    outl p1,a
    mov a,#(cue_select-0E00h)&0FFh
    call snd
    mov r1,#0                   ; sit down: bank 3's first entry
    mov a,#P1VDC(3)
    jmp far_go

ts_err:
    mov a,#(s_err-0E00h)&0FFh
    call say
    sel mb1
    mov r2,#90
    call delay
    sel mb1
    jmp ts_fetch

; ts_seek: put the reply cursor on table X_TBLSEL's record: 1 + 36*i.
;
; A page further on than it used to be: ts_join above grew when joining became a
; hand-off into bank 3. Moving by a whole page keeps ts_seek's own conditionals
; at exactly the same offset inside their page.
    org 0D26h
ts_seek:
    call rx_home
    mov a,#1
    call rx_adv
    mov r0,#X_TBLSEL
    movx a,@r0
    jz tsk_done
    mov r2,a
    mov a,#TABLE_STRIDE
    call rx_skip
tsk_done:
    ret

; ts_load: the highlighted table's name and player count into the buffers.
ts_load:
    mov a,#P1_SEL_XRAM
    outl p1,a
    call ts_seek
    mov a,#TBL_NAME
    call rx_adv
    mov r1,#V_TEXT              ; 12 characters of a 21-byte name
    mov r2,#TEXTLEN
    call rx_copy
    mov a,#TBL_PLAYERS-TBL_NAME-TEXTLEN
    call rx_adv
    mov r1,#V_BOT               ; "cur / max", 6 literal bytes
    mov r2,#6
    call rx_copy
    mov r1,#V_BOT+6
    mov r2,#2
    mov a,#(s_blank-0E00h)&0FFh
    call str_fill
    sel mb1
    ret

