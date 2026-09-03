; tableview.inc -- CARTRIDGE BANK 2. All eight seats at once.
;
; The hand view answers "what am I looking at"; this answers "who is still in,
; and what did they just do". They cannot share a screen: the VDC gives 12
; single characters and 4 quad groups of 4, and 28 glyphs will not carry eight
; seats AND a five-card hand. So this is the other half of the game, in its own
; cartridge bank, on the same three rows:
;
;   y=8    M A R T Y B O B    eight seat initials, on the quad lattice.
;                             Whose turn it is goes yellow.
;   y=28      ^                a sprite caret under the seat the stick is on --
;                             up and down still browse from here, and there is
;                             no room on this screen to spell out which
;   y=40   C R - F C - K A    what each of them last did, directly underneath
;   y=72   P 4 2 5   $ 4 9 8  the pot, and your purse
;
; It renders and hands control straight back: bank 3 keeps the poll loop, the
; countdown and the turn edge, so nothing here has to know about any of that.
; The way back is X_RET rather than a return address, because crossing banks is
; a jump in both directions.
;
; Everything it calls is either in the low bank (num3, show_char, show_quads,
; col_plain, gfx_on/off) or in the mailbox page every bank carries (the rx_*
; reply cursor). It needs no part of net.inc or url.inc, because it never
; transacts -- it reads the window bank 3's last poll left behind.

    org 8CAh                    ; straight after the mailbox, which every bank has

tv_render:
    mov a,#P1_SEL_XRAM
    outl p1,a

    ; ---- the bottom line: the pot, and your purse ----
    call rx_home
    mov a,#GAME_POT
    call rx_adv
    call rx_u16
    mov r1,#V_BOT
    mov a,#'P'
    mov @r1,a
    inc r1
    call num3
    sel mb1
    mov a,#GAME_PLAYERS+PL_PURSE-GAME_POT-2     ; 84 -> 174, seat 0 being you
    call rx_adv
    call rx_u16
    mov r1,#V_BOT+4
    mov a,#'$'
    mov @r1,a
    inc r1
    call num3
    sel mb1

    ; ---- the seats ----
    mov r1,#V_TOP               ; an initial each, on the top quad line
    mov a,#PL_NAME
    call tv_walk
    mov r1,#V_TEXT              ; and the first letter of each one's last move
    mov a,#PL_MOVE
    call tv_walk
    mov r1,#V_TEXT+TVSEATS      ; the four singles the seats do not use
    mov r2,#TEXTLEN-TVSEATS
    mov a,#' '
tv_pad:
    mov @r1,a
    inc r1
    djnz r2,tv_pad

    ; ---- paint ----
    mov a,#P1_SEL_VDC
    outl p1,a
    call gfx_off
    sel mb1
    call show_quads             ; initials at y=8, pot and purse at y=72
    sel mb1
    call tv_moves
    call tv_mark
    call tv_spot
    call gfx_on
    sel mb1

tv_back:
    mov a,#P1_SEL_XRAM
    outl p1,a
    mov r0,#X_RET
    movx a,@r0
    mov r1,a
    mov a,#P1VDC(3)
    jmp far_go

    org 940h
; ---------------------------------------------------------------------------
; tv_walk: one byte out of each of the eight seats, into the buffer at r1.
; A = that byte's offset inside the player record.
;
; Seats past playerCount get a blank rather than whatever the window holds
; there, so an empty chair reads as empty and not as a stale one.
; ---------------------------------------------------------------------------
tv_walk:
    mov r4,a
    call rx_home
    mov a,#128                  ; rx_adv takes at most 128 at a time
    call rx_adv
    mov a,#GAME_PLAYERS-128
    call rx_adv
    mov a,r4
    call rx_adv                 ; onto the field in seat 0
    mov r0,#X_PC
    movx a,@r0
    mov r3,a
    mov r2,#TVSEATS
tv_w:
    mov a,r3
    jz tv_wgap
    call rx_next
    mov @r1,a
    dec r3
    mov a,#PLAYER_STRIDE-1
    call rx_adv
    jmp tv_wnext
