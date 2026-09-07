; demo.inc -- DEMO=1: a static mock table, no network.
;
; Exercises every drawing path so palette, glyphs and sprite calibration
; happen by screenshot before the protocol exists (the astrocade port's
; trick): all four attributes, red and black cards, the lattice back,
; the ten, the chip, an inverse active-name bar, the white status bar,
; a bobbing sprite next to the active seat, a parked sprite, and a
; deliberate spaces-in-$C0 print that regression-tests the DPUTC guard.

DEMY0   EQU     (0-(2*8+SPYOFF))&$FF    ; sprite bob: row 2, +/- 2 px
DEMY1   EQU     (0-(2*8+2+SPYOFF))&$FF
DEMX    EQU     9*8+SPXOFF

DEMO0:  BSTA,UN DINI26
        BSTA,UN DCLSU
        BSTA,UN DCLSL
        BSTA,UN SPRINI
        BSTA,UN UDLDT           ; bank T (block 2 loads the UDC RAM)

; ---- header ------------------------------------------------------------
        DAT     0,0
        DCOL    AFELT
        SETSTR  DSPOT
        BSTA,UN DPRINT
        NUM16   1250
        BSTA,UN NUMPRT
        DAT     0,11
        DCOL    AALERT
        SETSTR  DSRND
        BSTA,UN DPRINT

; ---- band 1, rows 2-3: the active player -------------------------------
        DAT     2,0
        DCOL    ACARD           ; inverse name bar
        SETSTR  DSCLA
        LODI,R1 9
        BSTA,UN DPRTN
        DAT     2,10
        DCOL    AALERT
        LODI,R0 GCHIP
        BSTA,UN DPUTC
        DCOL    AFELT
        NUM16   500
        BSTA,UN NUMPRT
        DAT     3,0
        CARD    'a','s'
        CARD    'k','h'
        BSTA,UN DRWBAK
        CARD    'q','d'
        CARD    't','c'
        DAT     3,11
        DCOL    AFELT
        SETSTR  DSRAI
        LODI,R1 5
        BSTA,UN DPRTN

; ---- band 2, rows 5-6: folded ------------------------------------------
        DAT     5,0
        DCOL    AFELT
        SETSTR  DSTHO
        LODI,R1 9
        BSTA,UN DPRTN
        DCOL    AALERT
        LODI,R0 $1F             ; 'F'
        BSTA,UN DPUTC
        DAT     6,11
        DCOL    AFELT
        SETSTR  DSFLD
        LODI,R1 5
        BSTA,UN DPRTN

; ---- band 3, rows 8-9: waiting with a hole card -------------------------
        DAT     8,0
        DCOL    AFELT
        SETSTR  DSMAT
        LODI,R1 9
        BSTA,UN DPRTN
        DAT     8,10
        DCOL    AALERT
        LODI,R0 GCHIP
        BSTA,UN DPUTC
        DCOL    AFELT
        NUM16   25
        BSTA,UN NUMPRT
        DAT     9,0
        BSTA,UN DRWBAK
        CARD    '9','h'
        CARD    '8','c'
        CARD    '7','s'

; ---- your band, rows 17-18 ----------------------------------------------
        DAT     17,0
        DCOL    AFELT
        SETSTR  DSYOU
        LODI,R1 9
        BSTA,UN DPRTN
        DAT     18,0
        CARD    'a','h'
        CARD    'a','d'
        CARD    'k','s'
        CARD    'q','c'
        CARD    'j','d'

; ---- purse row 20 --------------------------------------------------------
        DAT     20,0
        DCOL    AFELT
        SETSTR  DSPUR
        BSTA,UN DPRINT
        NUM16   2450
        BSTA,UN NUMPRT
        DAT     20,14
        DCOL    AALERT
        SETSTR  DSCLK
        BSTA,UN DPRINT

; ---- status bar rows 22-24 ------------------------------------------------
        DAT     22,0
        DCOL    ACARD
        SETSTR  DSMV1
        LODI,R1 16
        BSTA,UN DPRTN
        DAT     23,0
        SETSTR  DSMV2
        LODI,R1 16
        BSTA,UN DPRTN
        DAT     24,0
        SETSTR  DSMV3
        LODI,R1 16
        BSTA,UN DPRTN

; ---- the $C0 guard regression, row 25 --------------------------------------
        DAT     25,0
        DCOL    AREDW
        SETSTR  DSGRD
        BSTA,UN DPRINT

; ---- sprites: 0 bobs by the active seat, 3 sits by the pot, 1-2 parked ----
        LODI,R1 3
        LODI,R2 0
        LODI,R3 9
        BSTA,UN SPRCELL

DEMLP:  LODI,R1 0
        LODI,R2 DEMY0
        LODI,R3 DEMX
        BSTA,UN SPRSET
        LODI,R3 16
DEMW1:  BSTA,UN WAITVB
        BDRR,R3 DEMW1
        LODI,R1 0
        LODI,R2 DEMY1
        LODI,R3 DEMX
        BSTA,UN SPRSET
        LODI,R3 16
DEMW2:  BSTA,UN WAITVB
        BDRR,R3 DEMW2
        BCTA,UN DEMLP

DSPOT:  DB      "POT",0
DSRND:  DB      "RND 3",0
DSCLA:  DB      "CLAUDE",0
DSTHO:  DB      "THOM",0
DSMAT:  DB      "MATT",0
DSYOU:  DB      "ARCADIA",0
DSRAI:  DB      "RAISE",0
DSFLD:  DB      "FOLD",0
DSPUR:  DB      "PURSE",0
DSCLK:  DB      "18",0
DSMV1:  DB      "1 FOLD   2 CHECK",0
DSMV2:  DB      "3 BET    4 RAISE",0
DSMV3:  DB      "5 ALLIN",0
DSGRD:  DB      "C0 GUARD OK",0
