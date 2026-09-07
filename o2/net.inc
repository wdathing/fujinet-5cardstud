; net.inc -- URL building, the HTTP round trip, and the name appkey.
;
; HIGH bank. Every call from here down into fujilib is followed by `sel mb1`,
; because AS emits the SEL MB0 before a cross-bank call and nothing after it;
; tools/checkmb.py fails the build if a restore is missing.
;
; The URL is streamed straight into the mailbox's TX byte stream a character at
; a time -- it is never assembled in RAM, because there are 128 bytes of
; external RAM in the whole machine and a /state URL is about 75 of them.
;
; P1 = P1_SEL_XRAM throughout, so a bare MOVX reaches both the mailbox registers
; ($E0-$E3) and external RAM ($00-$7F).

    org 934h
; ---------------------------------------------------------------------------
; ---------------------------------------------------------------------------

; net_cmd: start a request to the network device. A = command, r2 = nparam.
; The four net_* below differed only in those two values and a payload.
net_cmd:
    mov r6,#NET_DEVICEID
    mov r3,#0
    jmp fn_begin

; net_open: NET_OPEN the URL selected by r3. Returns A = 1 on ACK.
;
; The parameters must be pushed BEFORE the payload -- the stream is
; NPARAM x {size, value} and then TXLEN raw bytes -- so build_url runs from
; inside here, between the parameters and fn_txlen. (fn_begin's TXLEN_HI comes
; from r3 and is therefore briefly the URL selector; fn_txlen overwrites both
; halves before FN_W_SEQ ever fires the transaction.)
net_open:
    mov r2,#2                   ; the URL selector is in X_URL: it cannot ride in
    mov a,#NETCMD_OPEN          ; a register across fn_begin, which uses r3 for
    call net_cmd                ; TXLEN_HI and r5 for the device id
    mov a,#OPEN_MODE_HTTP_GET_H
    call fn_par1
    mov a,#OPEN_TRANS_NONE
    call fn_par1
    call build_url
    mov r0,#V_ULEN
    mov a,@r0
    mov r5,#0
    call fn_txlen
    call fn_fire
    call fn_ack
    ret

; net_status: NETCMD_STATUS. On success V_AVH:V_AVL is the byte count waiting
; and A = 1; on any failure A = 0.
;
; The 4th reply byte is nDevStatus_t, and checking it is not optional. For an
; HTTP devicespec the GET itself is deferred until this first STATUS call (see
; NetworkProtocolHTTP::status_file() in fujinet-firmware) and its result lands
; there -- an HTTP error response still has a real, readable body, so the byte
; count alone cannot tell a 400 page apart from a good answer.
net_status:
    mov r2,#2
    mov a,#NETCMD_STATUS
    call net_cmd
    clr a
    call fn_par1
    clr a
    call fn_par1
    call fn_fire
    call fn_ack
    jnz ns_ok
    ret
ns_ok:
    mov a,#FN_R_DATA&0FFh       ; the cart republishes slice 0 after every reply
    call fn_read
    mov r0,#V_AVL
    mov @r0,a
    mov a,#(FN_R_DATA+1)&0FFh
    call fn_read
    mov r0,#V_AVH
    mov @r0,a
    mov a,#(FN_R_DATA+3)&0FFh
    call fn_read
    xrl a,#1                    ; 1 = SUCCESS
    jz ns_good
    clr a
    ret
ns_good:
    mov a,#1
    ret

    org 984h
; net_read: NETCMD_READ of V_AVH:V_AVL bytes, clamped to GAME_MAXLEN (418, the
; largest struct this client reads and larger than TABLES_MAXLEN). A = 1 on ACK.
net_read:
    mov r0,#V_AVH
    mov a,@r0
    add a,#0FEh                 ; -2: carry means at least $200, far too big
    jc nr_clamp
    mov r0,#V_AVH
    mov a,@r0
    jz nr_len                   ; under 256: always fine
    mov r0,#V_AVL               ; high byte 1: compare the low against 418's
    mov a,@r0
    add a,#(256-(GAME_MAXLEN&0FFh))&0FFh
    jnc nr_len
nr_clamp:
    mov r0,#V_AVL
    mov a,#GAME_MAXLEN&0FFh
    mov @r0,a
    mov r0,#V_AVH
    mov a,#GAME_MAXLEN/256
    mov @r0,a
nr_len:
    mov r2,#1
    mov a,#NETCMD_READ
    call net_cmd
    mov r0,#V_AVH
    mov a,@r0
    mov r5,a
    mov r0,#V_AVL
    mov a,@r0
    mov r4,a
    mov a,#2                    ; the only two-byte parameter this client sends
    call fn_push
    mov a,r4
    call fn_push
    mov a,r5
    call fn_push
    call fn_fire
    call fn_ack
    ret

