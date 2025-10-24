# IFF <> Amiga Bitplane (and Chunky) Converter

This project consists of 2 command line utilities:  
- **iff2bpl** Converts ILBM (Interleaved Bitmap) IFF files to Amiga bitplane format and 8-bit chunky format.
- **bpl2iff** Converts Amiga raw bitplane format to ILBM (Interleaved Bitmap) IFF files (note: currently palette has all colours other than 0 fixed to $FFF)

# iff2bpl

## Overview

This tool converts IFF image files into formats suitable for Amiga applications:
- **Raw bitplane data (.bpl)** - Interleaved format for Amiga hardware (default)
- **Palette data (.pal)** - 16-bit words in Amiga colour register format  
- **Chunky pixel data (.chk)** - 8-bit per pixel chunky format (optional)
- **Non-interleaved planar data (.bpf)** - All rows of plane 0, then plane 1, etc. (optional)

## Features

- Converts ILBM (IFF) files to Amiga-compatible formats
- Supports both compressed (RLE/PackBits) and uncompressed IFF files
- Generates Amiga hardware-ready bitplane data
- Converts 8-bit RGB palette to 4-bit Amiga colour format
- Optional chunky format output for software-based pixel manipulation
- Optional non-interleaved planar format for specific development needs
- Custom output filename support
- Minimal dependencies - compiles with standard C libraries

## Usage

```bash
iff2bpl [-o output_name] [-c] [-cd] [-ni] <input.iff>
```

### Options

- `-o output_name` - Specify custom base name for output files
- `-c` - Also create chunky format output (.chk file)
- `-cd` - Also create chunky format with bit doubling (.chk file)
- `-ni` - Also create non-interleaved planar format (.bpf file)
- `<input.iff>` - Input IFF/ILBM file to convert

### Examples

```bash
# Basic conversion - creates image.bpl and image.pal
iff2bpl image.iff

# Custom output name - creates sprite.bpl and sprite.pal
iff2bpl -o sprite image.iff

# Include chunky format - creates image.bpl, image.pal and image.chk
iff2bpl -c image.iff

# Include non-interleaved format - creates image.bpl, image.pal and image.bpf
iff2bpl -ni image.iff

# Combined options - creates sprite.bpl, sprite.pal, sprite.chk and sprite.bpf
iff2bpl -c -ni -o sprite image.iff

# Include chunky format with bit doubling - creates image.bpl, image.pal and image.chk
iff2bpl -cd image.iff
```

## Output Files

### .bpl file (Bitplane Data)
Raw bitplane data in interleaved format ready for Amiga hardware. Each bitplane contains one bit per pixel, organized in the format expected by Amiga's custom chips.

### .pal file (Palette Data)
Palette entries converted from 8-bit RGB to 4-bit Amiga format, stored as 16-bit words compatible with Amiga colour registers.

### .chk file (Chunky Data)
Optional 8-bit per pixel format where each byte represents a complete pixel value (palette index). Useful for software-based pixel manipulation or conversion to other formats.

### .bpf file (Non-interleaved Planar Data)
Optional planar format where all rows of each bitplane are grouped together (plane 0 data, then plane 1 data, etc.) rather than interleaved by scanline. Useful for certain development workflows or tools that expect this data organization.

## Bit Doubling (-cd option)

The `-cd` option creates chunky data where each bit of the 4 least significant bits is expanded to 2 bits by replication. This effectively converts 4-bit color values to 8-bit values. This is useful with a particular class of C2P routines (up to 16 colours) which require such input.

Examples:
- `00000001` becomes `00000011`
- `00000010` becomes `00001100`
- `00001101` becomes `11110011`

## Technical Details

### Supported Formats
- Input: ILBM (IFF) files with BMHD, CMAP, and BODY chunks
- Compression: Uncompressed and RLE (PackBits) compressed data
- Color depth: Supports multiple bitplanes (typically 1-8 planes)

### Data Layout
- **Bitplanes (interleaved)**: Row-by-row mixing of planes - Row0-Plane0, Row0-Plane1, Row1-Plane0, etc.
- **Bitplanes (non-interleaved)**: All rows of each plane together - All Plane0 rows, then all Plane1 rows, etc.
- **Palette**: 4-bit RGB values packed into 16-bit words (Amiga colour register 0RGB format)
- **Chunky**: Linear pixel array, one byte per pixel

## Building

### With GCC
```bash
gcc iff2bpl.c -o iff2bpl.exe
```

### With Visual Studio Code
Use the included VS Code configuration files for building and debugging.

### Requirements
- Standard C compiler (GCC, Clang, MSVC)
- Standard C libraries (stdio, stdlib, stdint, string)

# bpl2iff

## Overview

`bpl2iff` converts raw Amiga planar bitplane data into an ILBM IFF file (FORM/ILBM) containing the `BMHD`, `CMAP` and `BODY` chunks. It supports interleaved and non-interleaved input, an optional byte-column transpose mode (`-t`) and optional PackBits (RLE) compression of the BODY chunk (`-r`).

The CMAP chunk created by `bpl2iff` will contain 2^n entries where `n` is the number of bitplanes. The first palette entry is set to black (`00,00,00`) and all other entries are set to white (`FF,FF,FF`).

## Usage

```
bpl2iff -x <xsize> -y <ysize> -n <bplnum> [-i] [-t] [-r] -o <output_name> <input_file>
```

Parameters:
- `-x <xsize>` : horizontal size in pixels (required)
- `-y <ysize>` : vertical size in pixels (required)
- `-n <bplnum>`: number of bitplanes (required)
- `-i`         : input is interleaved rows per plane (optional)
- `-t`         : input is stored in byte columns and must be transposed first (optional)
- `-r`         : compress BODY with PackBits (RLE) (optional)
- `-o <output_name>`: base name for output file (will have `.iff` appended if missing) (required)
- `<input_file>`: path to raw input file (required)

## Build

```bash
gcc bpl2iff.c -o bpl2iff.exe
```

## Example / Test

There are small test inputs and helpers in the `tests/` folder. See `tests/README.md` for details. A quick example using the provided test input:

```bash
./bpl2iff.exe -x 768 -y 8 -n 1 -t -r -o fonts.iff .\tests\FONTS01.FNT
```

This will create `fonts.iff` which should be a 1-bitplane, RLE compressed font file.

## License

Copyright (c) 2025 Kane/Suspect  
Licensed under the GNU GPLv3 License.

