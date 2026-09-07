; fujilib.inc -- display, input and timing primitives.
;
; Derived from fujinet-firmware/pico/o2/testrom/fujilib.inc. The mailbox half of
; that file lives in mailbox.inc instead, in the other bank, because every one
; of its callers is there; what is left here is what the screens draw with.
;
; LOW bank ($400-$7FF), so the state machine and the screens that sit alongside
; it call this without any MB juggling at all. The high-bank callers -- there
; are about a dozen, against ninety for the mailbox -- pay one `sel mb1` each.
;
; Register conventions, which are not optional -- reading a variable through r0
; in the middle of a draw silently redirects every character after it:
;   r0  MOVX address / VDC register walker      r4  glyph_ptr scratch
;   r1  buffer pointer                          r5  glyph_ptr scratch
;   r2  loop counter, NPARAM into fn_begin      r6  Y position / fn_reg scratch
;   r3  X position, TXLEN_HI into fn_begin      r7  pointer bias / fn_reg index
;
; fn_reg clobbers r0 and r6, which is why fn_begin reads its device argument out
; of r6 before the first call rather than after.

; Stick codes. Deliberately not the firmware tree's numbering, whose wait_input
; folds LEFT onto FIRE.
J_NONE  equ 0
J_UP    equ 1
J_DOWN  equ 2
J_LEFT  equ 3
J_RIGHT equ 4
J_FIRE  equ 5

; fn_fire's bound: 200 polls x 40 rounds, each round about a millisecond of
; delay, so roughly 8-13 seconds depending on the console's clock. Long enough
; for a cold TLS handshake, short enough that a dead link still returns.
FN_POLLS  equ 200
FN_ROUNDS equ 40

IN_REPEAT equ 8                 ; frames between auto-repeats of a held stick

    org 420h                    ; Pinned, section by section. Conditional jumps
                                ; on the 8048 are page-relative, so a routine
                                ; that grows across a 256-byte boundary stops
                                ; assembling -- and if sections floated, a fix
                                ; in one would move the conditionals in the
                                ; next. tools/checklayout.py catches a section
                                ; that outgrows its slot.

; ---------------------------------------------------------------------------
; vdc_init: blank the display and clear the power-on garbage.
;
; The quad registers matter as much as the singles and the sprites: leaving
; $40-$7F alone shows up as bars in the top-left.
; ---------------------------------------------------------------------------
vdc_init:
    selvdc
    mov r0,#vdc_control
    clr a
    movx @r0,a
    mov r0,#vdc_spr0_ctrl
    mov r2,#16
    call vfill
    mov r0,#vdc_spr0_shape
    mov r2,#32
    call vfill
    mov r0,#vdc_char0
    mov r2,#48
    call vfill
    mov r0,#vdc_quad0
    mov r2,#64
    call vfill
    mov r0,#vdc_color
    mov a,#GRIDCOL              ; felt, and the colour of the two rules on it
    movx @r0,a
    jmp grid_init
vfill:
    clr a
vfl:
    movx @r0,a
    inc r0
    djnz r2,vfl
    ret

    org 63Eh
; gfx_on / gfx_off: the VDC will not accept changes to the character records
; while the display is enabled (o2doc 4.0), so every redraw blanks first.
;
; gfx_on turns the GRID on with the characters. The grid is one colour for the
; whole screen and its lattice is fixed -- lines every 16 pixels across and every
; 24 down, the first at y=24 -- so it cannot be used freely here. What it can do
; is rule the two lines that happen to fall in the gaps between this program's
; three text rows: y=24 sits between the top line and the cards, y=48 just under
; the cards. Two rules framing the hand, and nothing crosses a glyph.
gfx_on:
    mov r0,#vdc_control
    mov a,#vdc_ctrl_fore|vdc_ctrl_grid
    movx @r0,a
    ret
gfx_off:
    mov r0,#vdc_control
    clr a
    movx @r0,a
    ret

