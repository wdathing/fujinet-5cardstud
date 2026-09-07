; url.inc -- building the devicespec for a request.
;
; HIGH bank, with the mailbox it feeds and the strings it reads, so none of this
; pays a SEL MB. It is a separate file only because net.inc had grown into two
; unrelated halves.
;
; The URL is streamed straight into the mailbox's TX byte stream a character at
; a time. It is never assembled in RAM: there are 128 bytes of external RAM in
; the whole machine and a /state URL is about 75 of them.
;
; P1 = P1_SEL_XRAM throughout, so a bare MOVX reaches both the mailbox registers
; ($E0-$E3) and external RAM ($00-$7F).

    org 8CAh
; ---------------------------------------------------------------------------
; URL construction
; ---------------------------------------------------------------------------

; u_put: push A into the TX stream and count it. V_ULEN becomes the payload
; length fn_txlen publishes; a URL over 255 bytes would silently wrap it, which
; is the cap endpoint.inc documents.
u_put:
    call fn_push
    mov r0,#V_ULEN
    mov a,@r0
    inc a
    mov @r0,a
    ret

; u_str: push the NUL-terminated string at page-$E00 offset A.
u_str:
    mov r1,a
us_l:
    mov a,r1
    call cm_get                 ; same bank -- no MB juggling
    jz us_done
    call u_put
    inc r1
    jmp us_l
us_done:
    ret

; u_xstr: push the NUL-terminated string in external RAM at r1, at most r2
; bytes.
;
; Stopping at the NUL is the point, not a nicety: these are fixed-width padded
; fields, and pushing the padding puts a literal $00 and whatever follows it
; into the middle of a URL. The length is a byte count, not a terminator, so
; nothing downstream stops there either -- it corrupts the request on the wire.
; The Intellivision port learned this as fn_strlen.
u_xstr:
    mov a,r1
    mov r0,a
    movx a,@r0
    jz ux_done
    call u_put
    inc r1
    djnz r2,u_xstr
ux_done:
    ret

; build_url: X_URL selects 0 tables, 1 state, 2 move. Pushes the whole
; devicespec and leaves its length in V_ULEN.
build_url:
    mov r0,#X_URL
    movx a,@r0
    mov r3,a
    mov r0,#V_ULEN
    clr a
    mov @r0,a
    mov a,#(url_ncolon-0E00h)&0FFh
    call u_str
    mov a,#(url_endpoint-0E00h)&0FFh
    call u_str

    mov a,r3
    jz bu_tables
    jmp bu_play
bu_tables:
    mov a,#(url_tables-0E00h)&0FFh      ; /tables?bin=1 -- no table, no player
    call u_str
    ret

bu_play:
    xrl a,#2
    jz bu_move
    mov a,#(url_state-0E00h)&0FFh
    call u_str
    jmp bu_query
bu_move:
    mov a,#(url_move-0E00h)&0FFh        ; /move/XX -- the 2-character code
    call u_str
    mov r0,#X_MVA
    movx a,@r0
    call u_put
    mov r0,#X_MVB
    movx a,@r0
    call u_put

bu_query:
    mov a,#(url_qtable-0E00h)&0FFh
    call u_str
    mov r1,#X_TABLE
    mov r2,#8
    call u_xstr
    mov a,#(url_aplayer-0E00h)&0FFh
    call u_str
    mov r1,#X_NAME
    mov r2,#8
    call u_xstr
    mov a,#(url_abin-0E00h)&0FFh
    call u_str
    ret

; The round trip
