; render.inc -- reading the Game struct and painting the three lines.
;
; HIGH bank. Nothing is ever buffered: the reply sits in the cartridge's ROM
; window and is walked in place with the rx_* cursor, exactly as the
; Intellivision port reads out of FN_RX. The only copies are the 28 characters
; actually on screen.
;
; The struct is read in two passes, because the cursor only moves forward and
; the two things the top line needs -- the pot, and YOUR purse -- sit either
; side of everything the bottom line needs. Re-homing costs one mailbox register
; write, which is cheaper than any way of avoiding it.

    org 0A51h

; ---------------------------------------------------------------------------
; read_state: fill V_TOP, V_BOT and V_TEXT from the reply.
; Sets P1 itself: callers arrive from a draw as often as from a transaction.
; ---------------------------------------------------------------------------
read_state:
    mov a,#P1_SEL_XRAM
    outl p1,a
    call rx_home

    mov a,#GAME_ROUND           ; 81, straight past lastResult
    call rx_adv
    call rx_next
    mov r0,#X_ROUND
    movx @r0,a

    call rx_u16                 ; pot
    mov r1,#V_TOP
    mov a,#'P'
    mov @r1,a
    inc r1
    call num3
    sel mb1

    mov r1,#X_ACT               ; activePlayer, moveTime, viewing and
    mov r2,#4                   ; validMoveCount: wire offsets 84-87, in order,
rs_hdr:                         ; and consecutive in external RAM to suit
    call rx_next
    movx @r1,a
    inc r1
    djnz r2,rs_hdr

    mov a,#GAME_PLAYERCOUNT-GAME_VALIDMOVES     ; 65 -> offset 153
    call rx_adv
    call rx_next
    mov r0,#X_PC
    movx @r0,a

    mov a,#PL_PURSE             ; 20 into player 0's record: always you, per the
    call rx_adv                 ; server's rotation
    call rx_u16
    mov r1,#V_TOP+4
    mov a,#'$'
    mov @r1,a
    inc r1
    call num3
    sel mb1

    call seek_spot              ; --- pass two: the spotlighted player
    mov r1,#V_BOT               ; name, 4 characters of a 9-byte field
    mov r2,#4
    call rx_copy
    mov a,#PL_BET-4             ; 6 -> +10, straight past the status byte: a
    call rx_adv                 ; folded hand already reads as "??" on the card
    call rx_u16                 ; row, so the byte says nothing the cards do not
    mov r1,#V_BOT+4
    mov a,#' '
    mov @r1,a
    inc r1
    call num3
    sel mb1
    mov a,#PL_HAND-PL_BET-2     ; 10 -> +22
    call rx_adv
    mov r1,#V_TEXT              ; five cards, two raw ASCII bytes each. No
    mov r2,#10                  ; conversion at all: the wire sends "as"/"th"/
    call rx_copy                ; "??" and the charmap folds lowercase already
    jmp set_marks

    org 0AB2h
; seek_spot: put the cursor on the spotlighted player's record, 154 + 33*i.
;
; Walked rather than computed: rx_adv takes at most 128 at a time and there is
; no 16-bit arithmetic anywhere in this client. The spotlight is clamped to the
; player count first -- a seat can empty between polls, and there is nothing at
; that offset to read when it does.
seek_spot:
    mov r0,#X_PC
    movx a,@r0
    mov r2,a
    mov r0,#V_SPOT
    mov a,@r0
    cpl a
    add a,r2                    ; carry means spot < playerCount
    jc ss_ok
    clr a                       ; off the end: fall back to you
    mov @r0,a
ss_ok:
    call rx_home
    mov a,#128
    call rx_adv
    mov a,#GAME_PLAYERS-128     ; 26 -> offset 154
    call rx_adv
    mov r0,#V_SPOT
    mov a,@r0
    jz ss_done
    mov r2,a
    mov a,#PLAYER_STRIDE
    call rx_skip
ss_done:
    ret

    org 0BD0h
; set_marks: the two single characters the cards do not use, at the outer edges
; of the card row. The left one is which seat the spotlight is on -- 0 is always
; you. The right one is the betting round: 0 waiting, 1-4 the streets, 5 the
; showdown. It was blank in the 3K cart, and the comment where this used to sit
; said why -- "it would have shown the betting round, and the bank ran out".
;
; Out of line now: the round costs three bytes over the blank it replaced, and
; the section it shared with seek_spot had none. It has no conditional jumps of
; its own, so it does not care where it sits.
set_marks:
    mov r0,#V_SPOT
    mov a,@r0
    add a,#'0'
    mov r1,#V_TEXT+10
    mov @r1,a
    inc r1
    mov r0,#X_ROUND             ; read_state is the only caller and it holds XRAM
    movx a,@r0
    add a,#'0'
    mov @r1,a
    ret

    org 0AE1h
