; game.inc -- the game screen and the poll loop (26-line multicolor).
;
; Rendering policy is the astrocade port's: every poll redraws
; everything, opaquely, straight out of the reply window -- identical
; pixels rewritten are invisible, so there is no flicker and no digest
; bookkeeping. The one thing opaque overdraw cannot fix is content that
; SHRANK (a new hand dealt, a player gone), so CHKNEW watches round and
; playerCount and answers with a full clear.
;
; Seat geometry: 16 columns cannot seat two hands side by side, so the
; Lynx ring becomes a vertical stack of 2-row bands -- name/bet row over
; hand/move row -- placed by BANDROW per player count. Server order
; reads top-down around the table; YOU (index 0) are always the fixed
; band at rows 17-18.
;
;   row  0   POT nnnnn   round token
;   2-15     opponent bands
;   17-18    your band
;   row 20   PURSE nnnnn        clock
;   22-24    the white bar: move menu / WAITING ON / lastResult

; GAMEIN: from the lobby: flip to the game screen and poll.
GAMEIN: BSTA,UN DCLSL           ; the 13-line scratch becomes visible rows
        BSTA,UN DCLSU
        BSTA,UN DINI26
        BSTA,UN SPRINI          ; suits back in red/black, all parked
        BSTA,UN UDLDT
        LODI,R0 $FF
        STRA,R0 SPRROW
        DAT     10,3
        DCOL    AFELT
        SETSTR  TJOING
        BSTA,UN DPRINT
; falls into GAMELP

GAMELP: LODA,R0 MOVBUF          ; a staged move rides the next request --
        BCFR,EQ GLMV            ; its reply IS the next state
        LODI,R0 1
        BCTR,UN GLREQ
GLMV:   LODI,R0 2
GLREQ:  BSTA,UN APICALL
        BCFA,EQ GLFAIL
        EORZ    R0
        STRA,R0 MOVBUF          ; consumed
        BSTA,UN CHKNEW
        BSTA,UN RENDER
        LODI,R0 120             ; ~2 s: one KEYSCN per frame
        STRA,R0 V_TICK
GLWAIT: BSTA,UN KEYSCN
        COMI,R0 KNONE
        BCFR,EQ GLKEY
        BSTA,UN SPRBOB          ; the turn cue rides the wait loop
        LODA,R0 V_TICK
        SUBI,R0 1
        STRA,R0 V_TICK
        BCFR,EQ GLWAIT
        BCTA,UN GAMELP
GLKEY:  COMI,R0 KCLR            ; Clear leaves the table
        BCFR,EQ GLK1
        LODI,R0 3
        BSTA,UN APICALL         ; best effort; we are going regardless
        BCTA,UN LOBBY
GLK1:   COMI,R0 0               ; keypad 0: poll now
        BCTA,EQ GAMELP
        COMI,R0 5               ; keypad 1-5: choose a move
        BCTA,GT GLWAIT          ; (anything else keeps waiting)
        COMI,R0 1
        BCTA,LT GLWAIT
        SUBI,R0 1
        STRA,R0 V_TMP2          ; n = key - 1
        EORZ    R0
        STRA,R0 RXPTR
        LODI,R0 GOMVCNT
        STRA,R0 RXPTR+1
        BSTA,UN RXNEXT
        COMA,R0 V_TMP2
        BCFA,GT GLWAIT          ; not on the menu right now
        LODA,R3 V_TMP2          ; move code offset = GOMOVES + n*13
        LODI,R0 GOMOVES
GLK2:   COMI,R3 0
        BCTR,EQ GLK3
        ADDI,R0 MVSTRID
        SUBI,R3 1
        BCTR,UN GLK2
GLK3:   STRA,R0 RXPTR+1
        EORZ    R0
        STRA,R0 RXPTR
        BSTA,UN RXNEXT
        STRA,R0 MOVBUF
        BSTA,UN RXNEXT
        STRA,R0 MOVBUF+1
        BSTA,UN SNDSEL
        BCTA,UN GAMELP          ; the move rides the poll going out now
