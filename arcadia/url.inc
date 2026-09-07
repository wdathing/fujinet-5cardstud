; url.inc -- building the devicespec for a request.
;
; The URL is streamed straight into the mailbox's TX byte stream a
; character at a time (the o2/astrocade way): it is never assembled in
; RAM. The cart counts the payload itself, so nothing here tracks a
; length; a whole /move request is ~90 bytes against the 320-byte TX
; ceiling. Page-1 code: literals append with one indexed load; the
; page-0 buffers (TBLBUF, PLNBUF -- NUL-terminated copies of fixed-width
; wire fields, pushed up to the NUL and never through it) stream through
; FNTXSTR in page 0.

; BLDURL: stream the devicespec for request (V_URL): 0 tables, 1 state,
; 2 move, 3 leave. Calls one down (FNTXSTR). Clobbers R0,R1,R3, FSTRP.
BLDURL: LODA,R0 UENDPT          ; "N:" unless the endpoint carries its own
        COMI,R0 'N'
        BCTR,EQ BURL1
        LODI,R1 'N'
        LODA,R0 FNDATA,R1
        LODI,R1 ':'
        LODA,R0 FNDATA,R1
BURL1:  SETFS1  UENDPT
        BSTA,UN FNTXSTR
        LODA,R0 *PVURL
        BCFR,EQ BURL2
        SETFS1  UTABLS          ; tables: no table, no player
        BCTA,UN FNTXSTR         ; tail call
BURL2:  COMI,R0 2
        BCTR,LT BURLST
        BCTR,EQ BURLMV
        SETFS1  ULEAVE
        BSTA,UN FNTXSTR
        BCTR,UN BURLQ
BURLST: SETFS1  USTATE
        BSTA,UN FNTXSTR
        BCTR,UN BURLQ
BURLMV: SETFS1  UMOVE
        BSTA,UN FNTXSTR
        LODA,R1 *PMOVB0         ; the move code's first 2 characters
        LODA,R0 FNDATA,R1
        LODA,R1 *PMOVB1
        LODA,R0 FNDATA,R1
BURLQ:  SETFS1  UQTAB
        BSTA,UN FNTXSTR
        SETFS1  TBLBUF
        BSTA,UN FNTXSTR
        SETFS1  UQPLR
        BSTA,UN FNTXSTR
        SETFS1  PLNBUF
        BSTA,UN FNTXSTR
        SETFS1  UQBIN
        BCTA,UN FNTXSTR         ; tail call

        INCLUDE "build/endpoint.inc"
UTABLS: DB      "tables?bin=1",0
USTATE: DB      "state",0
UMOVE:  DB      "move/",0
ULEAVE: DB      "leave",0
UQTAB:  DB      "?table=",0
UQPLR:  DB      "&player=",0
UQBIN:  DB      "&bin=1",0