; ---------------------------------------------------------------------------
; load_move: read validMove V_MVSEL into V_BOT (6 characters of its name) and
; stage its 2-character code in X_MVA/X_MVB. put_time fills the last two cells.
;
; Six characters is the longest name the server sends -- ALL-IN -- so nothing is
; truncated, and the two cells left over are exactly the countdown.
; ---------------------------------------------------------------------------
load_move:
    mov a,#P1_SEL_XRAM
    outl p1,a
    call rx_home
    mov a,#GAME_VALIDMOVES      ; 88
    call rx_adv
    mov r0,#X_MVSEL
    movx a,@r0
    jz lm_at
    mov r2,a
    mov a,#MOVE_STRIDE
    call rx_skip
lm_at:
    call rx_next                ; move code, 2 of its 3 bytes
    mov r0,#X_MVA
    movx @r0,a
    call rx_next
    mov r0,#X_MVB
    movx @r0,a
    call rx_next                ; the third byte is padding
    mov r1,#V_BOT
    mov r2,#6
    call rx_copy
    jmp put_time

; ---------------------------------------------------------------------------
; check_state: A = 1 if the reply is a plausible Game struct.
;
; Two cheap smoke tests first -- neither round nor playerCount can be
; implausible in a real response -- and then the length that playerCount
; implies. playerCount sits at offset 153, before anything a truncation could
; have eaten, so it is safe to compute from even when the rest is short: a
; 7-player game's response can land anywhere from the 154-byte floor up to its
; true ~385 and still clear a fixed minimum while being truncated partway
; through the player array.
; ---------------------------------------------------------------------------
    org 0B23h
check_state:
    mov a,#P1_SEL_XRAM
    outl p1,a
    mov r0,#X_ROUND
    movx a,@r0
    add a,#0FAh                 ; -6: carry means round > 5
    jc cs_no
    mov r0,#X_PC
    movx a,@r0
    jz cs_no                    ; a game with no players at all
    add a,#0F7h                 ; -9: carry means more than 8
    jc cs_no
    mov r3,#0                   ; want = 154 + 33 * playerCount
    mov r4,#GAME_PLAYERS
    mov r0,#X_PC
    movx a,@r0
    mov r2,a
cs_add:
    mov a,r4
    add a,#PLAYER_STRIDE
    mov r4,a
    jnc cs_a1
    mov a,r3
    inc a
    mov r3,a
cs_a1:
    djnz r2,cs_add
    jmp rx_atleast              ; a tail jump: its answer is ours
cs_no:
    clr a
    ret

    org 0CC0h                   ; show_result outgrew its old slot when it
                                ; learned to page, and its loop is page-relative
                                ; -- so it needs a run that fits inside ONE page,
                                ; which in this bank means here
; ---------------------------------------------------------------------------
; show_result: page the whole of lastResult across the card row.
;
; The server's end-of-hand line -- "hulk bot won with two pair, aces over tens"
; -- is an 81-byte field and the row is 12 characters, so the 3K cart showed the
; first twelve and stopped. This walks it twelve at a time instead.
;
; It stops on the first page that starts with the field's NUL padding rather
; than running all seven pages, so a short message does not sit through five
; screens of blanks. Read straight out of the reply window, which still holds
; the message between polls.
; ---------------------------------------------------------------------------
show_result:
    mov a,#P1_SEL_XRAM
    outl p1,a
    mov r0,#X_PAGE
    clr a
    movx @r0,a
sr_page:
    call rx_home
    mov r0,#X_PAGE
    movx a,@r0
    call rx_adv
    mov r1,#V_TEXT
    mov r2,#TEXTLEN
    call rx_copy
    mov r1,#V_TEXT              ; padding: there is nothing left to show
    mov a,@r1
    jz sr_done
    mov r5,#0
    call draw
    sel mb1
    mov r2,#RESULT_HOLD
    call delay
    sel mb1
    mov a,#P1_SEL_XRAM
    outl p1,a
    mov r0,#X_PAGE
    movx a,@r0
    add a,#TEXTLEN
    movx @r0,a
    add a,#256-RESULT_LEN       ; carry once the next page starts past the field
    jnc sr_page
sr_done:
    ret
