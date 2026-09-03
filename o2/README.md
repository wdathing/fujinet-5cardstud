# 5 Card Stud for the Magnavox Odyssey 2 / Philips Videopac

@ericcarrgh's 5 Card Stud over FujiNet, on the hardest target in the family.

The VDC gives 12 single characters and 4 quad groups of 4, and quad characters
are drawn at **double pitch**, so the whole screen is 28 glyphs. There is 64
bytes of internal RAM, 128 of external, one joystick and one button. Eight seats
and a five-card hand will not fit in 28 glyphs, so this is not the C client's
table -- it is **two** screens, and left, right or fire swaps between them.

The hand:

```
 y=8    P 0 3 7   $ 1 9 3        the pot, and your purse
 y=40   0 ## 3S JC 9D 4H 1       the spotlit player's hand, seat and round
 y=72   M E G     0 1 0          that player's name and bet
                                  ...or, on your turn, the move and a countdown
```

The table:

```
 y=8    A C J K H F M            eight seat initials; yellow is whose turn it is
 y=28       ^                    a sprite caret on the seat the stick is browsing
 y=40   C R - F C - K            what each of them last did
 y=72   P 0 3 2   $ 1 8 7        the pot, and your purse
```

Stick up/down moves the spotlight across the seats on either screen -- 0 is
always you, per the server's rotation. Two grid rules frame the table; they are
the only two rows of the VDC's fixed lattice that do not cross a glyph.

Cards need no conversion at all: the wire sends two ASCII bytes per card
(`"as"`, `"th"`) and the charmap folds lowercase, so reply bytes are drawn as
they arrive. Colour is the one thing added on the way past -- hearts and
diamonds red, spades and clubs white, and a hole card blue. `"??"` is not drawn
as punctuation: `?` is mapped to the character generator's solid block, so two
of them side by side are 16 pixels of blue, which reads as the back of a card.

## Building and running

```sh
./build.sh                 # Macro Assembler AS + p2bin per bank, then four checks
./run.sh                   # o2em, patched, against a live fujinet-pc-rs232
```

`run.sh` rebuilds if a source is newer than the image, the way `intv/run.sh`
does. It needs the FujiNet-patched o2em from
`fujinet-firmware/pico/o2/emu/o2em-fujinet.patch` and a console BIOS, which is
copyrighted and not in this repo — `BIOSDIR` defaults to that tree's gitignored
`bios/`. `ENDPOINT=` overrides the compiled-in server at build time.

Everything runs headless:

```sh
./run.sh -input=150:f -frames=900 -dumptxt=1     # replay the stick, dump the VDC
./run.sh -resetat=400 -frames=700                # pulse console RESET
```

`-dumptxt=1` prints the VDC register file and an ASCII rendering, so a wrong
picture is traced to the registers that produced it rather than guessed at.

## Four banks of 3K

The cart is 12288 bytes: **four banks of 3072**, which is the hardware ceiling.
The FujiNet cartridge serves `fuji_rom[bank][A0..A11]` with
`bank = 3 - (P10 + 2*P11)` (`picopac_cart.c:212`) -- the select pair is
INVERTED, so both bits high at reset picks bank 0, and bank 0 therefore has to
be the LAST chunk of the file. `build.sh` concatenates back to front.

Nine kilobytes is not an option: o2em chooses its switching scheme from the file
size, and a 3-bank image lands on the MegaCart romlatch scheme instead of
P10/P11 (`main.c:843-851`). Two banks or four.

