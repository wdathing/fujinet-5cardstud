;
;	Joystick input for 5 Card Stud (MSX)
;
;	unsigned char readJoystick(void);
;
;	Returns a bitmask matching src/msx/joystick.h:
;	  bit0 up, bit1 right, bit2 down, bit3 left, bit4 trigger
;
;	Ports 1 and 2 only. GTSTCK/GTTRIG id 0 is the cursor keys and the
;	space bar, which readCommonInput() already handles through the
;	keymap - reading it here would step every menu twice.
;
;	The BIOS entries are called directly rather than through z88dk's
;	msx_get_stick()/msx_get_trigger(): those live in msxbios.lib, and
;	linking that library displaces routines the --generic-console driver
;	needs, which wrecks the SCREEN 2 display.
;

	SECTION	code_user

	PUBLIC	readJoystick, _readJoystick

	GTSTCK	EQU	0x00D5		; A = stick id -> A = direction 0..8
	GTTRIG	EQU	0x00D8		; A = trigger id -> A = 0 or 0xFF

readJoystick:
_readJoystick:
	push	ix			; the BIOS is free to clobber both
	push	iy

	ld	a,1
	call	GTSTCK
	call	stick_to_mask
	push	hl			; stash port 1's mask

	ld	a,2
	call	GTSTCK
	call	stick_to_mask
	pop	de
	ld	a,l
	or	e			; merge both sticks
	push	af

	ld	a,1
	call	GTTRIG
	or	a
	jr	nz,have_trig		; pressed, don't bother with port 2
	ld	a,2
	call	GTTRIG
	or	a
have_trig:
	pop	hl			; H = merged direction mask (from push af)
	ld	a,h
	jr	z,no_trig		; Z still set from the last "or a"
	or	0x10
no_trig:
	ld	l,a
	ld	h,0

	pop	iy
	pop	ix
	ret

;	A = GTSTCK direction code (0..8) -> L = direction mask, H = 0
stick_to_mask:
	ld	hl,dir_table
	ld	e,a
	ld	d,0
	add	hl,de
	ld	l,(hl)
	ld	h,0
	ret

dir_table:
	defb	0x00			; 0 centered
	defb	0x01			; 1 up
	defb	0x03			; 2 up + right
	defb	0x02			; 3 right
	defb	0x06			; 4 right + down
	defb	0x04			; 5 down
	defb	0x0C			; 6 down + left
	defb	0x08			; 7 left
	defb	0x09			; 8 left + up
