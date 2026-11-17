# Build tooling

Each make target runs one script in scripts/. The scripts use three packages here: relink links
and packs, objmatch reads relocations and finds compilers, and asset_analysis reads ISO 9660.
Everything they write goes under .tools/, game/ and build/, which git ignores.

## Setup

scripts/setup-ps2-toolchain.sh installs the pinned toolchain into .tools/ps2. It checks each
download against the hashes in config/versions/toolchain.env.

- ee-gcc 2.9-ee-991111-01 comes from the Fatal Frame decompilation, at a fixed commit.
- GNU ld and as come from decompals' PS2 binutils.
- A few functions need other EE GCC 2.9x builds, from decomp.me. config/compilers.json lists
  them.

All are GPL builds. Setup fetches no Sony SDK headers or libraries.

## Extract

scripts/extract.py takes SLUS_203.44 and REZ.CVM from a disc image (bin/cue, raw bin or ISO), or
as plain files. It then copies the 29 overlays out of REZ.CVM, which is an ISO 9660 volume behind
a 0x1800-byte CRI header. It refuses any file whose sha256 differs from config/.

## Build

scripts/relink.py compiles each matching function with the compiler and flags from its record in
config/inventory. ld then links the main executable and each overlay into
build/relink/ps2-us-1.01/exact.

## Verify

scripts/verify_build.py checks the 30 linked files against the sha256 in
config/versions/builds.json and config/overlays/ps2-us-1.01.json. It fails unless all 30 match.

## Disc

scripts/build_disc.py writes the linked overlays into a copy of REZ.CVM. It then writes REZ.CVM
and the executable into a copy of your raw disc image (MODE2/2352, one data track). A file that
still fits keeps its sectors. A larger file moves to the end of the volume, and the script
updates its directory record, the volume size and the CVM header. Each sector it writes gets a
new EDC and Mode 2 Form 1 ECC. It never changes your original files.

## How the relink works

The linker places each function in its own section, in address order. Matching functions come
from source, compiled with -ffunction-sections. The relink extracts everything else from your
executable as assembly: non-matching functions, padding and the data sections. It writes each
address in that assembly as a relocation, so the address moves with its target.

In code, relink/mips.py follows each lui through the control flow to the instructions that use
it. It handles delay slots, register copies, stack spills and jump tables, and writes HI16/LO16,
R_MIPS_26 or GPREL16 relocations. In data, it treats a word as an address only when:

- a compiled unit's relocations say so,
- the word points to the start of a string, or
- the word sits in a jump table.

Integers and fixed-point values share the address range, so a value in range proves nothing.
Unproved words stay as plain numbers, and the report lists them.

The relink does not link a compiled unit's data sections. It places each one as a NOLOAD shadow
over the extracted data, at the address the unit's relocations prove. The unit's references then
land on the original data. Compiled functions that don't match shadow their extracted copies the
same way.

ld writes its own ELF headers, so relink/pack.py uses the original executable as a template. It
swaps in the linked image and recomputes every address, size and offset in the headers, the .DVP
VU overlay tables included.

An overlay is an image with no headers. The game loads it at a fixed address in main's .bss and
enters it through its first word. The relink links each overlay the same way, against the linked
main executable through ld's --just-symbols.

relink/verify.py then checks every reference. Each chunk and data label is a symbol named after
its original address, so the linked symbol table maps old addresses to new ones. Each reference's
new value must equal the mapped old value. Without padding, the map changes nothing. Pass --pad
ADDR:N to insert zero bytes and check that references follow their targets.
