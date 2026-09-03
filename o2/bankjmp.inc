; bankjmp.inc -- crossing cartridge banks.
;
; A bank switch changes the code under the PC, so the instruction that does it
; and every byte fetched after it has to be IDENTICAL, at the same address, in
; the old bank and the new one. That is the whole requirement. It is NOT that
; the low 1K must match: it cannot, because each bank's P1 constants carry its
; own select bits, and every P1 write is a bank write. Only these four bytes
; have to agree, and tools/checkresident.py is what says they do.
;
; far_go is therefore the one place in the program where P1's bank bits change.
; tools/checkp1.py proves that every other P1 write leaves them exactly as they
; were, which is what makes "the bank never changes underneath you" a checked
; property rather than a convention.
;
; It is a JUMP, never a call. The screen flow is already a chain of jumps --
; start -> boot -> table_select -> game_start -- so nothing needs a cross-bank
; return, and there is no software return-bank stack to get wrong. The
; registers survive the switch untouched (they are internal RAM $00-$07), which
; is the only reason an argument can be passed at all: internal RAM above $18 is
; entirely spoken for by the draw buffers.

    org 7F0h                    ; Pinned, and pinned forever: this address is the
                                ; one thing all four banks have to agree on. Not
                                ; $7FC, the actual tail -- on the 8048 the PC's
                                ; low byte wraps inside its page during a fetch,
                                ; so a two-byte instruction may not put its
                                ; operand on $xFF, and AS refuses the JMP there.
; far_go: A = the VDC-selected P1 value for the target bank, from P1VDC(n).
far_go:
    outl p1,a                   ; the bank changes HERE, and the two bytes after
    jmp bank_entry              ; it are fetched from the NEW bank -- where they
                                ; are the same two bytes, because far_go is the
                                ; same in all four. Three bytes in total: the
                                ; dispatcher moved into this same half, so there
                                ; is no SEL MB in the middle of the switch at all.

    org 700h
; ---------------------------------------------------------------------------
; bank_entry: where far_go lands. Pinned at the same address in every bank,
; because far_go's JMP is one of the three bytes that has to read the same on
; both sides of the switch. Its CONTENTS are per-bank -- that is the whole job.
;
; $700, in the low bank entry.inc vacated: page-aligned, so a dispatcher that
; grows a selector has a clean page for its conditionals, and in the same half
; as far_go, so the switch carries no SEL MB.
;
; One entry per bank for now. A bank that grows a second screen gets a selector
; in a register here; nothing needs one yet.
; ---------------------------------------------------------------------------
bank_entry:
    if BANK=0
    jmp table_select            ; boot has a name; take it to the lobby
    elseif BANK=1
    jmp boot                    ; the splash, the cart check and the name picker
    elseif BANK=2
    jmp tv_render               ; all eight seats
    else
    ; Bank 3 is the only bank anything comes back TO, so it is the only one that
    ; needs a selector. r1 carries it, because the registers are internal RAM
    ; $00-$07 and survive the switch untouched -- which matters, since internal
    ; RAM above $18 is entirely draw buffers.
    mov a,r1
    jz be_sit
    dec a
    jz be_loop
    jmp be_turn
be_sit:
    jmp game_start              ; the lobby joined a table; sit down
be_loop:
    jmp gl_loop                 ; the table view painted; carry on polling
be_turn:
    jmp gl_turn                 ; ...and it painted a fresh poll, so check the
    endif                       ; round and whose turn it is