```
                        ---- the low bank, MB0 ----
$400-$41F   resident   reset vectors and the forced-bank-0 prologue
$420-$5ED   resident   fujilib: the VDC, timing, glyphs, rows and quads
$608-$622   resident   cycle, the one list stepper
$623-$6FF   resident   delay, say, card_col, and the rest of fujilib out of line
$700-$71F   per bank   bank_entry, this bank's dispatcher
$7F0-$7F2   resident   far_go, the trampoline
                        ---- the high bank, MB1 ----
$800-$8C9   per bank   the mailbox: transactions and the reply cursor
$8CA-$DFF   PRIVATE    bank 0: url, net, the lobby
                       bank 1: boot, the name picker, the appkey
                       bank 2: the table view
                       bank 3: url, net, render, the game loop and the move UI
$E00-$ED3   resident   charmap, strings, sound cues, URL literals
$ED4-$EFF   per bank   snd; and put_time in bank 3
$F00-$F01   resident   fn_get -- MOVP reads the page the PC is in
$F02-$F1F              rx_atleast, in the two banks that carry net.inc
$F20-$FFF   the mailbox page, written by the cartridge into every bank
```

Everything a screen calls is either in the low bank or in its own; only screen
transitions cross, and they cross as jumps. There are six of them, and that is
the whole surface: `start` to `boot`, `boot` to the lobby, the lobby to the game
loop, and the game loop to and from the table view.

Free space, which is the whole point of the exercise:

| bank | | free |
|---|---|---|
| 0 | the lobby | 810 |
| 1 | boot and the name picker | 969 |
| 2 | the table view | 1200 |
| 3 | the game loop | 276 |
| | the low bank, shared by all four | 171 |
| | **total** | **3426**, against 65 in the 3K cart |

The split follows what each screen needs underneath it. The lobby and the game
loop both sit on the whole network stack, and together with it they did not fit
in one 3K bank -- so they are banks 0 and 3, with a copy of `url.inc` and
`net.inc` each. That duplication is exactly what a bank is for.

Each bank is its own assembly at `$400-$FFF`, from the same sources, through a
one-line wrapper `build.sh` generates (`BANK equ n`). AS does have
`PHASE`/`DEPHASE`, which would let one assembly emit all four -- but it shifts
the listing's address column too, and then every bank appears to overlap every
other. Worse, `checkmb.py` looks up `mem[addr+3]` to find a restore, and four
banks writing the same addresses would make it return whichever bank assembled
last: the checker would start silently PASSING a missing restore. Four separate
assemblies keep all three checkers exact, and the fact that bank 1's source
cannot name a bank 2 symbol is a feature -- the trampoline becomes the only
possible way across, at assembly time.

## Crossing banks

A bank switch changes the code under the PC. The instruction that does it and
every byte fetched afterwards come from the NEW bank, so those bytes have to be
identical, at the same address, in both. That is the whole requirement, and it
is deliberately the narrow one -- the low 1K *cannot* be identical, because
every `outl p1,a` is also a bank write and each bank's P1 constants carry its
own select bits. 42 single bytes differ across the four banks for exactly that
reason. `tools/checkresident.py` checks the switch path, the vectors and page
`$F00`, and reports the rest as private.

```
    org 7F0h                ; pinned forever: the one address all four banks
                            ; have to agree on. Not the tail $7FC -- the 8048's
                            ; PC wraps inside its page during a fetch, so a
                            ; two-byte instruction may not put its operand on
                            ; $xFF, and AS refuses the JMP there.
far_go:
    outl p1,a               ; A = P1VDC(n); the bank changes HERE
    jmp bank_entry          ; AS supplies the SEL MB1 in front of this itself
```

It is a JUMP, never a call. The screen flow is already a chain of jumps --
`start` -> `boot` -> `table_select` -> `game_start` -- so nothing needs a
cross-bank return and there is no return-bank stack to get wrong. Arguments ride
in registers, which survive the switch because they are internal RAM `$00-$07`;
internal RAM above `$18` is entirely spoken for by the draw buffers.

