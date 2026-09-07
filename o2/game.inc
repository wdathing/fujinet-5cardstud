; game.inc -- CARTRIDGE BANK 3. The poll loop and the move UI.
;
; Table select used to be here too; it is lobby.inc in bank 0 now. What is left
; is everything that runs once you are sitting at a table, with url.inc, net.inc
; and render.inc underneath it in this same bank.

    org 0C0Bh                   ; The loop, the poll and the paint decision all
                                ; reach each other with page-relative jumps, so
                                ; the three of them have to share page $C00.
                                ; They now start half a page earlier, in the
                                ; space the lobby left when it moved to bank 0.
; ===========================================================================
; The poll loop.
; ===========================================================================
game_start:
    mov a,#P1_SEL_XRAM          ; first, not halfway down: the highlighted move
    outl p1,a                   ; lives in external RAM now, and ts_join arrives
                                ; here with the VDC on the bus -- where a MOVX to
                                ; $27 would land in a character record instead
    mov r0,#V_SPOT
    clr a
    mov @r0,a
    mov r0,#X_MVSEL
    clr a
    movx @r0,a
    mov r0,#V_POLL
    clr a
    mov @r0,a
    mov r0,#V_HAVEMV
    clr a
    mov @r0,a
    mov r0,#X_PREVACT
    mov a,#0FFh
    movx @r0,a
    mov r0,#X_PREVRD            ; 0, not the round we are joining at: whatever
    clr a                       ; lastResult holds now belongs to a hand that
    movx @r0,a                  ; finished before we sat down
    mov r0,#X_PC
    mov a,#1                    ; until the first reply says otherwise
    movx @r0,a

gl_loop:
    mov a,#P1_SEL_VDC
    outl p1,a
    call wait_frame
    sel mb1
    call input_get
    sel mb1
    mov r5,a
    jz gl_tick
    mov r3,#0
    xrl a,#J_UP
    jz gl_spot
    mov r3,#1
    mov a,r5
    xrl a,#J_DOWN
    jz gl_spot
    mov a,r5                    ; left, right and fire all do nothing else while
    add a,#0FDh                 ; you are waiting -- J_LEFT/RIGHT/FIRE are 3/4/5,
    jc gl_hop                   ; so one add separates them from up and down --
gl_tick:                        ; so any of the three flips the view
    mov r0,#V_POLL
    mov a,@r0
    jz gl_poll
    dec a
    mov @r0,a
    jmp gl_loop
gl_hop:                         ; two bytes into an unrestricted JMP: gl_flip is
    jmp gl_flip                 ; in the next page and a conditional cannot reach

; Spotlight browse. The reply window still holds the last poll's struct, so
; moving the spotlight is a re-read and a redraw with no round trip at all --
; the cards of whoever you point at appear on the next frame.
gl_spot:
    call spot_move
    call read_state
    mov r5,#1
    mov a,#1                    ; whichever view paints, come back to gl_loop
    jmp gl_paint

    org 0C62h
gl_poll:
    mov r0,#V_HAVEMV            ; a staged move goes out as /move/XX, and the
    mov a,@r0                   ; reply to it is the next state. The flag is 0
    inc a                       ; or 1 and the URL selector 1 or 2, so this is
    mov r3,a                    ; the whole choice
    clr a
    mov @r0,a
    call api_call
    jnz gl_got
    jmp gl_fail
gl_got:
    ; A mailbox ACK only means the cart relayed something. An HTTP error page
    ; would sail through it and be read as a Game struct, so the length is
    ; checked before anything is believed -- first against the fixed prefix,
    ; then, once playerCount is known, against the exact size it implies.
    mov r3,#0
    mov r4,#GAME_MINLEN
    call rx_atleast
    jnz gl_len
    jmp gl_fail
gl_len:
    call read_state
    call check_state
    jnz gl_ok
    jmp gl_fail
gl_ok:
    ; The table view shows neither the hands nor the message, so the whole
    ; round-edge branch below is the hand view's business: paint over in bank 2
    ; and come straight back at gl_turn, which is what has to run either way.
    mov a,#P1_SEL_XRAM
    outl p1,a
    mov r0,#X_MODE
    movx a,@r0
    jz gl_hand
    mov r5,#1
    mov a,#2                    ; back at gl_turn
    jmp gl_paint
