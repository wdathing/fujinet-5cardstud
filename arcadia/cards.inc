; cards.inc -- drawing cards as character cells.
;
; A card is two adjacent cells, [rank][suit], both carrying the suit's
; attribute so the pips and rank share the ink on a white body: hearts
; and diamonds $C0 (red on white), clubs and spades $80 (black on
; white). A hidden card ("??" on the wire) is two lattice cells at $C0 --
; a red card back. The ten is the one rank needing two digits, so it has
; its own UDC and stays one cell.

; DRWCRD: draw the card whose rank ASCII is in V_RNK and suit ASCII in
; R0, at the next two cells. A '?' rank is the card back, a NUL rank is
; an undealt card (two felt blanks -- the hand string is NUL-padded on
; the wire); unknown suits and ranks land on catch-all table rows rather
; than walking off. Calls one down (DPUTC).
; Clobbers R0,R1,R2(+2),R3, DCOLOR, V_TMP2.
DRWCRD: STRZ    R1              ; hold the suit
        LODA,R0 V_RNK
        COMI,R0 '?'
        BCFR,EQ DCRDA
        BCTA,UN DRWBAK          ; hidden: the red lattice back
DCRDA:  COMI,R0 0
        BCFR,EQ DCRDB
        LODI,R0 AFELT           ; undealt: two felt blanks
        STRA,R0 DCOLOR
        EORZ    R0
        BSTA,UN DPUTC
        EORZ    R0
        BCTA,UN DPUTC           ; tail call
DCRDB:  LODZ    R1
        LODI,R1 0
DCRD1:  COMA,R0 SUITTB,R1       ; match the suit character
        BCTR,EQ DCRD2
        ADDI,R1 3
        COMI,R1 12              ; entry 12 is the catch-all
        BCFR,EQ DCRD1
DCRD2:  LODA,R0 SUITTB+2,R1
        STRA,R0 DCOLOR
        LODA,R0 SUITTB+1,R1
        STRA,R0 V_TMP2          ; suit glyph for the second cell
        LODA,R0 V_RNK
        COMI,R0 '2'
        BCTR,LT DCRDT
        COMI,R0 '9'
        BCTR,GT DCRDT
        SUBI,R0 $20             ; ASCII digit -> built-in digit glyph
        BCTR,UN DCRDP
DCRDT:  LODI,R1 0               ; letter ranks (and the catch-all)
DCRT1:  LODA,R0 RNKTB,R1
        BCTR,EQ DCRT2           ; terminator row: '.' fallback
        COMA,R0 V_RNK
        BCTR,EQ DCRT2
        ADDI,R1 2
        BCTR,UN DCRT1
DCRT2:  LODA,R0 RNKTB+1,R1
DCRDP:  BSTA,UN DPUTC           ; rank cell
        LODA,R0 V_TMP2
        BSTA,UN DPUTC           ; suit cell
        RETC,UN

; DRWBAK: draw a hidden card (two red lattice cells). Calls one down.
; Clobbers R0,R2(+2), DCOLOR.
DRWBAK: LODI,R0 AREDW
        STRA,R0 DCOLOR
        LODI,R0 GBACK
        BSTA,UN DPUTC
        LODI,R0 GBACK
        BSTA,UN DPUTC
        RETC,UN

; suit char -> glyph, attribute; last row is the catch-all
SUITTB: DB      'h',GHEART,AREDW
        DB      'd',GDIAM,AREDW
        DB      'c',GCLUB,ACARD
        DB      's',GSPADE,ACARD
        DB      0,GBACK,AREDW

; rank char -> glyph for the non-digits; 0 row is the catch-all
RNKTB:  DB      't',GTEN
        DB      'j',$23
        DB      'q',$2A
        DB      'k',$24
        DB      'a',$1A
        DB      0,$34