`start` begins with a HARD bank-0 write rather than this bank's constant. Out of
reset P1 is `$FF`, so P16 is high and the cartridge write strobe is gated off
until something clears it -- and o2em's `-resetat` restarts the 8048 without
re-pointing `rom` (`vmachine.c:186` calls `init_cpu`, which sets `p1=$FF` and
nothing else), so a reset taken while another bank was live re-enters the cart
still reading that bank. On real hardware the console BIOS runs before `$400`
and picks the bank from whatever it last left in P1. The prologue is identical
in all four banks, so whichever one serves the fetch, the next fetch is bank 0's.

## Three traps, and three checkers

Each is silent, undiagnosed, and wrong only at runtime -- the same shape as the
MB trap this port already had to tool around.

- **`tools/checkp1.py`** -- every `outl p1,a` is a bank write. A routine in bank
  2 that loads a bank-0 P1 constant switches banks mid-instruction-stream and
  carries on at the next address in the wrong 3K. The checker requires each of
  the 42 sites to carry `(3-BANK)`. `far_go` and `start` are exempt by name,
  because changing banks is what they are for.
- **`tools/checkresident.py`** -- the switch path must be byte-identical. It
  locates `far_go` and `bank_entry` in each listing rather than hardcoding them,
  so relocating one cannot quietly stop it being checked. It also asserts the
  image is exactly `BANKS x 3072`, that bank 0 carries the `"FUJI"` claim, and
  that no bank reads `"OPNB"` at `$40C`, which would flip o2em into Videopac+
  rendering (`main.c:855`) in a way nothing would connect back to.
- **`tools/checkmb.py`** now also catches cross-bank **JMPs**. AS emits a
  `SEL MB` in front of one just as it does for a CALL, and `RET` does not
  restore MB -- so a routine that TAIL-jumps across the banks hands its caller
  back with MB pointing at the wrong half. Whether that is a bug depends on
  whether the target ever returns, which the bytes cannot say, so each one has
  to be declared in a named `ONEWAY` list.
- **`tools/checkmb.py` also catches a restore that is no longer needed**, which
  is the trap that bites when code MOVES between the halves -- as all of
  entry.inc just did. Reassembled into the same half as what it calls, AS stops
  emitting the SEL in front of the call, and the hand-written restore behind it
  becomes a live `sel mb` that sets MB the wrong way and never puts it back. AS
  does not track explicit SELs and will not save you. Every standalone SEL in
  this program is a restore, so the rule is exact: one has to sit three bytes
  past a cross-bank CALL. Twenty stray `sel mb0` came out of entry.inc in that
  move; this is what made removing them a mechanical job rather than a careful
  one.

There is a fourth trap the checkers cannot see, worth knowing before moving code
into the resident half: a resident routine that reads page `$E00` reads the
CURRENT bank's copy. That is what makes per-bank strings work, and it is also
what would silently give a resident routine the wrong literal.

## Both banks, and the trap

Two different things are called a bank here, and they are orthogonal. Above,
*cartridge* banks: four 3K images, selected by P10/P11, only one visible at a
time. Below, the 8048's *memory* bank: the MB flag that supplies the twelfth
address bit, splitting whichever cartridge bank is live into `$400-$7FF` and
`$800-$FFF`. Every cartridge bank has both halves, and the trap below applies
inside each one.

`CALL` and `JMP` carry 11 address bits; the twelfth comes from the MB flag. AS
emits the `SEL MB` *before* a cross-bank call and nothing after it, so a routine
above `$7FF` that calls one below resumes with MB=0 and its next intra-bank jump
lands 2K low, in the console BIOS — silently, with no diagnostic:

```
       800 : E5 94 05        call low_sub       ; E5 = SEL MB0
       803 : 04 00           jmp  high_sub      ; MB still 0 -> $000
```

The firmware tree avoids this by keeping every callable routine below `$800`.
That costs 1K, and `fujicfg` already fills it to `$7EF`. This game needs about
2.5K, so it uses both banks and restores MB after every cross-bank call instead.
`tools/checkmb.py` fails the build when a restore is missing — currently **57
cross-bank calls, all restored**. A run of consecutive cross-bank calls needs no
restore between them, because AS emits each call's own SEL; the checker knows
that, so the exception costs nothing and is still exact.

