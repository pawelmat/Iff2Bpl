# IFF to Amiga Bitplane and Chunky Converter

A command-line utility that converts ILBM (Interleaved Bitmap) IFF files to Amiga bitplane format and 8-bit chunky format.

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

## License

Copyright (c) 2025 Kane/Suspect  
Licensed under the GNU GPLv3 License.

