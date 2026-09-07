; sprites.inc -- the 2637's four movable objects.
;
; Sprite images ARE the first four UDC slots, so bank T gives four suit
; sprites (title bounce, in-game turn cue) and bank L an arrow cursor.
; Position registers take raw values: y raw = 256 - y_internal, x raw =
; x_internal + 43 (both from the MAME model; SPYOFF/SPXOFF absorb the
; calibration against real cells, tuned by M0 screenshot).

SPYOFF  EQU     0               ; added to row*8 before negation
SPXOFF  EQU     43              ; added to col*8

; SPRINI: colours, sizes, and park everything. Sprites 0/1 red (hearts,
; diamonds), 2/3 black (clubs, spades), all 8x8. Leaf. Clobbers R0,R3.
SPRINI: LODI,R0 $ED             ; b7/b6 8x8, spr0 red(5), spr1 red(5)
        STRA,R0 SPRC01
        LODI,R0 $FF             ; spr2 black(7), spr3 black(7)
        STRA,R0 SPRC23
        LODI,R3 7
SPIN1:  EORZ    R0
        STRA,R0 SPRPOS,R3
        BDRR,R3 SPIN1
        EORZ    R0
        STRA,R0 SPRPOS
        RETC,UN

; SPRSET: sprite R1 (0-3) to raw y=R2, x=R3. Leaf. Clobbers R0,R1.
SPRSET: LODZ    R1
        RRL,R0
        STRZ    R1              ; index*2
        LODZ    R2
        STRA,R0 SPRPOS,R1
        LODZ    R3
        STRA,R0 SPRPOS+1,R1
        RETC,UN

; SPRCELL: sprite R1 at cell row R2 / col R3 (raw conversion inline).
; Calls one down (SPRSET). Clobbers R0,R1,R2,R3.
SPRCELL: LODZ   R2
        RRL,R0
        RRL,R0
        RRL,R0                  ; row*8
        ADDI,R0 SPYOFF
        EORI,R0 $FF             ; 256 - v
        ADDI,R0 1
        STRZ    R2
        LODZ    R3
        RRL,R0
        RRL,R0
        RRL,R0
        ADDI,R0 SPXOFF
        STRZ    R3
        BCTR,UN SPRSET          ; tail call

; SPRPARK: park sprite R1 offscreen. Calls one down. Clobbers R0,R1,R2,R3.
SPRPARK: LODI,R2 0
        LODI,R3 0
        BCTR,UN SPRSET          ; tail call

; SPRTTL: one frame of the title wave -- the four suit sprites bob in
; alternating phase under the title. Call once per frame from a key
; loop. Calls one down (SPRSET). Clobbers R0-R3, V_TMP.
SPRTTL: LODA,R0 SPRTIC
        ADDI,R0 1
        STRA,R0 SPRTIC
        ANDI,R0 $0F
        BCFR,EQ SPRT9           ; step every 16 frames
        LODA,R0 SPRPH
        EORI,R0 1
        STRA,R0 SPRPH
        EORZ    R0
        STRA,R0 V_TMP
SPRT1:  LODA,R0 V_TMP
        COMI,R0 4
        BCFR,LT SPRT9
        ADDA,R0 SPRPH           ; (idx+phase)&1 picks the bob offset
        ANDI,R0 1
        BCTR,EQ SPRTA
        LODI,R2 (0-(TTLY+3+SPYOFF))&$FF
        BCTR,UN SPRTB
SPRTA:  LODI,R2 (0-(TTLY+SPYOFF))&$FF
SPRTB:  LODA,R1 V_TMP
        LODA,R0 TTLXT,R1
        STRZ    R3
        BSTA,UN SPRSET
        LODA,R0 V_TMP
        ADDI,R0 1
        STRA,R0 V_TMP
        BCTR,UN SPRT1
SPRT9:  RETC,UN

TTLY    EQU     150             ; title-wave pixel line
TTLXT:  DB      2*8+SPXOFF,6*8+SPXOFF,9*8+SPXOFF,13*8+SPXOFF

; SPRBOB: one frame of the in-game turn cue -- sprite 0 (the red heart)
; bobs beside the active seat's name row (SPRROW; $FF parks it). Call
; once per frame from the poll wait loop. Calls one down. Clobbers
; R0-R3.
SPRBOB: LODA,R0 SPRROW
        COMI,R0 $FF
        BCFR,EQ SPRB1
        LODI,R1 0
        BCTA,UN SPRPARK         ; tail call
SPRB1:  LODA,R0 SPRTIC
        ADDI,R0 1
        STRA,R0 SPRTIC
        ANDI,R0 $0F
        BCFR,EQ SPRB9
        LODA,R0 SPRPH
        EORI,R0 1
        STRA,R0 SPRPH
        LODA,R0 SPRROW
        RRL,R0
        RRL,R0
        RRL,R0                  ; row*8 (26-line cells)
        ADDI,R0 SPYOFF
        ADDA,R0 SPRPH
        ADDA,R0 SPRPH           ; +0/+2
        EORI,R0 $FF
        ADDI,R0 1
        STRZ    R2
        LODI,R3 9*8+SPXOFF      ; the status-marker column
        LODI,R1 0
        BSTA,UN SPRSET
SPRB9:  RETC,UN
