; cycle.inc -- the one list stepper, and why it is its own file.
;
; Every screen in this game is a list you step through with the stick: the
; spotlight over the seats, the move menu, the table list, the name cursor and
; the character under it. Six copies of the same six lines was most of a page,
; so there is one -- and because those screens are now spread across cartridge
; banks, the one copy has to be in a file each bank can include, at the same
; org, rather than buried in whichever bank happens to hold render.inc.
;
; Resident, in the low bank that entry.inc vacated. It has to be somewhere every
; bank can reach, and the alternative -- a private copy at the same org in each
; -- collided with ne_accept, which cannot move: name_entry reaches it with a
; page-relative `jz`, so the two are stuck in one page together.
;
; $608 and not the start of the free run at $5EE: cycle's own conditional jumps
; are page-relative too, and at $5EE its 27 bytes would straddle $600. Keeping
; the old offset of $08 keeps them all exactly where they were.

    org 608h
; ---------------------------------------------------------------------------
; cycle: A = current index, r2 = how many there are, r3 = 0 back / 1 forward.
; Returns the next index, wrapping at both ends.
;
; Every screen in this game is a list you step through with the stick -- the
; spotlight over seats, the move menu, the table list, the name cursor and the
; character under it. Six copies of the same six lines was most of a page.
; ---------------------------------------------------------------------------
cycle:
    mov r4,a
    mov a,r3
    jnz cy_fwd
    jmp cy_back
cy_fwd:
    mov a,r4
    inc a
    mov r4,a
    mov a,r2
    xrl a,r4
    jnz cy_done
    mov r4,#0
    jmp cy_done
cy_back:
    mov a,r4
    jnz cy_b1
    mov a,r2
    mov r4,a
cy_b1:
    mov a,r4
    dec a
    mov r4,a
cy_done:
    mov a,r4
    ret
