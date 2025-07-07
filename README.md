# Iff2Bpl
 Amiga Iff to Bitplane converter

# Description
Converts ILBM (IFF) files to raw (interleaved) bitplane format for use in Amiga applications. 
It assumes the input file is a valid ILBM file and does not perform extensive error checking. It handles both RLE compressed and uncompressed ILBM formats.  

It writes the Amiga bitplane raw data to a .bpl file, and the palette to a .pal file. The names of these files are the same as the input file, just with different extensions.  
- The .bpl file contains interleaved bitplane data (i.e. line 1 bitplane 1, line 1 bitplane 2, ...line 1 bitplane N, line 2 bitplane 1...).
- The .pal file contains all colours in order in 0x0RGB format (each colour is saved as 2 bytes). The original colours from the CMAP hunk, which are 8-bit values per coponent, are scaled down to 4-bit values used by the Amiga.
  
# Usage
Usage: iff2bpl <input.iff>

Example: iff2bpl myimage.iff  
Output: myimage.bpl (bitplane data) and myimage.pal (palette data)

# Build
Compiles with: gcc iff2bpl.c -o iff2bpl.exe.

You can also use Visual Studio Code with the included configuration files to build this project if you have a C compiler installed with VSC.  
There are no external dependencies beyond standard C libraries.

# Copyright
Copyright (c) 2025 Kane/Suspect, provided under the GNU GPLv3 License.


