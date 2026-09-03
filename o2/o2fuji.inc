; o2fuji.inc -- mailbox map, wire format, screen layout and RAM map.
;
; The mailbox half is an 8048 mirror of
; fujinet-firmware/pico/o2/firmware/include/fuji_mailbox.h. Hand-synchronised;
; do not change one without re-checking the other. Same arrangement
; intv/fujinet.bas has with the Intellivision cartridge's copy of that header,
; and for the same reason: this is a client, and a client that guesses at the
; protocol fails in ways that look like hardware faults.

; ---------------------------------------------------------------------------
; console -> cart: MOVX writes, external-data window (P13 high, P14 low, P16 low)
; ---------------------------------------------------------------------------
; $E0-$E3 is not a taste decision. The Voice decodes writes to $80-$DF and
; $F0-$FF as sound triggers and $E4/$E8-$EF as bank selects, so $E0-$E3 and
; $E5-$E7 are the only Voice-safe addresses in the whole window -- which is why
; the write side is an indexed register pair rather than a flat file, and why
; PicoPAC's $FE/$FF handshake garbles with a Voice attached.
FN_W_REGSEL     equ 0E0h
FN_W_REGDATA    equ 0E1h
FN_W_DATA       equ 0E2h        ; auto-incrementing TX byte stream
FN_W_SEQ        equ 0E3h        ; written LAST; launches the transaction

FN_REG_DEVICE   equ 000h
FN_REG_CMD      equ 001h
FN_REG_NPARAM   equ 002h
FN_REG_TXLEN_LO equ 003h
FN_REG_TXLEN_HI equ 004h
FN_REG_DATA_RST equ 005h
FN_REG_RXSLICE  equ 006h

; ---------------------------------------------------------------------------
; cart -> console: MOVP reads out of program page $F00
; ---------------------------------------------------------------------------
; The connector carries no ~RD, so a cart answering a MOVX read has to drive the
; bus off a level condition and can collide. A ~PSEN read is just ROM, which the
; cart already serves -- so the reply lives in a ROM page the cart rewrites
; between transactions.
FN_R_ACKSEQ     equ 0F20h
FN_R_STATUS     equ 0F21h
FN_R_ERR        equ 0F22h
FN_R_REPLY_CMD  equ 0F23h
FN_R_RXLEN_LO   equ 0F24h
FN_R_RXLEN_HI   equ 0F25h
FN_R_MAGIC0     equ 0F29h       ; 'F' -- cart presence check
FN_R_MAGIC1     equ 0F2Ah       ; 'N'
FN_R_DATA       equ 0F30h       ; reply slice selected by FN_REG_RXSLICE

; The reply window is the whole page tail: 208 bytes, $F30-$FFF. That number
; matters -- FN_R_SLICE_LEN in fuji_mailbox.h and publish_slice() in
; firmware/src/fujimail.c both use it, while the firmware tree's fujilib.inc
; fn_rx assumes 128-byte slices at $F80 and ORs the offset with $30, so offsets
; 0/16/32/48 all alias onto the same byte. rx_adv/rx_next below do not: a slice
; is exactly the page tail, so an 8-bit page pointer wrapping past $FF IS the
; slice boundary, and no division by 208 happens anywhere.
FN_R_SLICE_LEN  equ 0D0h

FUJICMD_ACK     equ 006h
FUJICMD_NAK     equ 015h

; Fuji device (config/appkey).
FUJI_DEVICEID           equ 070h
FUJICMD_OPEN_APPKEY     equ 0DCh
FUJICMD_CLOSE_APPKEY    equ 0DBh
FUJICMD_READ_APPKEY     equ 0DDh
FUJICMD_WRITE_APPKEY    equ 0DEh

; Network device (N:).
NET_DEVICEID    equ 071h
NETCMD_OPEN     equ 04Fh
NETCMD_CLOSE    equ 043h
NETCMD_READ     equ 052h
NETCMD_STATUS   equ 053h