GLFAIL: DAT     0,15            ; a red mark in the corner; back off,
        DCOL    AALERT          ; keep Clear working
        LODI,R0 $31             ; 'X'
        BSTA,UN DPUTC
        BSTA,UN SNDERR
        LODI,R0 150
        STRA,R0 V_TICK
GLFW:   BSTA,UN KEYSCN
        COMI,R0 KCLR
        BCFR,EQ GLFW1
        LODI,R0 3
        BSTA,UN APICALL
        BCTA,UN LOBBY
GLFW1:  LODA,R0 V_TICK
        SUBI,R0 1
        STRA,R0 V_TICK
        BCFR,EQ GLFW
        BCTA,UN GAMELP

; CHKNEW: a new hand (round dropped) or a changed seat count means the
; screen holds cards that no longer exist: clear both halves.
; Calls two down. Clobbers R0,R1,R3, RXPTR.
CHKNEW: EORZ    R0
        STRA,R0 RXPTR
        LODI,R0 GOROUND
        STRA,R0 RXPTR+1
        BSTA,UN RXNEXT
        COMA,R0 PRVRND
        STRA,R0 PRVRND
        BCTR,LT CHKN1           ; round dropped: new hand
        LODI,R0 GOPLCNT
        STRA,R0 RXPTR+1
        BSTA,UN RXNEXT
        COMA,R0 PRVPC
        STRA,R0 PRVPC
        BCTR,EQ CHKN9           ; nothing shrank
        BCTR,UN GSTATIC
CHKN1:  LODI,R0 GOPLCNT         ; keep PRVPC honest on the round path too
        STRA,R0 RXPTR+1
        BSTA,UN RXNEXT
        STRA,R0 PRVPC
GSTATIC: BSTA,UN DCLSU
        BSTA,UN DCLSL
CHKN9:  RETC,UN

; ======================================================================
; RENDER: one poll onto the screen, zero-copy.
; ======================================================================
RENDER: EORZ    R0              ; header fields, read sequentially 81-87
        STRA,R0 RXPTR
        LODI,R0 GOROUND
        STRA,R0 RXPTR+1
        BSTA,UN RXNEXT
        STRA,R0 V_RND
        BSTA,UN RXNEXT          ; pot, u16 LE
        STRA,R0 V_NUM+1
        BSTA,UN RXNEXT
        STRA,R0 V_NUM
        BSTA,UN RXNEXT
        STRA,R0 V_ACT
        BSTA,UN RXNEXT
        STRA,R0 V_TIM
        BSTA,UN RXNEXT
        STRA,R0 V_VIEW
        BSTA,UN RXNEXT
        STRA,R0 V_MVC
        LODI,R0 GOPLCNT
        STRA,R0 RXPTR+1
        BSTA,UN RXNEXT
        STRA,R0 V_PC
        DAT     0,0             ; POT nnnnn
        DCOL    AFELT
        SETSTR  TPOT
        BSTA,UN DPRINT
        BSTA,UN NUMPRT
        DAT     0,11            ; round token, red
        DCOL    AALERT
        LODA,R0 V_RND
        BCFR,EQ RNDH1
        SETSTR  TWAIT
        LODI,R1 5
        BSTA,UN DPRTN
        BCTA,UN RNDDN
RNDH1:  COMI,R0 5
        BCTR,LT RNDH2
        SETSTR  TSHOW
        LODI,R1 5
        BSTA,UN DPRTN
        BCTA,UN RNDDN
RNDH2:  SETSTR  TRND
        LODI,R1 4
        BSTA,UN DPRTN
        LODA,R0 V_RND
        IORI,R0 $10
        BSTA,UN DPUTC
RNDDN:  LODA,R0 V_ACT           ; the your-turn cue, on the edge only
        COMA,R0 PRVACT
        STRA,R0 PRVACT
        BCTR,EQ RENDA2
        LODA,R0 V_ACT
        BCFR,EQ RENDA2
        BSTA,UN SNDTRN
RENDA2: LODI,R0 $FF             ; each seat marks itself if active
        STRA,R0 SPRROW
        EORZ    R0
        STRA,R0 V_SEAT
RENDP:  LODA,R0 V_SEAT
        COMA,R0 V_PC
        BCFR,LT RENDG
        BSTA,UN SEAT1
        LODA,R0 V_SEAT
        ADDI,R0 1
        STRA,R0 V_SEAT
        BCTR,UN RENDP
