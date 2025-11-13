This folder contains test inputs, outputs and helper programs used during development.

Files:
- test_input.bin - small sample input binary for testing
- dump_body.c / dump_body.exe - helper to dump BODY chunk
- print_form_header.c / print_form_header.exe - helper to inspect FORM header
- fonts8.fnt - a 1 bitplane font file with transposed rows/collumns. X=768 and y=8.
- fonts8.iff - the outcome of manual testing - see below
  
Manual testing: 
./bpl2iff.exe -x 768 -y 8 -n 1 -t -r -o fonts8.iff .\tests\fonts8.fnt