OPEN_MODE_HTTP_GET_H equ 00Ch
OPEN_TRANS_NONE      equ 000h

; Lobby appkey: creator 1 / app 1 / key 0 is the username shared by every
; FujiNet client on this server (src/misc.h). Key 1 is the room, which this port
; does not use -- there is no Odyssey 2 lobby to hand off a table.
AK_CREATOR_LO   equ 1
AK_CREATOR_HI   equ 0
AK_APP          equ 1
AK_KEY_NAME     equ 0
AK_MODE_READ    equ 0
AK_MODE_WRITE   equ 1

; ---------------------------------------------------------------------------
; Wire format -- server/util.go's appendFixedLengthString binary layout, as
; requested with ?bin=1. uint16 fields are little-endian (the server's default;
; the &be=1 path is exercised by the CoCo client alone and had a byte-order bug,
; so every other client in the family asks for LE). Offsets are read in place
; out of the reply window; nothing is ever buffered.
; ---------------------------------------------------------------------------
GAME_LASTRESULT     equ 0       ; 81 bytes
GAME_ROUND          equ 81      ; 0 waiting, 1-4 streets, 5 showdown
GAME_POT            equ 82      ; u16 LE
GAME_ACTIVEPLAYER   equ 84      ; signed, $FF = -1
GAME_MOVETIME       equ 85      ; seconds
GAME_VIEWING        equ 86
GAME_VALIDMOVECOUNT equ 87
GAME_VALIDMOVES     equ 88      ; 5 slots, always
GAME_PLAYERCOUNT    equ 153
GAME_PLAYERS        equ 154

MOVE_STRIDE     equ 13
MOVE_CODE       equ 0           ; 3 bytes; the first 2 go in the /move/XX path
MOVE_NAME       equ 3           ; 10 bytes

PLAYER_STRIDE   equ 33
PL_NAME         equ 0           ; 9 bytes
PL_STATUS       equ 9           ; 0 waiting, 1 playing, 2 folded, 3 left
PL_BET          equ 10          ; u16 LE
PL_MOVE         equ 12          ; 8 bytes, the seat's last action -- "CALL",
                                ; "RAISE", "FOLD". Only the table view reads it;
                                ; the hand view has no room for it.
PL_PURSE        equ 20          ; u16 LE
PL_HAND         equ 22          ; 11 bytes: 5 cards x 2 ASCII chars + NUL

TABLE_STRIDE    equ 36
TBL_ID          equ 0           ; 9 bytes
TBL_NAME        equ 9           ; 21 bytes
TBL_PLAYERS     equ 30          ; 6 bytes, literal "cur / max"

GAME_MAXLEN     equ 418
GAME_MINLEN     equ 154         ; fixed prefix through playerCount, 0 players
TABLES_MAXLEN   equ 361

; ---------------------------------------------------------------------------
; Screen layout
; ---------------------------------------------------------------------------
; The VDC offers 12 single characters and 4 quad groups of 4, and quad
; characters are drawn at DOUBLE pitch -- 16 pixels apart, not 8 (o2em's
; draw_quad advances `off += 16` past each glyph). A group therefore spans 64
; pixels, so quad0 at x=24 and quad1 at x=88 put their eight glyphs on an even
; 16-pixel pitch across x=24..136: one 8-character line. Two of those, plus the
; 12 singles at tight 8-pixel pitch, is the entire canvas. 28 glyphs.
;
; The singles are the only place text reads at 8-pixel pitch, so they carry the
; cards -- five rank/suit pairs with a real gap between cards.

QTOPY   equ 8                   ; top quad line: pot and your purse
QBOTY   equ 72                  ; bottom quad line: name+bet, or the move menu
QLX     equ 24                  ; left group of a quad line
QRX     equ 88                  ; right group; 88-24 = 64 = one group's span
CARDY   equ 40                  ; the singles row. Equal to CMAP_YP, so the card
                                ; row alone needs no character-pointer bias.