RENDG:  BSTA,UN SPRSHW
        BSTA,UN PURSE
        BCTA,UN BAR             ; tail call

; SPRSHW: the showdown flourish -- diamond/club/spade on the felt row
; above the result banner while the showdown stands, parked otherwise
; (sprite 0 stays the turn cue). Calls two down. Clobbers R0-R3, V_TMP2.
SPRSHW: LODA,R0 V_RND
        COMI,R0 5
        BCTR,LT SPSHP
        LODI,R1 1               ; diamond
        LODI,R2 21
        LODI,R3 5
        BSTA,UN SPRCELL
        LODI,R1 2               ; club
        LODI,R2 21
        LODI,R3 8
        BSTA,UN SPRCELL
        LODI,R1 3               ; spade
        LODI,R2 21
        LODI,R3 11
        BCTA,UN SPRCELL         ; tail call
SPSHP:  LODI,R0 3
        STRA,R0 V_TMP2
SPSH1:  LODA,R1 V_TMP2
        BSTA,UN SPRPARK
        LODA,R0 V_TMP2
        SUBI,R0 1
        STRA,R0 V_TMP2
        BCFR,EQ SPSH1
        RETC,UN

; ----------------------------------------------------------------------
; SEAT1: render seat (V_SEAT) as a 2-row band.
SEAT1:  LODA,R0 V_SEAT
        BCFR,EQ SEATOP
        LODI,R0 17              ; you: the fixed bottom band
        BCTR,UN SEATRW
SEATOP: LODA,R0 V_PC            ; row = BANDROW[(pc-2)*7 + i-1]
        SUBI,R0 2
        STRZ    R3
        RRL,R0
        RRL,R0
        RRL,R0                  ; *8
        SUBZ    R3              ; *7
        ADDA,R0 V_SEAT
        SUBI,R0 1
        STRZ    R1
        LODA,R0 BANDROW,R1
SEATRW: STRA,R0 V_ROW
        LODA,R0 V_SEAT          ; record base
        BSTA,UN PLBASE
        LODI,R1 PLSTAT
        BSTA,UN SETCUR
        BSTA,UN RXNEXT
        STRA,R0 V_ST
        LODA,R1 V_ROW           ; name row
        LODI,R2 0
        BSTA,UN DPOS
        LODA,R0 V_SEAT          ; active player: inverse name bar
        COMA,R0 V_ACT
        BCFR,EQ SEATN1
        LODI,R0 ACARD
        BCTR,UN SEATN2
SEATN1: LODI,R0 AFELT
SEATN2: STRA,R0 DCOLOR
        LODI,R1 PLNAME
        BSTA,UN SETCUR
        LODI,R1 9
        BSTA,UN RXPRNT
        LODA,R0 V_ST            ; status marker col 9
        COMI,R0 2
        BCTR,LT SEATNM
        DCOL    AALERT
        LODA,R0 V_ST
        COMI,R0 3
        BCTR,EQ SEATL
        LODI,R0 $1F             ; 'F' folded
        BCTR,UN SEATMK
SEATL:  LODI,R0 $25             ; 'L' left
SEATMK: BSTA,UN DPUTC
        BCTR,UN SEATBT
SEATNM: DCOL    AFELT
        EORZ    R0
        BSTA,UN DPUTC
SEATBT: LODI,R1 PLBET           ; chip + amount, cols 10-15
        BSTA,UN SETCUR
        BSTA,UN RXNEXT
        STRA,R0 V_NUM+1
        BSTA,UN RXNEXT
        STRA,R0 V_NUM
        BCFR,EQ SEATB2
        LODA,R0 V_NUM+1
        BCFR,EQ SEATB2
        DCOL    AFELT           ; no bet: blank the chip and the amount
        LODI,R1 0
        LODI,R3 6
        BSTA,UN DFILL
        BCTR,UN SEATHD
SEATB2: DCOL    AALERT
        LODI,R0 GCHIP
        BSTA,UN DPUTC
        DCOL    AFELT
        BSTA,UN NUMPRT
