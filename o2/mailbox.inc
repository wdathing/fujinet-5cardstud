; mailbox.inc -- the FujiNet mailbox and the reply cursor.
;
; HIGH bank, and deliberately so. Every caller of these routines is in net.inc
; or render.inc, also high, so putting them here makes those calls intra-bank:
; AS emits no SEL MB prefix and none is needed back. Left in the low bank
; alongside the display code they cost four bytes a call instead of two, and
; there are about ninety such calls.
;
; The mailbox layout is an 8048 mirror of fuji_mailbox.h; see o2fuji.inc.

    org 800h

; ---------------------------------------------------------------------------
; Mailbox
; ---------------------------------------------------------------------------

; fn_reg: write A into the mailbox register named by r7. P1 = P1_SEL_XRAM.
fn_reg:
    mov r6,a
    mov r0,#FN_W_REGSEL
    mov a,r7
    movx @r0,a
    mov r0,#FN_W_REGDATA
    mov a,r6
    movx @r0,a
    ret

; fn_push: append A to the TX byte stream. P1 = P1_SEL_XRAM.
;
; Runs in register bank 1 so it cannot disturb a caller walking a buffer with
; r0 -- which is what every payload loop does. It touches only r0', which is why
; $19-$1F stay usable as ordinary scratch (V_HAVEMV/V_POLL/V_PREV/V_LOCK live
; there): nothing else in this program ever selects bank 1.
fn_push:
    sel rb1
    mov r0,#FN_W_DATA
    movx @r0,a
    sel rb0
    ret

; fn_par1: append a one-byte parameter (size marker, then the value).
fn_par1:
    mov r4,a
    mov a,#1
    call fn_push
    mov a,r4
    call fn_push
    ret

; fn_read: A = offset within the cart's mailbox page -> A = the byte there.
; MOVP reads the page the PC is in, so the load itself lives at $F00 -- and
; because it is a program-memory read it works no matter how P1 is configured,
; which is what lets a draw pull reply bytes straight out of the window.
;
; A tail jump, not a call: $F00 is in this same bank, so this costs two bytes
; and no stack level. It relies on MB being 1 throughout high-bank code, which
; is precisely the invariant tools/checkmb.py enforces.
fn_read:
    jmp fn_get

; fn_begin: start a request. A = command, r6 = device, r2 = nparam, r3 = TXLEN_HI.
; TXLEN_LO is 0 here; fn_txlen sets both after a payload has been built.
fn_begin:
    mov r4,a
    mov a,r6                    ; grab the device BEFORE fn_reg clobbers r6
    mov r5,a
    clr a
    mov r7,#FN_REG_DATA_RST
    call fn_reg
    mov a,r5
    mov r7,#FN_REG_DEVICE
    call fn_reg
    mov a,r4
    mov r7,#FN_REG_CMD
    call fn_reg
    mov a,r2
    mov r7,#FN_REG_NPARAM
    call fn_reg
    clr a
    mov r7,#FN_REG_TXLEN_LO
    call fn_reg
    mov a,r3
    mov r7,#FN_REG_TXLEN_HI
    call fn_reg
    ret

; fn_txlen: set TXLEN to r5:A, for a payload whose length is only known once it
; has been built -- which is every URL this client sends.
fn_txlen:
    mov r4,a
    mov r7,#FN_REG_TXLEN_LO
    mov a,r4
    call fn_reg
    mov a,r5
    mov r7,#FN_REG_TXLEN_HI
    call fn_reg
    ret

; ---------------------------------------------------------------------------
; fn_fire: launch the transaction and wait for it.
; Returns A = the reply command (FUJICMD_ACK / _NAK), or 0 on timeout.
;
; The sequence number comes from the CART's persisted ACKSEQ, never from a local
; counter. A console RESET restarts this program and re-zeroes its variables but
; does NOT reset the cartridge, so a local counter recomputes a value the cart
; has already acknowledged: no further request is ever sent, and the screen goes
; on showing the previous reply, looking like a stable success. It cost real
; debugging time on the Intellivision, and -resetat=N exists to prove it does
; not happen here. Sequence 0 is reserved.
; ---------------------------------------------------------------------------
fn_fire:
    mov a,#FN_R_ACKSEQ&0FFh
    call fn_read
    inc a
    jnz ff_seq
    inc a