; grid_init: rule two lines, and clear the rest of a grid that has never been
; written. $C0-$E9 come up as whatever the VDC powered on with, and vdc_init has
; always left them alone -- harmless while the grid was disabled, garbage the
; moment it is not.
;
; $C0-$C8 are the nine COLUMNS, and bit j of each is that column's segment on
; row j. So bits 0 and 1 in all nine is two full-width rules, at y=24 and y=48.
grid_init:
    mov r0,#vdc_gridh0
    mov r2,#9
    mov a,#GRIDROWS
    call vfl
    mov r0,#vdc_gridi0          ; the ninth column of each row: off
    mov r2,#9
    call vfill
    mov r0,#vdc_gridv0          ; and no verticals at all
    mov r2,#10
    jmp vfill

    org 450h                    ; back into the vdc_init section

; ---------------------------------------------------------------------------
; wait_frame: block until the next vertical blank. P1 = P1_SEL_VDC.
;
; Bit 3 of the VDC status register is the VBLANK flag. On hardware it is a 40us
; pulse at the start of blanking; o2em holds it for the rest of the frame.
; Waiting for it to be clear BEFORE waiting for it to set is what makes one call
; mean one frame under both -- polling only for the set edge would return twice
; inside a single 40us window on hardware.
; ---------------------------------------------------------------------------
wait_frame:
    mov r0,#vdc_status
wf_out:
    movx a,@r0
    anl a,#vdc_stat_vblank
    jnz wf_out
wf_in:
    movx a,@r0
    anl a,#vdc_stat_vblank
    jz wf_in
    ret

; delay: block r2 frames. P1 = P1_SEL_VDC.
;
; Out of line, in the run entry.inc left free, because vdc_init above grew by the
; byte its P1 select costs now and this section had no slack at all. The djnz is
; page-relative and targets delay itself, so all that matters is that the five
; bytes do not straddle $700.
    org 623h
delay:
    call wait_frame
    djnz r2,delay
    ret

    org 468h                    ; glyph_ptr's conditionals need a clean page
; ---------------------------------------------------------------------------
; Text output
; ---------------------------------------------------------------------------

; glyph_ptr: write the character-generator pointer for the ASCII byte in A into
; the VDC record at r0 (low byte, then colour + 9th bit), advancing r0 by 2.
; r7 = the row's bias, -(Y/2) held as an unsigned byte. P1 = P1_SEL_VDC.
; Clobbers r4, r5.
;
; The VDC adds (Y>>1) to the stored pointer while it draws, so the register
; value for glyph N on row Y is N*8 - Y/2 -- nine bits, with bit 8 in bit 0 of
; the control byte next to the colour. The table holds glyph indices rather than
; finished pointers, so that arithmetic happens here: 64 bytes of table instead
; of 192, which is what let the URL literals into page $E00 and freed page $D00
; for code.
;
; The nine-bit add is the fiddly part. glyph*8 is 256*h + l with h set for
; glyph >= 32; the bias is negative, held as B = 256 + bias, so
;   glyph*8 + bias = 256*(h - 1 + c) + r   where l + B = 256*c + r
; and bit 8 comes out as h when the add carries and h flipped when it does not.
;
; The charmap only covers $20-$5F, so lowercase folds here too. The wire is
; lowercase throughout ("as", "th", "fold"), so that runs on nearly every
; character the game draws.
glyph_ptr:
    add a,#0E0h                 ; index = c - $20
    jnc gp_blank
    mov r4,a
    anl a,#0C0h
    jz gp_have
    mov a,r4                    ; $60-$7F: fold to uppercase and re-test
    add a,#0E0h
    mov r4,a
    anl a,#0C0h
    jz gp_have
gp_blank:
    mov r4,#0                   ; index 0 is ' ' -- the table starts at $20
gp_have:
    mov a,r4
    add a,#(cm_glyph-0E00h)&0FFh
    call cm_get
    sel mb0
    mov r4,a
    anl a,#20h                  ; bit 8 of glyph*8
    jz gp_h0
    mov a,#1
gp_h0:
    mov r5,a
    mov a,r4
    anl a,#01Fh
    rl a
    rl a
    rl a                        ; (glyph & 31) * 8, and bit 7 was clear each time
    add a,r7
    mov r4,a
    jc gp_ptr                   ; carried: bit 8 stands
    mov a,r5                    ; did not: bit 8 flips
    xrl a,#1
    mov r5,a