Conditional jumps are page-relative, which makes where a routine sits part of
whether it assembles. Every section is placed by an explicit `org`;
`tools/checklayout.py` catches one that outgrows its slot. Where a jump had to
reach across a whole routine, it is written as a two-byte hop into an
unrestricted `JMP` rather than left to depend on the layout.

## What the Intellivision port taught, and what it did not

Carried over unchanged: the sequence number comes from the cart's own
`FN_R_ACKSEQ`, never a local counter (`-resetat=400` proves it — the sequence
continues at 69, not 1); the NET_STATUS settle loop waits for two equal readings
rather than guessing a delay; `nDevStatus_t` is checked, because an HTTP error
page has a perfectly readable body; the reply length is validated against what
`playerCount` implies before anything is believed; the table is drawn *before*
the end-of-hand message, because the poll carrying the message is the one
carrying the revealed hands; fixed-width padded fields are pushed up to their
NUL, never through it.

Three things are different here, and each is a bug on this hardware:

- **`fn_rx` computes the wrong address.** The firmware tree's copy ORs the reply
  offset with `$30`, assuming 128-byte slices at `$F80`. The cartridge publishes
  **208-byte slices at `$F30`** (`publish_slice()` in `firmware/src/fujimail.c`),
  so offsets 0/16/32/48 all alias onto the same byte. The browser survives it
  because host slot 0's name lives at offsets 0-11; a Game struct reaches 418.
  `rx_adv`/`rx_next` here use the fact that a slice is exactly the page tail, so
  a page pointer wrapping past `$FF` *is* the slice boundary — no division by
  208 and no 16-bit arithmetic anywhere. Verified live on a 7-player table: the
  spotlight reads seat 6 at offset 352, in the second slice.
- **`fn_fire`'s timeout is about 4ms.** 256 `djnz` iterations only passes because
  the o2em model answers synchronously; an HTTPS GET through the ESP32 takes
  seconds. This one waits in frames.
- **The CLOSE cannot follow the READ.** `publish_slice()` fills the whole window
  from the latest reply and *zeroes whatever it does not cover*, so a CLOSE —
  whose reply is empty — wipes the data the READ just delivered. `api_call`
  closes the *previous* connection at the start of the next request instead,
  which also matches how every screen renders straight out of the window between
  polls. The Intellivision client closes immediately and gets away with it only
  because its cartridge writes just `rxlen` bytes into `FN_RX` and leaves the
  rest of the buffer alone. That is luck, not design.

## What the space bought

Everything in this list was cut from the 3K cart for want of room, and the file
and line each one cites is where the cut used to be documented.

- **The table view.** All eight seats, their last moves and whose turn it is.
  The wire has carried `PL_MOVE` since the beginning and no O2 screen had ever
  read it -- it was not even in the offsets table.
- **Colour on the cards.** Per-glyph colour is a field the VDC character record
  always had; `glyph_ptr` used to OR in a constant. It now reads V_COL, which
  cost an internal RAM byte the cart did not have -- V_MVSEL moved out to
  external RAM to make one, since it is the only piece of screen state never
  touched during a draw.
- **Card backs**, by mapping `?` to the character generator's solid block.
- **Two grid rules.** The grid had never been switched on, and its registers had
  never been written -- they came up as whatever the VDC powered on with, which
  was harmless only while it stayed disabled.
- **The betting round**, on the marker that `render.inc` used to blank because
  "the bank ran out".
- **The whole end-of-hand message**, twelve characters at a time, instead of the
  first twelve and a hold.

## Not here

No in-game menu: the O2 has no keypad, and the console RESET button is the way
out of a table. No lobby room appkey — there is no Odyssey 2 lobby to hand one
off. The end-of-hand message shows the head of `lastResult` and holds, rather
than scrolling all 81 bytes through 12 characters.