ff_seq:
    mov r5,a
    mov r0,#FN_W_SEQ
    movx @r0,a
    mov r7,#FN_ROUNDS
ff_round:
    mov r6,#FN_POLLS
ff_poll:
    mov a,#FN_R_ACKSEQ&0FFh
    call fn_read
    xrl a,r5
    jz ff_ok
    mov r4,#0                   ; ~1ms between polls: fine enough that a fast
ff_wait:                        ; reply is not delayed, coarse enough that a
    djnz r4,ff_wait             ; slow one does not spin the bus flat out
    djnz r6,ff_poll
    djnz r7,ff_round
    clr a                       ; timed out
    ret
ff_ok:
    mov a,#FN_R_REPLY_CMD&0FFh
    call fn_read
    ret

; fn_ack: A = fn_fire's result -> A = 1 on ACK, else 0.
fn_ack:
    xrl a,#FUJICMD_ACK
    jnz fa_no
    mov a,#1
    ret
fa_no:
    clr a
    ret

; ---------------------------------------------------------------------------
; The reply cursor.
;
; The cart publishes one 208-byte slice of the reply at a time, at $F30-$FFF --
; which is exactly the tail of the page. So an 8-bit page pointer WRAPPING PAST
; $FF is the slice boundary, and walking the reply needs no division by 208 and
; no 16-bit arithmetic anywhere. Player i's record at 154 + 33*i is reached by
; rx_home, rx_adv 128, rx_adv 26, then i times rx_adv 33.
;
; All three need P1 = P1_SEL_XRAM, because switching slices writes a mailbox
; register. The MOVP reads themselves do not care.
; ---------------------------------------------------------------------------

; rx_home: publish slice 0 and point at the first reply byte.
rx_home:
    clr a
    mov r0,#V_SLICE
    mov @r0,a
    mov r7,#FN_REG_RXSLICE
    call fn_reg
    mov r0,#V_RXP
    mov a,#FN_R_DATA&0FFh
    mov @r0,a
    ret

; rx_bump: publish the next slice.
rx_bump:
    mov r0,#V_SLICE
    mov a,@r0
    inc a
    mov @r0,a
    mov r7,#FN_REG_RXSLICE
    call fn_reg
    ret

; rx_adv: advance the cursor by A bytes. A must be <= 128, so the page pointer
; can cross at most one slice boundary per call: with the pointer in $30..$FF,
; ptr+A tops out at $17F, so the wrapped low byte plus $30 cannot carry again.
rx_adv:
    mov r0,#V_RXP
    add a,@r0
    jc rxa_cross
    mov @r0,a
    ret
rxa_cross:
    add a,#FN_R_DATA&0FFh       ; the low byte is the index into the next slice
    mov @r0,a
    call rx_bump
    ret

; rx_next: read the byte at the cursor, then advance one.
rx_next:
    mov r0,#V_RXP
    mov a,@r0
    call fn_read
    mov r5,a
    mov r0,#V_RXP
    mov a,@r0
    inc a
    jz rxn_cross
    mov @r0,a
    mov a,r5
    ret
rxn_cross:
    mov a,#FN_R_DATA&0FFh
    mov @r0,a
    call rx_bump
    mov a,r5
    ret

; rx_copy: copy r2 bytes from the cursor into internal RAM at r1.
rx_copy:
    call rx_next
    mov @r1,a
    inc r1
    djnz r2,rx_copy
    ret

; rx_skip: advance the cursor by r2 lots of A bytes. Used to walk to player i
; without any 16-bit arithmetic.
rx_skip:
    mov r4,a
rxs_l:
    mov a,r4
    call rx_adv
    djnz r2,rxs_l
    ret

; rx_u16: read a little-endian u16 at the cursor into r6 (high) : r5 (low),
; ready for num3. Advances the cursor past both bytes.
rx_u16:
    call rx_next
    mov r3,a
    call rx_next
    mov r6,a
    mov a,r3
    mov r5,a
    ret