gl_hand:
    ; Draw BEFORE looking at lastResult. The poll carrying the showdown message
    ; is the same one carrying the revealed hands: by the next poll the server
    ; has usually started a new hand and reset them, so showing the message
    ; first means the reveal is drawn after it has already gone.
    mov r5,#1
    call draw
    sel mb1
    ; The end of a hand, on the round edge: 5 is the showdown, and a drop means
    ; the next hand has been dealt. Either way lastResult is the line the server
    ; wants shown, and this is the poll that still has the revealed hands in the
    ; window behind it.
    mov a,#P1_SEL_XRAM
    outl p1,a
    mov r0,#X_ROUND
    movx a,@r0
    mov r3,a
    mov r0,#X_PREVRD
    movx a,@r0
    mov r2,a
    mov a,r3
    movx @r0,a
    xrl a,r2
    jz gl_turn                  ; the round did not move
    mov a,r3
    xrl a,#5
    jz gl_show
    mov a,r2                    ; a drop means a new hand was dealt
    cpl a
    add a,r3
    jc gl_turn
gl_show:
    mov a,#P1_SEL_VDC
    outl p1,a
    mov a,#(cue_select-0E00h)&0FFh
    call snd
    call show_result
    call read_state             ; put the table back after the message
    mov r5,#1
    call draw
    sel mb1

    org 0CFCh
gl_turn:
    mov a,#P1_SEL_XRAM
    outl p1,a
    mov r0,#X_ACT               ; your turn is activePlayer 0, and only when you
    movx a,@r0                  ; are in the hand rather than watching it
    jnz gl_skip
    mov r0,#X_VIEW
    movx a,@r0
    jz gl_mine
gl_skip:
    jmp gl_wait                 ; a short hop into a JMP: a conditional here has
gl_mine:                        ; to reach across the whole move UI entry
    mov r0,#X_MODE              ; the move UI is drawn on the hand view, so
    clr a                       ; taking a turn ends the table view
    movx @r0,a
    call move_ui
gl_wait:
    mov a,#P1_SEL_XRAM
    outl p1,a
    mov r0,#X_ACT               ; remember whose turn it was, so the cue fires
    movx a,@r0                  ; on the edge and not on every poll that still
    mov r3,a                    ; says it is yours
    mov r0,#X_PREVACT
    mov a,r3
    movx @r0,a
    mov r0,#V_HAVEMV            ; a staged move goes out at once; anything else
    mov a,@r0                   ; waits out the poll interval
    jnz gl_now
    mov a,#POLL_FRAMES
gl_now:
    mov r0,#V_POLL
    mov @r0,a
    jmp gl_loop
gl_fail:
    mov r0,#V_POLL
    mov a,#FAIL_FRAMES
    mov @r0,a
    jmp gl_loop

    org 0B70h                   ; In the space the lobby vacated. Both callers
                                ; reach it with an unrestricted JMP, and its own
                                ; conditional stays inside page $B00.
; ---------------------------------------------------------------------------
; gl_paint: A = the bank 0 entry to carry on at (1 gl_loop, 2 gl_turn),
; r5 = 0/1 as for draw.
;
; A JUMP and never a call. In the table view the painting happens in bank 2, and
; a cartridge bank switch is one-way -- so "where to carry on" is data, not a
; return address, and the hand path goes through the same dispatcher so that
; both routes leave by exactly one door.
;
; The entry is reloaded from X_RET after draw rather than kept in a register:
; draw walks the buffers with r1 and glyph_ptr scribbles on r4 and r5.
; ---------------------------------------------------------------------------
gl_paint:
    mov r4,a
    mov a,#P1_SEL_XRAM
    outl p1,a
    mov r0,#X_RET
    mov a,r4
    movx @r0,a
    mov r0,#X_MODE
    movx a,@r0
    jz gp_hand
    mov r1,#0                   ; bank 2 has one entry
    mov a,#P1VDC(2)
    jmp far_go
gp_hand:
    call draw
    sel mb1
    mov a,#P1_SEL_XRAM
    outl p1,a
    mov r0,#X_RET
    movx a,@r0
    mov r1,a
    jmp bank_entry

; gl_flip: swap the view. The repaint is the poll loop's next redraw, which is
; at most a frame away, so this only has to move the flag and click.
gl_flip:
    mov a,#P1_SEL_XRAM
    outl p1,a
    mov r0,#X_MODE
    movx a,@r0
    xrl a,#1
    movx @r0,a
    mov a,#P1_SEL_VDC
    outl p1,a
    mov a,#(cue_cursor-0E00h)&0FFh
    call snd
    mov r5,#1
    call read_state
    mov a,#1                    ; back at gl_loop
    jmp gl_paint

; spot_move: r3 = 0 back / 1 forward. Moves the spotlight, wrapping on the
; player count.
;
; Sits in the hole cycle left when cycle went resident, rather than beside the
; rest of the game loop: it grew by the one byte of its own `call cycle` restore
; and there was nothing left at $D2B. It has no conditional jumps of its own, so
; unlike almost everything else here it can be placed anywhere in this half.
    org 0B08h                   ; spot_move follows gl_paint in the source but
                                ; not in memory; both are pinned
