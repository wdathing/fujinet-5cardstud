#!/usr/bin/env python3
"""Fail a build when a P1 write carries the wrong cartridge bank's select bits.

P10 and P11 are the cartridge bank-select lines, and the cart decodes
`bank = 3 - (P10 + 2*P11)` (picopac_cart.c:212) -- inverted, so both high at
reset picks bank 0. They sit in the same port as the VDC / external-RAM / joystick
selects, and the 8048 has no way to write part of a port with OUTL. So EVERY

    mov a,#P1_SEL_VDC
    outl p1,a

is also a bank write, and a routine assembled into bank 2 that loads a bank-0
constant switches banks *at the OUTL* and executes the very next instruction out
of the wrong bank. Nothing diagnoses it: it assembles clean, and at runtime the
program continues at the right address in the wrong 3K.

This is the same shape as the MB trap that tools/checkmb.py catches, and it is
caught the same way -- out of the AS listing, because a raw byte stream cannot be
walked without knowing where instructions begin.

The rule is exact rather than a heuristic: in bank N every immediate loaded into
A immediately before an `outl p1,a` must have low two bits (3 - N). The P1
constants in vdctext.inc derive from BANK precisely so this holds by
construction; the checker is here for the hand-written exception that eventually
gets added.

Two sites deliberately break the rule, and both exist to change banks:

  far_go  the trampoline, which takes the target bank's P1 value in A
  start   the reset prologue, which forces bank 0 with a hard immediate --
          the same bytes in all four banks, so that whichever bank happens to
          be serving the $400 fetch after a reset, the next fetch is bank 0's
"""
import re
import sys

MOV_A_IMM = 0x23
OUTL_P1_A = 0x39
ORL_P1_D  = 0x89        # sets the bits in the operand
ANL_P1_D  = 0x99        # clears the bits NOT in the operand

EXEMPT = {"far_go", "start"}

# Same shape as checkmb.py's: an address, the bytes, then the source text.
LINE = re.compile(r"^(?:\(\d+\))?\s*\d+/\s*([0-9A-F]{3,4}) : ((?:[0-9A-F]{2} )+)\s{2,}(.*)$")

# A label, and ONLY a label: it has to sit at the very start of the listing's
# source column. Matching `\bword:` anywhere on the line -- which is what a first
# cut does -- picks up file citations in comments ("vmachine.c:186" reads as a
# label named "c"), and since EXEMPT is keyed on the label that silently moves
# the exemption onto the wrong routine.
LABEL = re.compile(r" : (?:(?:[0-9A-F]{2} )+)?\s*([a-z_][a-z_0-9]*):")


def check(path, bank):
    want = (3 - bank) & 3
    mem = {}
    sites = []
    rmw = []
    label = "(start)"

    for line in open(path, errors="ignore"):
        m = LABEL.search(line)
        if m:
            label = m.group(1)
        m = LINE.match(line)
        if not m:
            continue
        addr = int(m.group(1), 16)
        data = [int(b, 16) for b in m.group(2).split()]
        for i, b in enumerate(data):
            mem[addr + i] = b
        if data == [OUTL_P1_A]:
            sites.append((addr, label, m.group(3).strip()))
        if len(data) == 2 and data[0] in (ORL_P1_D, ANL_P1_D):
            rmw.append((addr, data[0], data[1], label, m.group(3).strip()))

    bad = []
    for addr, label, src in sites:
        if label in EXEMPT:
            continue
        # The immediate is the two bytes in front: 23 <data>, then 39.
        if mem.get(addr - 2) != MOV_A_IMM:
            bad.append((addr, label, src,
                        "P1 is written from a value this checker cannot see; "
                        "load it with `mov a,#const` or add the routine to EXEMPT"))
            continue
        imm = mem[addr - 1]
        if (imm & 3) != want:
            bad.append((addr, label, src,
                        "immediate $%02X selects bank %d, but this is bank %d "
                        "(wanted low bits %d)" % (imm, (3 - (imm & 3)) & 3, bank, want)))

    # The read-modify-write form, which resident code uses precisely because it
    # cannot disturb the bank. That property is not automatic: an ORL whose
    # operand has a bank bit SET would set it, and an ANL whose operand has one
    # CLEAR would clear it. Either way the bank moves and the next fetch comes
    # from somewhere else.
    for addr, op, dat, label, src in rmw:
        if op == ORL_P1_D and (dat & 3):
            bad.append((addr, label, src,
                        "orl p1,#$%02X sets bank bits; an ORL mask must have "
                        "bits 0-1 clear" % dat))
        if op == ANL_P1_D and (dat & 3) != 3:
            bad.append((addr, label, src,
                        "anl p1,#$%02X clears bank bits; an ANL mask must have "
                        "bits 0-1 set" % dat))

    if bad:
        print("%s: WRONG BANK IN A P1 WRITE" % path, file=sys.stderr)
        for addr, label, src, why in sorted(bad):
            print("  $%03X in '%s': %s -- %s" % (addr, label, src, why), file=sys.stderr)
        return 1

    print("%s: %d P1 write(s) selecting bank %d, %d read-modify-write(s) "
          "preserving it" % (path, len(sites), bank, len(rmw)))
    return 0


if len(sys.argv) != 3:
    print("usage: checkp1.py <listing> <bank>", file=sys.stderr)
    sys.exit(2)
sys.exit(check(sys.argv[1], int(sys.argv[2])))
