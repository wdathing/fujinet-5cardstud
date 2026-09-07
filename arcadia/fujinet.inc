; fujinet.inc -- N: device commands and the 5 Card Stud wire format.
;
; The values are hand-synchronised with astrocade/fujinet.inc and
; o2/o2fuji.inc (which carries the family's documentation of WHY each one
; is what it is); this file restates the numbers in AS 2650 syntax. The
; mailbox transport itself is mailbox.inc.

; ---- Network device (N:) ----------------------------------------------
NETDEV  EQU     $71             ; N1:
NCOPEN  EQU     $4F             ; 'O'
NCCLOSE EQU     $43             ; 'C'
NCREAD  EQU     $52             ; 'R'
NCSTAT  EQU     $53             ; 'S'

NMHTTPG EQU     $0C             ; open mode: HTTP GET
NTRNONE EQU     0               ; translation: none

; NET_STATUS reply: 4 bytes in slice 0.
NSAVLO  EQU     0               ; bytes waiting, low
NSAVHI  EQU     1               ; bytes waiting, high
NSCONN  EQU     2               ; connected flag
NSDEVST EQU     3               ; nDevStatus_t; 1 = SUCCESS. Checking this is
                                ; not optional: the GET is deferred until the
                                ; first STATUS, and an HTTP error page still
                                ; has a readable body (o2/net.inc).

; ---- Wire format (?bin=1, uint16 little-endian) -----------------------
; /state, /move/XX, /leave -> Game; /tables -> Tables. Offsets are read in
; place out of the reply slice window; nothing is buffered.
GOLASTR EQU     0               ; lastResult, 81 bytes
GOROUND EQU     81              ; 0 waiting, 1-4 streets, 5 showdown
GOPOT   EQU     82              ; u16
GOACTPL EQU     84              ; signed, $FF = -1
GOMVTIM EQU     85              ; seconds
GOVIEW  EQU     86
GOMVCNT EQU     87
GOMOVES EQU     88              ; 5 slots, always
GOPLCNT EQU     153
GOPLYRS EQU     154

MVSTRID EQU     13
MVCODE  EQU     0               ; 3 bytes; the first 2 go in the /move/XX path
MVNAME  EQU     3               ; 10 bytes

PLSTRID EQU     33
PLNAME  EQU     0               ; 9 bytes
PLSTAT  EQU     9               ; 0 waiting, 1 playing, 2 folded, 3 left
PLBET   EQU     10              ; u16
PLMOVE  EQU     12              ; 8 bytes, the seat's last action
PLPURSE EQU     20              ; u16
PLHAND  EQU     22              ; 11 bytes: 5 cards x 2 ASCII chars + NUL

TBSTRID EQU     36
TBID    EQU     0               ; 9 bytes
TBNAME  EQU     9               ; 21 bytes
TBPLYRS EQU     30              ; 6 bytes, literal "cur / max"

GMAXLEN EQU     418
GMINLEN EQU     154             ; fixed prefix through playerCount, 0 players
TMAXLEN EQU     361