tv_wgap:
    mov a,#' '
    mov @r1,a
tv_wnext:
    inc r1
    djnz r2,tv_w
    ret

    org 980h
; ---------------------------------------------------------------------------
; tv_moves: the middle row, eight glyphs at the quad lattice's 16-pixel pitch so
; each lands under its own seat. The remaining four singles are parked at the
; left edge as blanks -- every single character has to be given a position, and
; an unset one draws wherever the last screen left it.
; ---------------------------------------------------------------------------
tv_moves:
    call col_plain
    sel mb1
    mov r6,#CARDY
    mov r7,#CARDB
    mov r0,#vdc_char0
    mov r1,#V_TEXT
    mov r3,#TVX
    mov r2,#TVSEATS
tv_ml:
    mov a,@r1
    inc r1
    call show_char              ; advances r3 by 8...
    sel mb1
    mov a,r3
    add a,#8                    ; ...and 8 more makes the 16-pixel pitch
    mov r3,a
    djnz r2,tv_ml
    mov r3,#MARKLX
    mov r2,#TEXTLEN-TVSEATS
tv_mp:
    mov a,#' '
    call show_char
    sel mb1
    djnz r2,tv_mp
    ret

    org 9C0h
; ---------------------------------------------------------------------------
; tv_mark: paint the seat whose turn it is yellow, in place.
;
; quad0 and quad1 are contiguous, $40-$4F and $50-$5F, and every glyph's colour
; byte is the fourth of its four -- so across both groups seat i's colour sits
; at $43 + 4i, with no special case at the join. Only the colour changes here,
; so the 9th pointer bit sharing that byte has to be read back and kept.
; ---------------------------------------------------------------------------
tv_mark:
    mov a,#P1_SEL_XRAM
    outl p1,a
    mov r0,#X_ACT
    movx a,@r0
    mov r2,a
    mov a,#P1_SEL_VDC
    outl p1,a
    mov a,r2
    add a,#0F8h                 ; -8: carry means this is not a seat at all,
    jc tv_noturn                ; which is what $FF means between turns
    mov a,r2
    rl a
    rl a                        ; i*4; the seat is 0..7 so nothing rotates out
    add a,#vdc_quad0+3
    mov r0,a
    movx a,@r0
    anl a,#001h
    orl a,#col_chr_yellow
    movx @r0,a
tv_noturn:
    ret

    org 0A00h
; ---------------------------------------------------------------------------
; tv_spot: sprite 0, a caret under the spotlit seat.
;
; The hand view answers "which seat am I on" with a digit at the edge of the
; card row; here there is no spare glyph to say it with, and the spotlight still
; moves under up and down. So it is the one job on this screen worth a sprite.
;
; Four rows of shape rather than eight: a sprite draws two scanlines per row, so
; eight would run from 28 to 43 and into the move row at 40. The caret is
; symmetric, which sidesteps the other sprite trap -- the VDC clocks a sprite
; byte out LSB first, mirrored against the way characters read.
;
; The hand view does not have to undo this: draw clears the sprite each time it
; paints, so flipping back takes the caret with it.
; ---------------------------------------------------------------------------
tv_spot:
    mov r0,#vdc_spr0_shape
    mov a,#018h                 ;    ..##..
    movx @r0,a
    inc r0
    mov a,#03Ch                 ;   .####.
    movx @r0,a
    inc r0
    mov a,#07Eh                 ;  ######
    movx @r0,a
    inc r0
    mov a,#0FFh                 ; ########
    movx @r0,a
    inc r0
    mov r2,#4                   ; and four blank rows: see above
    call vfill
    sel mb1

    mov r0,#vdc_spr0_ctrl
    mov a,#TVSPRY
    movx @r0,a
    inc r0
    mov r1,#V_SPOT              ; internal RAM reads whatever P1 says, so the
    mov a,@r1                   ; spotlight is legible mid-paint
    rl a
    rl a
    rl a
    rl a                        ; x16, the seat pitch; seat 7 lands at x=136
    add a,#TVX
    movx @r0,a
    inc r0
    mov a,#col_spr_white
    movx @r0,a
    ret