TEXTX   equ 40                  ; 12-char tight text row (menus), x=40..128
CARDX   equ 24                  ; first card; pairs then run 24,32 48,56 72,80
                                ; 96,104 120,128 -- tight inside a card, 16
                                ; pixels between cards
MARKLX  equ 8                   ; singles 10 and 11: the row's outer edges,
MARKRX  equ 144                 ; clear of every card position

; The table view uses the same three rows, so the bias constants above serve it
; unchanged. The only new geometry is its middle row: eight glyphs on the SAME
; 16-pixel lattice the quad groups sit on, so each seat's last move lands
; directly under that seat's initial.
TVX     equ QLX                 ; x=24, under the first glyph of quad0
TVSEATS equ 8                   ; the server's maximum, and the screen's

; The grid: rules in the gaps between the three text rows. Its lattice is fixed
; -- columns every 16 pixels, rows every 24 starting at y=24 -- and a character
; is SEVEN glyph rows drawn two scanlines each, so a row at y covers y..y+13:
;
;   8-21   the top line          24  rule -- clear
;   40-53  the cards             48  strikes straight through them
;   72-85  the bottom line       72  strikes through those
;                                96  rule -- clear, below everything
;
; So two of the four available rows are unusable, and the two that are left are
; a line under the top row and a baseline under the whole table.
GRIDROWS equ 009h               ; bits 0 and 3: rows at y=24 and y=96

; Sprite 0 marks the spotlit seat on the table view, in the band between the
; first rule and the move row -- 28..35, which is why the caret is four rows of
; shape and not eight: a full sprite is sixteen scanlines and would reach the
; moves at y=40.
TVSPRY  equ 28
TVSPRW  equ 16                  ; the seat pitch, and the caret's step
GRIDCOL  equ col_bck_green|col_grd_white|col_grd_lum

; Character-pointer bias for a row at Y. The VDC adds Y/2 to the pointer as it
; draws, so the stored value has to have it taken back out; glyph_ptr adds this
; to glyph*8 as a nine-bit sum.
BIAS    function y,(-(y/2))&0FFh
QTOPB   equ BIAS(QTOPY)
QBOTB   equ BIAS(QBOTY)
CARDB   equ BIAS(CARDY)

; ---------------------------------------------------------------------------
; Internal RAM. $00-$07 is register bank 0, $08-$17 the stack, $18-$1F register
; bank 1 -- so $20-$3F is every byte of scratch the 8048 nominally has, and it
; is all spoken for by the draw buffers. Internal RAM is the one store readable
; no matter how P1 is configured, which is why the draw buffers live here.
;
; $19-$1F is reclaimed on top of that: fn_push is the only routine in this
; program that ever selects register bank 1, and it uses r0' ($18) alone. The
; other seven bytes are therefore ordinary scratch, addressed as memory. They
; hold the per-frame state that would otherwise cost a P1 flip through
; xget/xput on every single frame.
; ---------------------------------------------------------------------------
V_ULEN   equ 019h               ; bytes pushed into the URL so far
V_AVL    equ 01Ah               ; NET_STATUS "bytes available", low
V_AVH    equ 01Bh               ; ...and high
V_HAVEMV equ 01Ch               ; a move is staged for the next request
V_POLL   equ 01Dh               ; frames until the next poll
V_PREV   equ 01Eh               ; previous stick code, for the FIRE edge
V_LOCK   equ 01Fh               ; stick auto-repeat countdown
V_SLICE  equ 020h               ; reply slice currently published by the cart
V_RXP    equ 021h               ; page pointer into the window, $30..$FF
V_SPOT   equ 022h               ; spotlighted player (0 = you)
V_COL    equ 023h               ; colour bits for the next glyph glyph_ptr draws.
                                ; This byte used to be V_MVSEL. Internal RAM is
                                ; full -- $24-$3F is the 28 draw buffers, one per
                                ; glyph on screen -- so per-card colour had to
                                ; displace something, and the highlighted move is
                                ; the one piece of screen state never touched
                                ; during a draw.
