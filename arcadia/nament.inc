; nament.inc -- the title + name-entry screen (13-line doublescan mode).
;
; Eight slots, edited with the disc and keypad: disc up/down turns the
; character wheel A-Z 0-9 then '-', keypad 4/6 move the cursor, Clear
; blanks the slot and steps back, Enter (or FIRE) accepts. '-' is the
; blank -- the wheel has no space, so a name can never need the '+' URL
; escape -- and everything from the first '-' on is dropped at accept.
; An all-blank name keeps what PLNBUF had (the "ARCADIA" default).
;
; The four suit sprites (UDC bank T) ride a slow wave under the title --
; the edit buffer lives in the $1A00 scratch this mode leaves free.

NAMEED  EQU     $1A00           ; 13-line scratch: 8 edit slots
V_NCUR  EQU     $1A08           ; cursor slot

NAMESCR: BSTA,UN KEYRAW         ; a held key must not leak in
        STRA,R0 V_KEY
        BSTA,UN DINI13
        BSTA,UN DCLSU
        BSTA,UN SPRINI
        BSTA,UN UDLDT           ; bank T: suit sprites + glyphs
        DAT     1,0
        DCOL    AALERT
        LODI,R0 GHEART
        BSTA,UN DPUTC
        DAT     1,2
        DCOL    AFELT
        SETSTR  TFIVE
        BSTA,UN DPRINT
        DAT     1,15
        LODI,R0 GSPADE
        BSTA,UN DPUTC
        DAT     4,2
        SETSTR  TNAME
        BSTA,UN DPRINT
        DAT     10,1
        SETSTR  TEDIT
        BSTA,UN DPRINT
        DAT     11,1
        SETSTR  TACCPT
        BSTA,UN DPRINT
        LODI,R3 7               ; edit buffer = name padded with '-'
NEIN1:  LODA,R0 PLNBUF,R3
        BCFR,EQ NEIN2           ; walking down, so a NUL (or the bytes
        LODI,R0 '-'             ; past it) blanks; real chars copy
NEIN2:  STRA,R0 NAMEED,R3
        BDRR,R3 NEIN1
        LODA,R0 PLNBUF
        STRA,R0 NAMEED
        EORZ    R0
        STRA,R0 V_NCUR
        BSTA,UN NEDRAW
NEKEY:  BSTA,UN KEYSCN
        COMI,R0 KNONE
        BCFR,EQ NEK0
        BSTA,UN SPRTTL          ; the wave rides the wait loop
        BCTR,UN NEKEY
NEK0:   COMI,R0 KUP
        BCFR,EQ NEK1
        LODI,R1 1               ; wheel forward
        BCTA,UN NEWHL
NEK1:   COMI,R0 KDOWN
        BCFR,EQ NEK2
        LODI,R1 36              ; wheel back (mod 37)
        BCTA,UN NEWHL
NEK2:   COMI,R0 4               ; keypad 4: cursor left
        BCFR,EQ NEK3
        LODA,R0 V_NCUR
        BCTA,EQ NEKEY
        SUBI,R0 1
        STRA,R0 V_NCUR
        BCTA,UN NEDRW
NEK3:   COMI,R0 6               ; keypad 6: cursor right
        BCFR,EQ NEK4
        LODA,R0 V_NCUR
        COMI,R0 7
        BCFA,LT NEKEY
        ADDI,R0 1
        STRA,R0 V_NCUR
        BCTA,UN NEDRW
NEK4:   COMI,R0 KCLR            ; Clear: blank the slot, step back
        BCFR,EQ NEK5
        LODA,R1 V_NCUR
        LODI,R0 '-'
        STRA,R0 NAMEED,R1
        LODA,R0 V_NCUR
        BCTR,EQ NEK4A
        SUBI,R0 1
        STRA,R0 V_NCUR
NEK4A:  BCTA,UN NEDRW
NEK5:   COMI,R0 KENT            ; Enter or FIRE accepts
        BCTR,EQ NEACC
        COMI,R0 2
        BCFA,EQ NEKEY
NEACC:  LODI,R3 0               ; copy up to the first '-'
NEAC1:  COMI,R3 8
        BCTR,EQ NEAC9
        LODA,R0 NAMEED,R3
        COMI,R0 '-'
        BCTR,EQ NEAC9
        STRA,R0 PLNBUF,R3
        ADDI,R3 1
        BCTR,UN NEAC1
NEAC9:  COMI,R3 0               ; empty name: keep what PLNBUF had
        BCTA,EQ NEKEY
NEAC2:  COMI,R3 9               ; NUL-fill the rest so stale characters
        BCTR,EQ NEACD           ; never leak back into the editor
        EORZ    R0
        STRA,R0 PLNBUF,R3
        ADDI,R3 1
        BCTR,UN NEAC2
NEACD:  BSTA,UN SNDSEL
        RETC,UN

NEWHL:  LODA,R2 V_NCUR          ; step the wheel at the cursor by R1
        LODA,R0 NAMEED,R2
        LODI,R3 0               ; find its wheel index
NEWH1:  COMA,R0 WHEEL,R3
        BCTR,EQ NEWH2
        ADDI,R3 1
        BCTR,UN NEWH1
NEWH2:  LODZ    R3
        ADDZ    R1
NEWH3:  COMI,R0 37
        BCTR,LT NEWH4
        SUBI,R0 37
        BCTR,UN NEWH3
NEWH4:  STRZ    R3
        LODA,R0 WHEEL,R3
        STRA,R0 NAMEED,R2
NEDRW:  BSTA,UN SNDKEY
        BSTA,UN NEDRAW
        BCTA,UN NEKEY

; NEDRAW: the eight slots at row 6, cols 4-11, cursor cell inverse.
; Calls three down (DPUTC via DXLAT). Clobbers R0-R3, V_TMP.
NEDRAW: EORZ    R0
        STRA,R0 V_TMP
NED1:   LODA,R0 V_TMP
        COMI,R0 8
        BCFR,LT NED9
        LODI,R1 6
        ADDI,R0 4
        STRZ    R2
        BSTA,UN DPOS
        LODA,R0 V_TMP
        COMA,R0 V_NCUR
        BCFR,EQ NED2
        LODI,R0 ACARD           ; the cursor cell is inverse
        BCTR,UN NED3
NED2:   LODI,R0 AFELT
NED3:   STRA,R0 DCOLOR
        LODA,R1 V_TMP
        LODA,R0 NAMEED,R1
        BSTA,UN DXLAT
        BSTA,UN DPUTC
        LODA,R0 V_TMP
        ADDI,R0 1
        STRA,R0 V_TMP
        BCTR,UN NED1
NED9:   RETC,UN

WHEEL:  DB      "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-"
TFIVE:  DB      "5 CARD STUD",0
TNAME:  DB      "YOUR NAME",0
TEDIT:  DB      "DISC SPINS 4 6",0
TACCPT: DB      "ENTER ACCEPTS",0