SEATHD: LODA,R1 V_ROW           ; hand row
        ADDI,R1 1
        LODI,R2 0
        BSTA,UN DPOS
        LODA,R0 V_ST
        COMI,R0 2
        BCTR,LT SEATH1
        DCOL    AFELT           ; folded/left: no cards
        LODI,R1 0
        LODI,R3 10
        BSTA,UN DFILL
        BCTR,UN SEATMV
SEATH1: LODI,R1 PLHAND
        BSTA,UN SETCUR
        BSTA,UN DRWHND
SEATMV: LODA,R1 V_ROW           ; last-move word, cols 11-15
        ADDI,R1 1
        LODI,R2 11
        BSTA,UN DPOS
        DCOL    AFELT
        LODI,R1 PLMOVE
        BSTA,UN SETCUR
        LODI,R1 5
        BSTA,UN RXPRNT
        LODA,R0 V_SEAT          ; the turn cue bobs by this band
        COMA,R0 V_ACT
        BCFR,EQ SEAT9
        LODA,R0 V_ROW
        STRA,R0 SPRROW
SEAT9:  RETC,UN

; DRWHND: draw 5 cards from the state cursor at the current position.
; Calls three down (DRWCRD -> DPUTC). Clobbers R0-R3, V_RNK, V_CNT.
DRWHND: LODI,R0 5
        STRA,R0 V_CNT
DRWH1:  BSTA,UN RXNEXT
        STRA,R0 V_RNK
        BSTA,UN RXNEXT
        BSTA,UN DRWCRD          ; suit in R0; a '?' rank draws the back
        LODA,R0 V_CNT
        SUBI,R0 1
        STRA,R0 V_CNT
        BCFR,EQ DRWH1
        RETC,UN

; PLBASE: V_BASE = GOPLYRS + 33 * player R0 (records 4+ live in slice 1).
; Leaf. Clobbers R0,R3.
PLBASE: STRZ    R3
        RRL,R0
        RRL,R0
        RRL,R0
        RRL,R0
        RRL,R0                  ; *32
        ADDZ    R3              ; *33
        ADDI,R0 GOPLYRS         ; may carry into slice 1
        STRA,R0 V_BASE+1
        LODI,R0 0
        TPSL    $01
        BCFR,EQ PLB1
        LODI,R0 1
PLB1:   STRA,R0 V_BASE
        RETC,UN

; SETCUR: state cursor = V_BASE + field R1. Leaf. Clobbers R0,R1.
SETCUR: LODA,R0 V_BASE+1
        ADDZ    R1
        STRA,R0 RXPTR+1
        LODA,R0 V_BASE
        TPSL    $01
        BCFR,EQ SETC1
        ADDI,R0 1
SETC1:  STRA,R0 RXPTR
        RETC,UN

; ----------------------------------------------------------------------
; PURSE: row 20 -- your purse (or the viewing banner) and the move clock.
PURSE:  DAT     20,0
        LODA,R0 V_VIEW
        BCTR,EQ PUR1
        DCOL    AALERT
        SETSTR  TVIEW
        LODI,R1 14
        BSTA,UN DPRTN
        BCTR,UN PURT
PUR1:   DCOL    AFELT
        SETSTR  TPURSE
        BSTA,UN DPRINT
        EORZ    R0              ; your purse: player 0's record
        STRA,R0 RXPTR
        LODI,R0 GOPLYRS+PLPURSE
        STRA,R0 RXPTR+1
        BSTA,UN RXNEXT
        STRA,R0 V_NUM+1
        BSTA,UN RXNEXT
        STRA,R0 V_NUM
        BSTA,UN NUMPRT
PURT:   DAT     20,14           ; the move clock
        LODA,R0 V_ACT
        COMI,R0 $FF
        BCTR,EQ PURT0
        DCOL    AALERT
        LODA,R0 V_TIM
        LODI,R1 0
PURTT:  COMI,R0 10
        BCTR,LT PURTD
        SUBI,R0 10
        ADDI,R1 1
        BCTR,UN PURTT
PURTD:  STRZ    R3
        LODZ    R1
        IORI,R0 $10
        BSTA,UN DPUTC
        LODZ    R3
        IORI,R0 $10
        BSTA,UN DPUTC
        RETC,UN
