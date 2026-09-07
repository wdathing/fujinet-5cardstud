; tail.inc -- the routines that live in the tails of the two data pages.
;
; The cart image is 3K and both code banks are full, but neither data page is.
; Page $E00 holds the charmap ($E02-$E41) and then the strings, sound cues and
; URL literals; page $F00 belongs to the client only as far as $F1F, since
; everything from $F20 up is written by the cartridge (fuji_mailbox.h,
; FN_R_STUB_END). What is left over at the end of each is where these three
; ended up. All are bank 1, like the code they call.

    org 0ED4h

; ---------------------------------------------------------------------------
; Sound
; ---------------------------------------------------------------------------

; snd: play cue A (a page-$E00 offset). P1 = P1_SEL_VDC.
;
; Descriptors are five bytes: three shift-register bytes, the control byte, and
; a hold in frames. The VDC's sound is a 24-bit shift register (o2doc 4.10);
; with the recirculation bit clear it plays once and stops on its own, so a
; click costs nothing to schedule. A cue that has to last sets recirculation and
; a hold, and this silences it afterwards.
snd:
    mov r1,a
    mov r0,#vdc_sound0
    mov r2,#4
snd_l:
    mov a,r1
    call cm_get
    movx @r0,a
    inc r0
    inc r1
    djnz r2,snd_l
    mov a,r1
    call cm_get
    jz snd_done                 ; no hold: the shift register stops itself
    mov r2,a
    call delay
    sel mb1
    mov r0,#vdc_soundctrl
    clr a
    movx @r0,a
snd_done:
    ret

    if BANK=3
; put_time: the 2-digit countdown in the last two cells of the bottom line.
; Called only from load_move and the move UI, both in this cartridge bank.
put_time:
    mov a,#P1_SEL_XRAM
    outl p1,a
    mov r0,#X_TIME
    movx a,@r0
    mov r1,#V_BOT+6
    call num2
    sel mb1
    ret
    endif

    if (BANK=0)||(BANK=3)
    org 0F02h                   ; page $F00, above the client's read stub and
                                ; below $F20, where the cartridge's half of the
                                ; mailbox begins (fuji_mailbox.h FN_R_STUB_END)
                                ;
                                ; rx_atleast calls rx_len over in net.inc, so it
                                ; belongs to the two banks that carry net.inc and
                                ; validate a reply: the lobby and the game loop.
                                ; The screens in banks 1 and 2 render out of a
                                ; window one of those two has already checked.

; rx_atleast: A = 1 if the last reply was at least r3:r4 bytes long.
;
; The reply length is the only defence against an HTTP error page being read as
; a Game struct: a mailbox ACK means the cart relayed SOMETHING, not that the
; server liked the request.
;
; The comparison is written as (255 - actual) + want, so `carry` means
; want > actual. The obvious form -- negate `want` and add -- silently answers
; "false" when want is 0, because -0 is 0 and the add never carries.
rx_atleast:
    call rx_len
    mov a,r6
    xrl a,r3
    jnz ra_hidiff
    mov a,r5
    cpl a
    add a,r4
    jc ra_no
    jmp ra_yes
ra_hidiff:
    mov a,r6
    cpl a
    add a,r3
    jc ra_no
ra_yes:
    mov a,#1
    ret
ra_no:
    clr a
    ret
    endif