V_TEXT   equ 024h               ; 12 bytes: the singles row (cards or menu text)
V_TOP    equ 030h               ; 8 bytes: top quad line
V_BOT    equ 038h               ; 8 bytes: bottom quad line
TEXTLEN  equ 12
QUADLEN  equ 8

; ---------------------------------------------------------------------------
; External RAM, $00-$7F (o2doc 3.0: "128 bytes of RAM external to the
; processor... set P14 low and P13 high"). Everything that must survive a poll
; but is not needed mid-draw lives here, reached through xget/xput.
; ---------------------------------------------------------------------------
X_NAME    equ 000h              ; player name, 8 + NUL
X_TABLE   equ 009h              ; table id, 8 + NUL
X_PC      equ 012h              ; playerCount, this poll
X_ROUND   equ 013h              ; round, this poll
X_ACT     equ 015h              ; activePlayer ($FF = nobody).  These four are
X_TIME    equ 016h              ; the wire's offsets 84-87 in order, and are
X_VIEW    equ 017h              ; consecutive here so read_state copies them
X_MVCNT   equ 018h              ; with one loop instead of four stores.
X_MVA     equ 019h              ; staged move code, 2 chars
X_MVB     equ 01Ah
X_TBLSEL  equ 01Bh              ; highlighted table
X_TBLCNT  equ 01Ch              ; tables in the reply
X_NECUR   equ 01Dh              ; name-entry cursor
X_FRAME   equ 01Eh              ; frames elapsed in the current countdown second
X_PREVAV  equ 01Fh              ; previous NET_STATUS reading, low
X_PREVAVH equ 020h              ; ...and high
X_PREVACT equ 021h              ; previous poll's activePlayer, for the turn edge
X_PREVRD  equ 022h              ; previous poll's round.
                                ;
                                ; This is how the end-of-hand message is
                                ; detected. The Intellivision port sums the
                                ; first 20 bytes of lastResult and watches that
                                ; sum change; here `round` is a byte already
                                ; being read and it says the same thing -- 5 is
                                ; the showdown, and a drop means the next hand
                                ; has been dealt -- for about eighty fewer
                                ; bytes, which was a quarter of what the bank
                                ; had left.
X_MVSEL   equ 027h              ; highlighted move, evicted from internal RAM by
                                ; V_COL. Every reader already has XRAM selected.
X_MODE    equ 028h              ; 0 the hand, 1 the table. Left or right flips it
                                ; when it is not your turn; your turn forces the
                                ; hand back, because the move UI is drawn there.
X_PAGE    equ 02Ah              ; how far into lastResult show_result has got
X_RET     equ 029h              ; which bank 3 entry the table view returns to.
                                ; The switch is a jump both ways, so the way back
                                ; is data rather than a return address.
X_URL     equ 025h              ; which URL a transaction is building
X_OK      equ 026h              ; a flag that has to outlive a transaction
X_RXLO    equ 023h              ; the reply length the cart reported, captured
X_RXHI    equ 024h              ; between the READ and the CLOSE
X_NEBUF   equ 030h              ; name-entry buffer, 8 character codes

RESULT_HOLD equ 80              ; frames each 12-character page of the end-of-hand
                                ; message holds the card row
RESULT_LEN  equ 81              ; lastResult's field width on the wire

; ---------------------------------------------------------------------------
; P1 configurations. Getting these wrong is not a quiet failure: with the VDC
; still selected a joystick read never reaches the joystick path and returns 0,
; which decodes as every direction pressed at once rather than as centred.
; P16 must stay low in both, or the cartridge write strobe is gated off.
; ---------------------------------------------------------------------------
P1_SEL_XRAM equ P1_BANKSEL|P1_KBD|P1_VDC|P1_NC|P1_LUM