; net_close: always issued, whatever the read did.
net_close:
    mov r2,#0
    mov a,#NETCMD_CLOSE
    call net_cmd
    call fn_fire
    ret

; rx_grab: copy the cart's reported reply length into X_RXLO/X_RXHI.
;
; This has to happen between the READ and the CLOSE. FN_R_RXLEN belongs to the
; most recent transaction, and the CLOSE that always follows a read reports zero
; bytes -- so reading it afterwards says every reply was empty and every length
; check fails. The Intellivision port keeps #net_gotlen for the same reason.
rx_grab:
    mov a,#FN_R_RXLEN_LO&0FFh
    call fn_read
    mov r0,#X_RXLO
    movx @r0,a
    mov a,#FN_R_RXLEN_HI&0FFh
    call fn_read
    mov r0,#X_RXHI
    movx @r0,a
    ret

; rx_len: that captured length, into r6 (high) : r5 (low).
rx_len:
    mov r0,#X_RXLO
    movx a,@r0
    mov r5,a
    mov r0,#X_RXHI
    movx a,@r0
    mov r6,a
    ret

    org 9DFh                   ; page-aligned: api_call's settle loop is a
                                ; conditional jump over its own length
; api_call: r3 = which URL (0 tables, 1 state, 2 move). One full round trip.
; Returns A = 1 with the reply in the window and the cursor homed, else 0.
;
; The CLOSE belongs at the START of the next request, not the end of this one.
; publish_slice() in the cartridge firmware fills the whole 208-byte window from
; the latest reply and ZEROES whatever the reply does not cover -- so a CLOSE,
; whose reply is empty, wipes the data the READ just delivered. The window has to
; stay live until the next poll anyway: every screen in this game renders out of
; it between polls rather than buffering anything.
;
; (The Intellivision client closes immediately and gets away with it because its
; cartridge writes only rxlen bytes into FN_RX and leaves the rest of the buffer
; alone. That is luck, not design, and it does not hold here.)
;
; The settle loop is the Intellivision port's: the ESP32 can report STATUS as
; soon as SOME of the response has arrived, well before the rest, and
; open+status+read in one breath easily outruns it. Rather than guess a delay,
; poll until two consecutive readings agree, bounded so a stuck connection still
; falls through to the ordinary failure path instead of hanging here.
api_call:
    mov a,#P1_SEL_XRAM
    outl p1,a
    mov r0,#X_URL
    mov a,r3
    movx @r0,a
    call net_close              ; the PREVIOUS request's connection; on the first
    mov r0,#X_URL               ; call there is none and the NAK is ignored
    movx a,@r0
    mov r3,a
    call net_open
    jnz ac_open
    jmp ac_fail
ac_open:
    mov r0,#X_PREVAV            ; no previous reading yet
    clr a
    movx @r0,a
    mov r0,#X_PREVAVH
    clr a
    movx @r0,a
    mov r7,#20
    org 0A00h
ac_settle:
    call net_status
    jnz ac_have
    jmp ac_closefail
ac_have:
    mov r0,#V_AVL
    mov a,@r0
    mov r3,a
    mov r0,#V_AVH
    mov a,@r0
    orl a,r3
    jz ac_again                 ; nothing has arrived at all yet
    mov r0,#X_PREVAV
    movx a,@r0
    xrl a,r3
    jnz ac_again
    mov r0,#V_AVH
    mov a,@r0
    mov r3,a
    mov r0,#X_PREVAVH
    movx a,@r0
    xrl a,r3
    jz ac_settled               ; the same reading twice running: it has landed
ac_again:
    mov r0,#V_AVL
    mov a,@r0
    mov r0,#X_PREVAV
    movx @r0,a
    mov r0,#V_AVH
    mov a,@r0
    mov r0,#X_PREVAVH
    movx @r0,a
    mov a,#P1_SEL_VDC           ; wait_frame reads the VDC status register
    outl p1,a
    call wait_frame
    sel mb1
    mov a,#P1_SEL_XRAM
    outl p1,a
    djnz r7,ac_more             ; the hop is two bytes; the JMP that follows
    jmp ac_closefail            ; never settled: treat it as a failed poll
ac_more:
    jmp ac_settle               ; carries eleven address bits and no page limit
ac_settled:
    call net_read
    mov r0,#X_OK                ; not a register: rx_grab and rx_home both go
    movx @r0,a                  ; through routines that use r3
    call rx_grab
    mov r0,#X_OK
    movx a,@r0
    jz ac_fail
    call rx_home                ; the cart republished slice 0; resync V_SLICE
    mov a,#1
    ret
ac_closefail:
    call net_close
ac_fail:
    clr a
    ret