spot_move:
    mov a,#P1_SEL_XRAM
    outl p1,a
    mov r0,#X_PC
    movx a,@r0
    mov r2,a
    mov r0,#V_SPOT
    mov a,@r0
    call cycle
    sel mb1
    mov r0,#V_SPOT
    mov @r0,a
    mov a,#P1_SEL_VDC
    outl p1,a
    mov a,#(cue_cursor-0E00h)&0FFh
    call snd
    ret

    org 0D42h
; ===========================================================================
; move_ui: your turn. The bottom line becomes the move name and a countdown;
; left/right choose, the button submits, up/down still move the spotlight.
;
; The countdown starts from the server's moveTime, which is already net of a
; round-trip grace period, and runs down locally rather than re-polling once a
; second. Running out submits whatever is highlighted, matching the C clients.
; ===========================================================================
move_ui:
    mov r0,#X_MVCNT
    movx a,@r0
    jnz mu_have
    ret                         ; a turn carrying no moves is a malformed reply
mu_have:
    mov r3,a
    mov r0,#X_MVSEL
    clr a
    movx @r0,a
    mov a,r3
    xrl a,#1
    jz mu_cue
    mov r0,#X_MVSEL             ; default to the second move, never Fold
    mov a,#1
    movx @r0,a
mu_cue:
    mov r0,#X_ACT               ; only on the edge: move_ui is re-entered on
    movx a,@r0                  ; every poll that is still your turn
    mov r3,a
    mov r0,#X_PREVACT
    movx a,@r0
    xrl a,r3
    jz mu_nocue
    mov a,#P1_SEL_VDC
    outl p1,a
    mov a,#(cue_turn-0E00h)&0FFh
    call snd
mu_nocue:
    mov a,#P1_SEL_XRAM
    outl p1,a
    mov r0,#X_FRAME
    clr a
    movx @r0,a

    org 0D6Fh
mu_show:
    call load_move
    mov r5,#1
    call draw
    sel mb1
mu_loop:
    mov a,#P1_SEL_VDC
    outl p1,a
    call wait_frame
    sel mb1
    call input_get
    sel mb1
    mov r5,a
    jnz mu_act                  ; dispatch the stick BEFORE ticking the clock:
                                ; the once-a-second redraw runs through
                                ; glyph_ptr, which owns r5

; The countdown starts from the server's moveTime, which is already net of a
; round-trip grace period, and runs down here rather than re-polling once a
; second. Running out submits whatever is highlighted, matching the C clients.
    mov a,#P1_SEL_XRAM
    outl p1,a
    mov r0,#X_FRAME
    movx a,@r0
    inc a
    movx @r0,a
    xrl a,#SECOND
    jnz mu_loop
    mov r0,#X_FRAME
    clr a
    movx @r0,a
    mov r0,#X_TIME
    movx a,@r0
    jz mu_submit
    dec a
    movx @r0,a
    jz mu_submit
    call put_time               ; only the two countdown cells changed
    mov a,#P1_SEL_VDC
    outl p1,a
    call gfx_off
    sel mb1
    call show_quads
    sel mb1
    call gfx_on
    sel mb1
    jmp mu_loop

mu_act:
    xrl a,#J_FIRE
    jz mu_submit
    mov a,r5
    add a,#0FDh                 ; J_LEFT/J_RIGHT become 0/1: the cycle direction
    jc mu_move
    mov r3,#0                   ; up steps the spotlight back
    mov a,r5
    xrl a,#J_UP
    jz mu_spot
    mov r3,#1
mu_spot:
    call spot_move
    call read_state
    jmp mu_show
mu_move:
    mov r3,a
    mov a,#P1_SEL_XRAM
    outl p1,a
    mov r0,#X_MVCNT
    movx a,@r0
    mov r2,a
    mov r0,#X_MVSEL
    movx a,@r0
    call cycle
    sel mb1
    mov r0,#X_MVSEL
    movx @r0,a
    mov a,#P1_SEL_VDC
    outl p1,a
    mov a,#(cue_cursor-0E00h)&0FFh
    call snd
    jmp mu_show
mu_submit:
    mov a,#P1_SEL_VDC
    outl p1,a
    mov a,#(cue_select-0E00h)&0FFh
    call snd
    call load_move              ; stages X_MVA/X_MVB for the /move/XX path
    mov r0,#V_HAVEMV
    mov a,#1
    mov @r0,a
    ret
