#!/usr/bin/env python3
"""Fail a build when the bytes a bank switch runs through are not the same in
every bank.

Changing banks changes the code under the PC. The instruction that does it and
every byte fetched after it come from the NEW bank, so those bytes have to be
identical, at the same address, in the bank being left and the bank being
entered. That -- not "the low 1K matches" -- is the actual requirement, and it
is a good thing it is the weaker one: the low 1K CANNOT match while ordinary
code selects peripherals with `outl p1,a`, because every such immediate carries
its own bank's select bits (see tools/checkp1.py).

So the invariant is narrow and exact:

  * far_go sits at the same address in every bank and has the same bytes there
  * bank_entry, which far_go jumps to, sits at the same address in every bank
  * the whole low bank is the same in every bank -- it has to be, since a
    routine down there runs under whichever bank called it. That is only
    possible because resident code selects peripherals with ORL/ANL rather than
    OUTL, so P10/P11 survive the write. bank_entry at $700 is the one exception,
    and being per-bank is its entire purpose
  * the reset vectors are part of that, and they matter twice over: o2em's
    -resetat path
    restarts the 8048 WITHOUT re-pointing `rom` (vmachine.c:186 calls init_cpu,
    which sets p1=$FF and nothing else), so the $400 fetch after a reset comes
    from whichever bank was live -- and on real hardware the console BIOS runs
    before $400 and picks the bank with whatever it last left in P1
  * fn_get, the two bytes at $F00, is the same in every bank -- MOVP reads the
    page the PC is in, so every bank's fn_read jumps there and every bank needs
    it. Only those two bytes: the rest of $F00-$F1F is ordinary private space,
    and rx_atleast lives there in bank 0 alone

and three things about the image as a whole that are cheap to get wrong:

  * it is exactly BANKS x 3072 bytes, or o2em maps it as something else entirely
    (a 3-bank image becomes the MegaCart romlatch scheme -- main.c:849)
  * bank 0 is the LAST chunk and carries the "FUJI" claim, or the cartridge
    disables the mailbox for the session (o2map.c:46-59)
  * no bank's $40C-$40F reads "OPNB", which flips o2em into Videopac+ mode
    (main.c:855) -- a false positive there renders through a different path
    entirely and nothing would connect the two
"""
import re
import sys

# The whole low bank, minus the two bytes of bank_entry, which is per-bank by
# definition -- dispatching is its job. Everything else down here selects
# peripherals with ORL/ANL, which leaves P10/P11 alone, so the bytes come out
# the same in all four banks and a switch anywhere inside is survivable.
RESIDENT  = [(0x400, 0x6FF), (0x720, 0x7FF)]
DISPATCH  = (0x700, 0x71F)      # bank_entry's window, per-bank by definition
STUB      = (0xF00, 0xF01)      # fn_get alone: see below
CLAIM     = 0xF2C
CLAIM_SIG = b"FUJI"
OPNB      = (0x40C, 0x40F)
BANK_LEN  = 3072
BASE      = 0x400

LINE  = re.compile(r"^(?:\(\d+\))?\s*\d+/\s*([0-9A-F]{3,4}) : ((?:[0-9A-F]{2} )+)")
LABEL = re.compile(r" : (?:(?:[0-9A-F]{2} )+)?\s*([a-z_][a-z_0-9]*):")


def listing(path):
    """-> (bytes by address, address of each label)."""
    mem, at = {}, {}
    label = None
    for line in open(path, errors="ignore"):
        m = LABEL.search(line)
        if m:
            label = m.group(1)
        m = LINE.match(line)
        if not m:
            continue
        addr = int(m.group(1), 16)
        if label is not None:
            at.setdefault(label, addr)
        for i, b in enumerate(m.group(2).split()):
            mem[addr + i] = (int(b, 16), label)
    return mem, at


def check(stem, nbanks):
    bad = []
    mems, ats, imgs = [], [], []
    for n in range(nbanks):
        mem, at = listing("%s_b%d.lst" % (stem, n))
        mems.append(mem)
        ats.append(at)
        imgs.append(open("%s_b%d.bin" % (stem, n), "rb").read())

    for n, img in enumerate(imgs):
        if len(img) != BANK_LEN:
            bad.append("bank %d is %d bytes, not %d" % (n, len(img), BANK_LEN))

    def byte(n, a):
        return imgs[n][a - BASE]

    def same(lo, hi, what):
        for a in range(lo, hi + 1):
            vals = {byte(n, a) for n in range(nbanks)}
            if len(vals) > 1:
                bad.append("%s: $%03X differs across banks (%s)"
                           % (what, a, ", ".join("bank %d=$%02X" % (n, byte(n, a))
                                                 for n in range(nbanks))))
                return

    for lo, hi in RESIDENT:
        same(lo, hi, "resident $%03X-$%03X" % (lo, hi))
    same(*STUB, "page $F00 stub")

    # far_go and bank_entry, located rather than hardcoded, so relocating them
    # cannot quietly stop them being checked.
    for name in ("far_go", "bank_entry"):
        addrs = {n: ats[n].get(name) for n in range(nbanks)}
        if any(a is None for a in addrs.values()):
            bad.append("%s: not found in %s"
                       % (name, ", ".join("bank %d" % n for n, a in addrs.items() if a is None)))
            continue
        if len(set(addrs.values())) > 1:
            bad.append("%s sits at different addresses: %s"
                       % (name, ", ".join("bank %d=$%03X" % (n, a) for n, a in addrs.items())))

    if "far_go" in ats[0]:
        span = sorted(a for a, (_, lab) in mems[0].items() if lab == "far_go")
        if not span:
            bad.append("far_go emits no bytes")
        else:
            same(span[0], span[-1], "far_go")

    if imgs[0][CLAIM - BASE:CLAIM - BASE + 4] != CLAIM_SIG:
        bad.append("bank 0 has no %r claim at $%03X -- the cartridge would "
                   "disable the mailbox for the session" % (CLAIM_SIG.decode(), CLAIM))
    for n in range(nbanks):
        if imgs[n][OPNB[0] - BASE:OPNB[1] - BASE + 1] == b"OPNB":
            bad.append("bank %d reads 'OPNB' at $%03X: o2em would switch to "
                       "Videopac+ rendering" % (n, OPNB[0]))

    if bad:
        print("%s: RESIDENT MISMATCH" % stem, file=sys.stderr)
        for b in bad:
            print("  " + b, file=sys.stderr)
        return 1

    differ = sum(1 for a in range(BASE, BASE + BANK_LEN)
                 if len({byte(n, a) for n in range(nbanks)}) > 1)
    print("%s: %d bank(s) of %d bytes; switch path identical, %d byte(s) "
          "private to a bank" % (stem, nbanks, BANK_LEN, differ))
    return 0


if len(sys.argv) != 3:
    print("usage: checkresident.py <build/stem> <banks>", file=sys.stderr)
    sys.exit(2)
sys.exit(check(sys.argv[1], int(sys.argv[2])))
