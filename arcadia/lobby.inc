; lobby.inc -- table selection (13-line doublescan mode).
;
; Up to 8 rows straight out of the /tables reply window: name in cols
; 1-10, the server's literal "cur / max" field right-aligned -- the '/'
; renders through the built-in diagonal, no UDC spent. The cursor is
; sprite 0 wearing bank L's arrow, gliding in the margin; disc moves it,
; Enter/FIRE (or a digit shortcut) joins, Clear returns to the name
; screen, 0 refetches the list.

LOBBY:  BSTA,UN KEYRAW          ; no leaking presses
        STRA,R0 V_KEY
        BSTA,UN DINI13
        BSTA,UN DCLSU
        BSTA,UN SPRINI
        BSTA,UN UDLDL           ; bank L: arrow sprite, real slash
        LODI,R0 $C0             ; sprite 0 stays 8x8 but goes white so it
        STRA,R0 SPRC01          ; reads on the felt margin
        DAT     0,1
        DCOL    AFELT
        SETSTR  TCHOOS
        BSTA,UN DPRINT
        DAT     11,1
        SETSTR  TJOINK
        BSTA,UN DPRINT
        DAT     12,1
        SETSTR  TBACKK
        BSTA,UN DPRINT
TBLRF:  EORZ    R0              ; fetch /tables
        BSTA,UN APICALL
        BCTR,EQ TBLOK
        DAT     9,1
        DCOL    AALERT
        SETSTR  TENET
        BSTA,UN DPRINT
        BSTA,UN SNDERR
TBLRW:  BSTA,UN KEYSCN
        COMI,R0 KNONE
        BCTR,EQ TBLRW           ; any key: try again
        BCTR,UN TBLRF
TBLOK:  EORZ    R0              ; the count byte
        STRA,R0 RXPTR
        STRA,R0 RXPTR+1
        BSTA,UN RXNEXT
        BCTR,EQ TBLRW           ; none yet: wait for a key, refetch
        COMI,R0 8
        BCTR,LT TBOK2
        LODI,R0 8               ; eight rows fit
TBOK2:  STRA,R0 V_CNT
        EORZ    R0
        STRA,R0 V_SEL
        BSTA,UN TBDRAW
TBKEY:  BSTA,UN TBCURS
        BSTA,UN KEYSCN
        COMI,R0 KNONE
        BCTR,EQ TBKEY
        COMI,R0 KUP
        BCFR,EQ TBK1
        LODA,R0 V_SEL
        BCTR,EQ TBKEY
        SUBI,R0 1
        STRA,R0 V_SEL
        BSTA,UN SNDKEY
        BCTR,UN TBKEY
TBK1:   COMI,R0 KDOWN
        BCFR,EQ TBK2
        LODA,R0 V_SEL
        ADDI,R0 1
        COMA,R0 V_CNT
        BCFA,LT TBKEY
        STRA,R0 V_SEL
        BSTA,UN SNDKEY
        BCTR,UN TBKEY
TBK2:   COMI,R0 KCLR            ; Clear: back to the name screen
        BCFR,EQ TBK3
        BSTA,UN NAMESCR
        BCTA,UN LOBBY
TBK3:   COMI,R0 KENT            ; Enter or FIRE joins
        BCTA,EQ TBJOIN
        COMI,R0 2
        BCTA,EQ TBJOIN
        COMI,R0 0               ; 0 refetches
        BCTA,EQ TBLRF
        COMI,R0 1               ; digits 1-8: row shortcut
        BCTA,LT TBKEY
        COMI,R0 8
        BCTA,GT TBKEY
        SUBI,R0 1
        COMA,R0 V_CNT
        BCFA,LT TBKEY           ; row >= count: reject
        STRA,R0 V_SEL
; falls into the join
TBJOIN: BSTA,UN SNDSEL
        LODA,R0 V_SEL           ; copy the id, up to its NUL
        LODI,R1 TBID
        BSTA,UN TBOFF
        EORZ    R0
        STRA,R0 V_TMP2
TBJC:   BSTA,UN RXNEXT
        BCTR,EQ TBJC9
        LODA,R1 V_TMP2
        STRA,R0 TBLBUF,R1
        LODA,R0 V_TMP2
        ADDI,R0 1
        STRA,R0 V_TMP2
        COMI,R0 9
        BCTR,LT TBJC
TBJC9:  LODA,R1 V_TMP2
        EORZ    R0
        STRA,R0 TBLBUF,R1
        LODI,R0 $FF             ; force the new-hand path on first poll
        STRA,R0 PRVRND
        STRA,R0 PRVACT          ; and arm the your-turn cue
        BCTA,UN GAMEIN

; TBOFF: state cursor = table record R0 + field R1; record base =
; 1 + rec*36, and row 7's name/players fields cross into slice 1.
; Leaf. Clobbers R0,R1,R3.
TBOFF:  RRL,R0
        RRL,R0                  ; *4
        STRZ    R3
        RRL,R0
        RRL,R0
        RRL,R0                  ; *32
        ADDZ    R3              ; *36
        ADDI,R0 1               ; + the count byte
        ADDZ    R1              ; + the field (may carry into slice 1)
        STRA,R0 RXPTR+1
        LODI,R0 0
        TPSL    $01
        BCFR,EQ TBOF1
        LODI,R0 1
TBOF1:  STRA,R0 RXPTR
        RETC,UN

; TBCURS: park the arrow sprite beside row (V_SEL). 13-line rows are 16
; pixels tall. Calls one down (SPRSET). Clobbers R0-R3.
TBCURS: LODA,R0 V_SEL
        ADDI,R0 2
        RRL,R0
        RRL,R0
        RRL,R0
        RRL,R0                  ; row*16
        ADDI,R0 4+SPYOFF        ; centre on the glyph half of the cell
        EORI,R0 $FF
        ADDI,R0 1
        STRZ    R2
        LODI,R3 SPXOFF          ; column 0
        LODI,R1 0
        BCTA,UN SPRSET          ; tail call

; TBDRAW: the list rows: name and "cur / max". Calls four down
; (RXPRNT -> RXNEXT -> MSLICE). Clobbers R0-R3, DCOLOR, V_SEAT, V_TMP2.
TBDRAW: EORZ    R0
        STRA,R0 V_SEAT          ; row index (V_TMP is APICALL's)
TBD1:   LODA,R0 V_SEAT
        COMA,R0 V_CNT
        BCFA,LT TBD9
        LODA,R0 V_SEAT
        LODI,R1 TBNAME
        BSTA,UN TBOFF
        LODA,R0 V_SEAT
        ADDI,R0 2               ; rows 2-9
        STRZ    R1
        LODI,R2 1
        BSTA,UN DPOS
        DCOL    AFELT
        LODI,R1 10
        BSTA,UN RXPRNT
        LODA,R0 V_SEAT          ; the players field, right after
        LODI,R1 TBPLYRS
        BSTA,UN TBOFF
        LODI,R1 5
        BSTA,UN RXPRNT
        LODA,R0 V_SEAT
        ADDI,R0 1
        STRA,R0 V_SEAT
        BCTA,UN TBD1
TBD9:   RETC,UN

TCHOOS: DB      "CHOOSE A TABLE",0
TJOINK: DB      "ENTER JOINS",0
TBACKK: DB      "CLEAR RENAMES",0
TENET:  DB      "NET ERR - RETRY",0
