; input.inc -- keypad + disc, folded into one code space.
;
; Two hardware facts shape this (see the bring-up's fujidisp.inc):
;   - FIRE is wired to the SAME line as keypad '2' ($1901 b3): they are
;     electrically indistinguishable. Outside the game screen that wire
;     is SELECT; inside the game it is strictly "move 2" (validated
;     against the server's move count, ignored otherwise).
;   - The P1 disc is an analog pot ($19FF, mux clear) valid ONLY during
;     vertical blank; read elsewhere it looks like "down". Every scan
;     starts by syncing to a fresh vblank EDGE, which also gives the
;     game loop its frame clock for free: one KEYSCN = one frame.
;
; Codes: 0-9 = the digits, KUP/KDOWN = disc, KENT = Enter, KCLR = Clear,
; KNONE = nothing. '2' doubles as FIRE. Column bits priority-encode
; through 16-byte tables so a mashed pair cannot alias to a third key.

KNONE   EQU     $FF
KUP     EQU     $80
KDOWN   EQU     $81
KENT    EQU     $82
KCLR    EQU     $83

JOYP1   EQU     $19FF           ; P1 vertical pot: up reads < $40, down > $C0
JOYUP   EQU     $40
JOYDN   EQU     $C0

; WAITVB: wait for the next vertical-blank EDGE (SENSE low then high).
; Leaf. Preserves everything but the PSW condition code.
WAITVB: TPSU    $80
        BCTR,EQ WAITVB          ; still in the previous vblank
WVB1:   TPSU    $80
        BCFR,EQ WVB1            ; wait for the next one to start
        RETC,UN

; KEYRAW: R0 = code of the first control found this frame (KNONE if
; none). Costs exactly one frame. Calls one down (WAITVB). Clobbers R0,R1.
KEYRAW: BSTR,UN WAITVB
        LODA,R0 JOYP1
        COMI,R0 JOYUP
        BCTR,LT KRUP
        COMI,R0 JOYDN
        BCTR,GT KRDN
        LODA,R0 $1900           ; b3='1' b2='4' b1='7' b0=Clear
        ANDI,R0 $0F
        BCFR,EQ KRC0
        LODA,R0 $1901           ; b3='2'/FIRE b2='5' b1='8' b0='0'
        ANDI,R0 $0F
        BCFR,EQ KRC1
        LODA,R0 $1902           ; b3='3' b2='6' b1='9' b0=Enter
        ANDI,R0 $0F
        BCFR,EQ KRC2
        LODI,R0 KNONE
        RETC,UN
KRUP:   LODI,R0 KUP
        RETC,UN
KRDN:   LODI,R0 KDOWN
        RETC,UN
KRC0:   STRZ    R1
        LODA,R0 KT0,R1
        RETC,UN
KRC1:   STRZ    R1
        LODA,R0 KT1,R1
        RETC,UN
KRC2:   STRZ    R1
        LODA,R0 KT2,R1
        RETC,UN

; nibble -> code, highest bit wins (top-row keys take precedence)
KT0:    DB      KNONE,KCLR,7,7,4,4,4,4,1,1,1,1,1,1,1,1
KT1:    DB      KNONE,0,8,8,5,5,5,5,2,2,2,2,2,2,2,2
KT2:    DB      KNONE,KENT,9,9,6,6,6,6,3,3,3,3,3,3,3,3

; KEYSCN: edge-detected KEYRAW -- a control reports once per press, and
; a rocked disc or second key reports without a full release.
; Calls two down. Clobbers R0,R1.
KEYSCN: BSTA,UN KEYRAW
        COMA,R0 V_KEY
        BCTR,EQ KSNONE          ; unchanged: held or still idle
        STRA,R0 V_KEY
        COMI,R0 KNONE
        BCFR,EQ KSHIT
KSNONE: LODI,R0 KNONE
KSHIT:  RETC,UN

; GETKEY: block for a fresh control; R0 = code. Calls three down.
GETKEY: BSTR,UN KEYSCN
        COMI,R0 KNONE
        BCTR,EQ GETKEY
        RETC,UN