PURT0:  DCOL    AFELT
        LODI,R1 0
        LODI,R3 2
        BSTA,UN DFILL
        RETC,UN

; ----------------------------------------------------------------------
; BAR: rows 22-24, the white strip: your move menu when it is your
; turn, otherwise WAITING ON <name> -- except between hands and at the
; showdown, when lastResult is the news (the server leaves it holding
; the PREVIOUS hand's winner all through a live hand).
BAR:    DAT     22,0
        DCOL    ACARD
        LODI,R1 0
        LODI,R3 48              ; the three rows are contiguous cells
        BSTA,UN DFILL
        LODA,R0 V_ACT
        BCFA,EQ BARLR           ; not you (or nobody)
        LODA,R0 V_VIEW
        BCFA,EQ BARLR
        LODA,R0 V_MVC
        BCTA,EQ BARLR
        EORZ    R0              ; the menu: 2 entries per row
        STRA,R0 V_SEL
BARM:   LODA,R0 V_SEL
        COMA,R0 V_MVC
        BCFA,LT BARDN
        STRZ    R3
        LODA,R0 BROWT,R3
        STRZ    R1
        LODA,R0 BCOLT,R3
        STRZ    R2
        BSTA,UN DPOS
        DCOL    AREDW           ; the digit, red on white
        LODA,R0 V_SEL
        ADDI,R0 $11             ; glyph '1'+n
        BSTA,UN DPUTC
        DCOL    ACARD
        EORZ    R0
        BSTA,UN DPUTC
        LODA,R3 V_SEL           ; name: GOMOVES + n*13 + MVNAME
        LODI,R0 GOMOVES+MVNAME
BARML:  COMI,R3 0
        BCTR,EQ BARMD
        ADDI,R0 MVSTRID
        SUBI,R3 1
        BCTR,UN BARML
BARMD:  STRA,R0 RXPTR+1
        EORZ    R0
        STRA,R0 RXPTR
        LODI,R1 6
        BSTA,UN RXPRNT
        LODA,R0 V_SEL
        ADDI,R0 1
        STRA,R0 V_SEL
        BCTA,UN BARM
BARDN:  RETC,UN
BARLR:  LODA,R0 V_RND
        BCTR,EQ BARRES          ; between hands: the result stands
        COMI,R0 5
        BCFR,LT BARRES          ; showdown: the result is the news
        LODA,R0 V_ACT
        COMI,R0 $FF
        BCTR,EQ BARDN           ; nobody: the bar stays blank
        DAT     22,1
        DCOL    ACARD
        SETSTR  TWTON
        BSTA,UN DPRINT
        DAT     23,3
        LODA,R0 V_ACT
        BSTA,UN PLBASE
        LODI,R1 PLNAME
        BSTA,UN SETCUR
        LODI,R1 9
        BSTA,UN RXPRNT
        RETC,UN
BARRES: EORZ    R0              ; the last-result banner, 3 x 14
        STRA,R0 RXPTR
        STRA,R0 RXPTR+1
        DAT     22,1
        DCOL    ACARD
        LODI,R1 14
        BSTA,UN RXPRNT
        DAT     23,1
        LODI,R1 14
        BSTA,UN RXPRNT
        DAT     24,1
        LODI,R1 14
        BSTA,UN RXPRNT
        RETC,UN

; opponent band start rows, 7 per player count (2..8), index i-1
BANDROW: DB     8,0,0,0,0,0,0
        DB      5,11,0,0,0,0,0
        DB      3,8,13,0,0,0,0
        DB      2,6,10,14,0,0,0
        DB      2,5,8,11,14,0,0
        DB      2,4,6,8,10,12,0
        DB      2,4,6,8,10,12,14

BROWT:  DB      22,22,23,23,24  ; menu entry n -> bar row
BCOLT:  DB      0,8,0,8,0       ; and column

TJOING: DB      "JOINING...",0
TPOT:   DB      "POT",0
TWAIT:  DB      "WAIT",0
TSHOW:  DB      "SHOW",0
TRND:   DB      "RND",0
TVIEW:  DB      "VIEWING",0
TPURSE: DB      "PURSE",0
TWTON:  DB      "WAITING ON",0
