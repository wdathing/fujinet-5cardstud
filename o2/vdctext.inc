; vdctext.inc -- direct VDC character output, no BIOS.
;
; The VDC's character-pointer byte is a BYTE OFFSET into the 512-byte character
; generator, and the VDC adds (Y>>1) to it while drawing. Glyphs are 7 rows in
; an 8-byte slot, so for glyph index N at row Y the register value is
;   N*8 - (Y>>1)
; with bit 8 of that going into bit 0 of the char's control byte. Verified
; against o2em's cset.c and vdc.c:536 (`c = chr + ypos; if (col&1) c += 256`).

CHRPTR  function idx,y,(idx*8-(y/2))&0FFh          ; low byte -> char reg byte 2
CHRCTL  function idx,y,col,col|((idx*8-(y/2))>>8&1) ; colour + 9th pointer bit

; P1 bits (all active low unless noted)
P1_BANK0    equ 001h    ; P10 cart bank select, low bit
P1_BANK1    equ 002h    ; P11 cart bank select, high bit
P1_KBD      equ 004h    ; P12 0 = keyboard scanner enabled
P1_VDC      equ 008h    ; P13 0 = VDC on the bus
P1_XRAM     equ 010h    ; P14 0 = external RAM on the bus
P1_NC       equ 020h    ; P15 not connected
P1_COPY     equ 040h    ; P16 1 = RAM->VDC copy mode (also gates cart ~WR off)
P1_LUM      equ 080h    ; P17 1 = VDC luminance to the video mixer

; Cart bank select. The cartridge decodes bank = 3 - (P10 + 2*P11) -- the pair is
; INVERTED, so both bits high at reset selects bank 0, which is why bank 0 has to
; be the last chunk of the image file (picopac_cart.c:212, o2map.c:61-71).
;
; BANK is defined by the one-line wrapper build.sh generates for each bank. It
; matters far more than it looks: every `outl p1,a` in this program is also a
; bank write, so a routine assembled into bank 2 that loads a bank-0 P1 constant
; switches banks mid-instruction-stream and carries on executing at the next
; address in the WRONG bank -- silently, exactly like the MB trap.
; tools/checkp1.py fails the build when one carries the wrong bits.
P1_BANKSEL  equ (3-BANK)&003h

; The VDC-selected P1 value for some OTHER bank -- far_go's argument. The
; P1_SEL_* constants below are always this bank's; this names another's, and it
; is the only way a bank number is allowed to reach P1.
P1VDC   function n,(((3-n)&003h)|P1_KBD|P1_XRAM|P1_NC|P1_LUM)

; ---------------------------------------------------------------------------
; Selecting a peripheral WITHOUT touching the bank.
;
; P10/P11 share the port with the peripheral selects and the 8048 cannot write
; half a port with OUTL, so every `outl p1,a` is also a bank write. Read-modify-
; write is the way out, and it is not an invention: the console's own BIOS does
; exactly this. `vdcenable` ($0E7) is ORL P1,#$BC / ANL P1,#$B7 and
; `extramenable` ($0EC) is ORL P1,#$BC / ANL P1,#$AF -- byte for byte the masks
; below. (Its $37F-$38B bank helpers are the same trick pointed the other way:
; ANL P1,#$FC / ORL P1,#$03 move P10/P11 and nothing else.)
;
; A routine written this way selects a peripheral without knowing which bank it
; is running in, which is what lets the same bytes sit in all four banks. Byte-
; identical is in turn what makes a bank switch inside the resident half
; survivable -- and tools/checkresident.py is what proves it stayed that way.
;
; The invariant, which tools/checkp1.py checks: an ORL mask must have bits 0-1
; CLEAR and an ANL mask must have them SET. Either way round the bank moves.
; ---------------------------------------------------------------------------
P1_PARK  equ 0BCh               ; park KBD, VDC, XRAM, NC and LUM high
P1_MVDC  equ 0B7h               ; then drop one: VDC   (and COPY, always)
P1_MJOY  equ 0BBh               ;                KBD
P1_MXRAM equ 0AFh               ;                XRAM

selvdc  macro
    orl p1,#P1_PARK
    anl p1,#P1_MVDC
    endm

seljoy  macro
    orl p1,#P1_PARK
    anl p1,#P1_MJOY
    endm

selxram macro
    orl p1,#P1_PARK
    anl p1,#P1_MXRAM
    endm

; VDC selected: P13 low, everything else parked. Copy mode MUST stay low or the
; cartridge write strobe is disabled.
P1_SEL_VDC  equ P1_BANKSEL|P1_KBD|P1_XRAM|P1_NC|P1_LUM

; The same thing with HARD bank-0 bits, for the one write that has to force a
; bank rather than preserve one: the reset prologue. See start in 5card.a48.
P1_SEL_VDC0 equ P1_BANK0|P1_BANK1|P1_KBD|P1_XRAM|P1_NC|P1_LUM

; Joysticks and keyboard: VDC and external RAM both OFF the bus (P13, P14 high)
; and the scanner enabled (P12 low). Getting this wrong is not a quiet failure --
; with the VDC still selected the bus read never reaches the joystick path at
; all and returns 0, which is every direction pressed at once.
P1_SEL_JOY  equ P1_BANKSEL|P1_VDC|P1_XRAM|P1_NC|P1_LUM
