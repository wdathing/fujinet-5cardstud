; sound.inc -- three blocking beep cues.
;
; The 2637's sound registers are SHARED with the display: $18FD carries
; the multicolor bit in b7 (every frequency write keeps it) and $18FE
; b5-7 are the horizontal shift (volume writes touch b0-3 only, and the
; shift stays 0). Nothing outside this file writes either register after
; DINIT.

; BEEP: frequency R1 (0-$7F, higher = lower pitch), duration R3 frames.
; Calls one down (WAITVB). Clobbers R0,R3.
BEEP:   LODZ    R1
        IORI,R0 $80             ; keep multicolor
        STRA,R0 PITCH
        LODI,R0 $0F             ; enable + full loudness
        STRA,R0 VOLUME
BEEP1:  BSTA,UN WAITVB
        BDRR,R3 BEEP1
        EORZ    R0
        STRA,R0 VOLUME
        LODI,R0 $80
        STRA,R0 PITCH
        RETC,UN

; SNDKEY: key click. Calls two down. Clobbers R0,R1,R3.
SNDKEY: LODI,R1 $08
        LODI,R3 2
        BCTA,UN BEEP            ; tail call

; SNDSEL: selection confirm. Clobbers R0,R1,R3.
SNDSEL: LODI,R1 $10
        LODI,R3 4
        BCTA,UN BEEP

; SNDERR: low error burr. Clobbers R0,R1,R3.
SNDERR: LODI,R1 $60
        LODI,R3 12
        BCTA,UN BEEP

; SNDTRN: your-turn two-tone. Clobbers R0,R1,R3.
SNDTRN: LODI,R1 $18
        LODI,R3 6
        BSTR,UN BEEP
        LODI,R1 $0C
        LODI,R3 8
        BCTA,UN BEEP