gp_ptr:
    mov a,r4
    movx @r0,a
    inc r0
    mov a,r5                    ; the 9th pointer bit, 0 or 1
    ; The colour comes from V_COL rather than a constant, so the card row can
    ; paint a heart red and a hole card blue. Reading it needs an indirect
    ; register and both are taken -- r0 is walking the VDC, r1 is the caller's
    ; buffer pointer -- so this borrows register bank 1's r0 for two
    ; instructions. fn_push is the only other user of rb1, and it transacts;
    ; nothing transacts mid-draw.
    sel rb1
    mov r0,#V_COL
    orl a,@r0
    sel rb0
    movx @r0,a
    inc r0
    ret

; show_char: draw the ASCII byte in A as the single character whose record base
; is r0, at Y = r6, X = r3, bias r7. Advances r0 by 4 and r3 by 8.
show_char:
    mov r5,a
    mov a,r6
    movx @r0,a                  ; +0 Y
    inc r0
    mov a,r3
    movx @r0,a                  ; +1 X
    inc r0
    mov a,r5
    call glyph_ptr              ; +2/+3 pointer; r0 ends past the record
    mov a,r3
    add a,#8
    mov r3,a
    ret

; show_row: draw V_TEXT across the 12 single characters. r5 = 0 for the menu
; row (12 characters at a tight 8-pixel pitch), 1 for the game row (five
; rank/suit pairs with a real gap between cards, then the two markers at the
; row's outer edges).
;
; Every single character carries its own X register, so the gap between cards
; costs nothing but this loop. It is the whole reason the cards go on the
; singles and the text goes on the quads.
show_row:
    mov r6,#CARDY
    mov r7,#CARDB
    call col_plain              ; menu text is plain; the card branch overrides
    mov r0,#vdc_char0           ; this per card
    mov r1,#V_TEXT
    mov a,r5                    ; read before the first show_char, which owns r5
    jnz sr_cards
    mov r3,#TEXTX
    mov r2,#TEXTLEN
sr_menu:
    mov a,@r1
    inc r1
    call show_char
    djnz r2,sr_menu
    ret
sr_cards:
    mov r3,#CARDX
    mov r2,#5
sr_pair:
    inc r1                      ; peek the suit: it colours the whole card, so it
    mov a,@r1                   ; has to be read before the rank is drawn
    dec r1
    call card_col
    mov a,@r1
    inc r1
    call show_char              ; rank; X advances 8, onto the suit
    mov a,@r1
    inc r1
    call show_char              ; suit; X advances 8
    mov a,r3
    add a,#8                    ; ...and 8 more, so cards do not run together
    mov r3,a
    djnz r2,sr_pair
    call col_plain              ; the markers are not part of a card
    mov r3,#MARKLX
    mov a,@r1
    inc r1
    call show_char
    mov r3,#MARKRX
    mov a,@r1
    call show_char
    ret

; show_quad: draw 4 bytes from r1 as one quad group, base r0, Y = r6, X = r3.
;
; The VDC mirrors a write to the group's Y/X across all four sub-records, so
; only the first pair is written and the other three records' own Y/X bytes are
; stepped over. One group is 4 glyphs at 16-pixel pitch spanning 64 pixels --
; o2em's draw_quad advances `off += 16` past each glyph, on top of the 8 the
; glyph itself occupies. r0 ends 18 past the base, NOT 16, so a caller drawing
; the second group has to reload it.
show_quad:
    mov a,r6
    movx @r0,a                  ; +0 Y
    inc r0
    mov a,r3
    movx @r0,a                  ; +1 X
    inc r0
    mov r2,#4
sq_l:
    mov a,@r1
    inc r1
    call glyph_ptr
    inc r0                      ; step over the next record's own Y and X,
    inc r0                      ; which the VDC ignores for all but the first
    djnz r2,sq_l
    ret

; show_quads: draw V_TOP and V_BOT as the two 8-character lines. quad0 at x=24
; and quad1 at x=88 are 64 apart -- exactly one group's span -- so their eight
; glyphs land on an even 16-pixel pitch and read as one line. Same for quad2 and
; quad3 on the row below. r1 carries over from the left group into the right
; half of each buffer.
show_quads:
    call col_plain              ; the quad lines are never coloured, and the move
    mov r6,#QTOPY               ; UI repaints them without going through show_row
    mov r7,#QTOPB
    mov r1,#V_TOP
    mov r0,#vdc_quad0
    call sq_pair
    mov r6,#QBOTY
    mov r7,#QBOTB
    mov r1,#V_BOT
    mov r0,#vdc_quad2
sq_pair:
    mov r3,#QLX
    call show_quad
    mov a,r0                    ; show_quad ends 18 past the base, and the next
    add a,#0FEh                 ; group starts 16 past it
    mov r0,a
    mov r3,#QRX
    jmp show_quad

; say: fill the singles row from the page-$E00 string at offset A and draw it.
; Five screens do exactly this; twelve bytes apiece was worth a helper.
;
; Also out of line, for the same reason: draw below grew by a byte and this
; section was full. say has no conditional jumps, so it does not care where it
; lands.
    org 628h
say:
    mov r1,#V_TEXT
    mov r2,#TEXTLEN
    call str_fill
    mov r5,#0
    jmp draw

    org 6A0h
; ---------------------------------------------------------------------------
; Card colour. A card is one colour across both its characters, and the suit
; byte is what decides it, so the pair is peeked before either is drawn.
;
;   h, d  red        s, c  white        ?  blue, a face-down card
;
; The wire is lowercase throughout, but the fold costs one instruction and makes
; this independent of that. Writing V_COL borrows register bank 1's r0, because
; the caller's r0 is walking the VDC and its r1 is walking the draw buffer --
; the same borrow glyph_ptr makes, for the same reason.
; ---------------------------------------------------------------------------
card_col:
    orl a,#020h
    mov r4,a
    xrl a,#'h'
    jz cc_red
    mov a,r4
    xrl a,#'d'
    jz cc_red
    mov a,r4
    xrl a,#'?'
    jz cc_back
col_plain:                      ; also the entry for ordinary text
    mov a,#CMAP_COL
    jmp cc_set
cc_red:
    mov a,#col_chr_red
    jmp cc_set
cc_back:
    mov a,#col_chr_blue
cc_set:
    sel rb1
    mov r0,#V_COL
    mov @r0,a
    sel rb0
    ret

    org 6D0h                    ; out of line beside say and delay: show_quads
                                ; above grew when it took over resetting V_COL,
                                ; and there is no conditional jump in draw to
                                ; care where it lands
; draw: paint all three lines. r5 = 0 for the 12-character menu row, 1 for the
; five card pairs plus the markers. Blanks the display first -- the VDC refuses
; character-record changes while it is enabled (o2doc 4.0) -- and re-enables at
; the end. Sets P1 itself, so a caller coming out of a mailbox transaction does
; not have to remember to.
draw:
    selvdc
    mov r0,#vdc_spr0_ctrl       ; the table view's spotlight caret. A sprite with
    clr a                       ; y=0 is off, and clearing it here means the hand
    movx @r0,a                  ; view never has to know the caret exists
    call gfx_off
    call show_row
    call show_quads
    jmp gfx_on

    org 528h
; ---------------------------------------------------------------------------
; Numbers and strings
; ---------------------------------------------------------------------------

; num3: write r6:r5 (a little-endian u16 from the wire) as three ASCII digits at
; r1, clamped to 999. Advances r1 by 3. Clobbers r3, r4, r5, r6.
;
; Repeated subtraction rather than a divide: the 8048 has none, the values are
; small, and this runs a handful of times per poll.
num3:
    mov a,r6
    add a,#0FCh                 ; -4: carry means the value is at least $400
    jc n3_max
    mov a,r6
    xrl a,#3
    jnz n3_go
    mov a,r5                    ; high byte 3: 1000 is $03E8
    add a,#018h                 ; -$E8
    jnc n3_go
n3_max:
    mov a,#'9'
    mov @r1,a
    inc r1
    mov @r1,a
    inc r1
    mov @r1,a
    inc r1
    ret
n3_go:
    mov r3,#'0'
n3_h:                           ; hundreds, subtracting across both bytes
    mov a,r5
    add a,#09Ch                 ; -100
    mov r4,a
    jc n3_hkeep
    mov a,r6
    jz n3_hdone                 ; borrow with nothing to borrow from: under 100
    dec r6
n3_hkeep:
    mov a,r4
    mov r5,a
    inc r3
    jmp n3_h
n3_hdone:
    mov a,r3
    mov @r1,a
    inc r1
    mov r3,#'0'
n3_t:                           ; tens; the value is under 100 by now
    mov a,r5
    add a,#0F6h                 ; -10
    jnc n3_tdone
    mov r5,a
    inc r3
    jmp n3_t
n3_tdone:
    mov a,r3
    mov @r1,a
    inc r1
    mov a,r5
    add a,#'0'
    mov @r1,a
    inc r1
    ret

; num2: write A as two ASCII digits at r1, advancing r1 by 2. Only the move
; countdown uses this, and the server's moveTime is well under 100, so there is
; no clamp -- a wild value draws a letter rather than costing 25 bytes to guard.
num2:
    mov r3,#'0'
n2_l:
    add a,#0F6h                 ; -10
    jnc n2_u
    mov r4,a
    inc r3
    mov a,r4
    jmp n2_l
n2_u:
    add a,#10
    mov r4,a
    mov a,r3
    mov @r1,a
    inc r1
    mov a,r4
    add a,#'0'
    mov @r1,a
    inc r1
    ret

; str_fill: copy the NUL-terminated string at page-$E00 offset A into r2 bytes
; at r1, padding the rest with spaces. Strings share page $E00 with the charmap
; and are read through the same MOVP stub.
str_fill:
    mov r3,a
    mov r6,#1                   ; 1 = still inside the string
sf_l:
    mov a,r6
    jz sf_pad
    mov a,r3
    call cm_get
    sel mb0
    jnz sf_put
    mov r6,#0
sf_pad:
    mov a,#' '
sf_put:
    mov @r1,a
    inc r1
    inc r3
    djnz r2,sf_l
    ret

    org 596h
; ---------------------------------------------------------------------------
; Input
; ---------------------------------------------------------------------------

; joy_poll: read stick 0 and return a J_* code in A. Leaves P1 on the VDC.
;
; The keyboard and joysticks can only be scanned from internal ROM -- P20-P23
; double as A8-A11 and toggle on every external fetch -- so this goes through
; the BIOS entry point rather than poking the port. The port configuration is
; not optional and not a quiet failure: with the VDC still on the bus the read
; never reaches the joystick path and returns 0, which decodes as every
; direction pressed at once rather than as centred.
joy_poll:
    seljoy
    mov r1,#0
    call getjoystick
    selvdc           ; put the port back before anything else looks
    jf0 jp_fire
    mov a,r3
    jz jp_horz
    xrl a,#0FFh
    jz jp_up
    mov a,#J_DOWN
    ret
jp_up:
    mov a,#J_UP
    ret
jp_horz:
    mov a,r2
    jz jp_none
    xrl a,#0FFh
    jz jp_left
    mov a,#J_RIGHT
    ret
jp_left:
    mov a,#J_LEFT
    ret
jp_fire:
    mov a,#J_FIRE
    ret
jp_none:
    clr a
    ret

; input_get: one frame of input. Returns a J_* code to act on, or J_NONE.
;
; Directions auto-repeat while held (a lock countdown, as the Intellivision port
; does with inp_lock); FIRE reports once per physical press. Without that edge,
; one press of the only button this console has walks through several menus at
; once -- the same class of bug as a still-held CLEAR closing the Intellivision
; menu on the frame it opened it.
input_get:
    call joy_poll
    mov r5,a
    mov r0,#V_PREV
    mov a,@r0
    mov r6,a                    ; r6 = the previous frame's code
    mov a,r5
    mov @r0,a
    jnz ig_held
    mov r0,#V_LOCK              ; centred: arm the next press to fire at once
    clr a
    mov @r0,a
    ret                         ; A is already J_NONE
ig_held:
    mov a,r5
    xrl a,#J_FIRE
    jz ig_fire
    mov r0,#V_LOCK
    mov a,@r0
    jz ig_go
    dec a
    mov @r0,a
    clr a
    ret
ig_go:
    mov a,#IN_REPEAT
    mov @r0,a
    mov a,r5
    ret
ig_fire:
    mov a,r6                    ; edge only: a held button reports nothing
    xrl a,#J_FIRE
    jz ig_hold
    mov a,#J_FIRE
    ret
ig_hold:
    clr a
    ret
