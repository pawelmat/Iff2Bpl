/*
    BPL to IFF (ILBM) converter

    Converts raw Amiga planar bitmap data to an ILBM IFF file (FORM/ILBM) and writes
    the BMHD, CMAP and BODY chunks. The program only uses standard C libraries and
    is intended to be built with a standard C compiler.

    Supported command line parameters:
        -x <xsize>    Horizontal size of the picture in pixels (required)
        -y <ysize>    Vertical size of the picture in pixels (required)
        -n <bplnum>   Number of bitplanes (required)
        -i            Input bitplane rows are interleaved in memory (row0_plane0,row0_plane1,...). If omitted the input is expected to be non-interleaved (all rows of plane0, then plane1, ...)
        -t <colwidth> Input data is stored in byte-columns of specified width and must be transposed before conversion. Each column contains <colwidth> bytes per row; transpose reorders bytes to rows.
        -r            Compress the BODY chunk using PackBits (RLE). When omitted the BODY is written uncompressed.
        -o <output>   Base name for the output file (the program will append ".iff" if missing) (required)
        <input_file>  Path to the raw input file containing planar data

    Notes:
        - CMAP: the generated palette contains 2^n entries (where n is the number of bitplanes).
                        If a palette is found at the end of the raw file, then it is used.
                        Otherwise the first entry is set to RGB 00,00,00 and the remaining entries are set to FF,FF,FF.

    Examples:
        bpl2iff -x 320 -y 256 -n 5 -o image.raw.iff input.bpl
        bpl2iff -x 16 -y 4 -n 1 -t 1 -o test.iff tests/test_input.bin
        bpl2iff -x 320 -y 200 -n 4 -r -o compressed.iff input.bpl

    Compile:
        gcc bpl2iff.c -o bpl2iff.exe

    Copyright (c) 2025 Kane/Suspect, provided under the GNU GPLv3 License.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#pragma pack(push,1)
typedef struct {
    uint16_t width;
    uint16_t height;
    uint16_t x;
    uint16_t y;
    uint8_t numPlanes;
    uint8_t masking;
    uint8_t compression;
    uint8_t pad1;
    uint16_t transparentColor;
    uint8_t xAspect;
    uint8_t yAspect;
    uint16_t pageWidth;
    uint16_t pageHeight;
} BMHD;
#pragma pack(pop)

// write 32-bit BE
void write_be32(FILE* f, uint32_t v) {
    uint8_t b[4];
    b[0] = (v >> 24) & 0xFF;
    b[1] = (v >> 16) & 0xFF;
    b[2] = (v >> 8) & 0xFF;
    b[3] = v & 0xFF;
    fwrite(b,1,4,f);
}

// write 16-bit BE
void write_be16(FILE* f, uint16_t v) {
    uint8_t b[2];
    b[0] = (v >> 8) & 0xFF;
    b[1] = v & 0xFF;
    fwrite(b,1,2,f);
}

void usage(const char* prog) {
    fprintf(stderr, "Usage: %s -x <xsize> -y <ysize> -n <bplnum> [-i] [-t <colwidth>] [-r] -o <output_name> <input_file>\n", prog);
}

// PackBits (ILBM RLE) encoder: compress src_len bytes into dynamically allocated buffer, returns size and sets out_len
uint8_t* packbits_encode(const uint8_t* src, size_t src_len, size_t* out_len) {
    // Worst case size is src_len + src_len/128 + 1, allocate conservatively
    size_t max_out = src_len + (src_len / 128) + 16;
    uint8_t* out = (uint8_t*)malloc(max_out);
    if (!out) return NULL;
    size_t si = 0, di = 0;
    while (si < src_len) {
        // find run of repeated bytes
        size_t run_len = 1;
        while (si + run_len < src_len && src[si] == src[si + run_len] && run_len < 128) run_len++;
        if (run_len >= 3) {
            // emit any pending literals before the run
            // but in this simple loop just emit run directly
            out[di++] = (uint8_t)(1 - run_len); // n = -(run_len-1) in signed byte -> 1-run_len as unsigned
            out[di++] = src[si];
            si += run_len;
        } else {
            // emit literal sequence up to 128 bytes or until a run of 3+ starts
            size_t lit_start = si;
            size_t lit_len = 0;
            while (si < src_len && lit_len < 128) {
                // peek ahead to see if a run of 3 starts
                if (si + 2 < src_len && src[si] == src[si+1] && src[si] == src[si+2]) break;
                si++; lit_len++;
            }
            out[di++] = (uint8_t)(lit_len - 1); // n = lit_len-1
            memcpy(out + di, src + lit_start, lit_len);
            di += lit_len;
        }
    }
    *out_len = di;
    // shrink buffer
    uint8_t* shr = (uint8_t*)realloc(out, di);
    if (shr) out = shr;
    return out;
}

// decode PackBits into dst, returns number of bytes written (or 0 on error)
size_t packbits_decode(const uint8_t* src, size_t src_len, uint8_t* dst, size_t dst_len) {
    size_t si = 0, di = 0;
    while (si < src_len && di < dst_len) {
        int8_t n = (int8_t)src[si++];
        if (n >= 0) {
            int count = n + 1;
            if (si + count > src_len) return 0;
            if (di + count > dst_len) return 0;
            memcpy(dst + di, src + si, count);
            si += count; di += count;
        } else if (n != -128) {
            int count = (-n) + 1;
            if (si >= src_len) return 0;
            if (di + count > dst_len) return 0;
            uint8_t v = src[si++];
            memset(dst + di, v, count);
            di += count;
        }
        // n == -128 -> NOP
    }
    return di;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        usage(argv[0]);
        return 1;
    }

    int xsize = -1, ysize = -1, bplnum = -1;
    int interleaved = 0;
    int transpose_cols = 0;
    int transpose_col_width = 0;
    int use_rle = 0;
    const char* outname = NULL;
    const char* infile = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-x") == 0 && i+1 < argc) {
            xsize = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-y") == 0 && i+1 < argc) {
            ysize = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-n") == 0 && i+1 < argc) {
            bplnum = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-i") == 0) {
            interleaved = 1;
        } else if (strcmp(argv[i], "-t") == 0 && i+1 < argc) {
            transpose_col_width = atoi(argv[++i]);
            if (transpose_col_width > 0) transpose_cols = 1;
        } else if (strcmp(argv[i], "-r") == 0) {
            use_rle = 1;
        } else if (strcmp(argv[i], "-o") == 0 && i+1 < argc) {
            outname = argv[++i];
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        } else {
            infile = argv[i];
        }
    }

    if (xsize <= 0 || ysize <= 0 || bplnum <= 0 || outname == NULL || infile == NULL) {
        fprintf(stderr, "Some mandatory parameters missing\n");
        usage(argv[0]);
        return 1;
    }

    // Ensure output name ends with .iff
    char outfilename[1024];
    strncpy(outfilename, outname, sizeof(outfilename)-1);
    outfilename[sizeof(outfilename)-1] = '\0';
    size_t olen = strlen(outfilename);
    if (olen < 4 || strcmp(outfilename+olen-4, ".iff") != 0) {
        strncat(outfilename, ".iff", sizeof(outfilename)-olen-1);
    }

    // compute expected input size
    size_t row_bytes = ((xsize + 15) / 16) * 2; // bytes per row per plane (word-aligned as ILBM expects)
    size_t bytes_per_row_min = (xsize + 7) / 8; // minimal bytes per row (no word padding)
    size_t columns = 0; // number of byte-columns per row (transpose works on these bytes)
    size_t plane_input_size;
    if (transpose_cols) {
        // input stores columns: each column is transpose_col_width bytes wide, and each has ysize rows
        // compute number of columns needed to cover bytes_per_row_min
        columns = (bytes_per_row_min + transpose_col_width - 1) / transpose_col_width;
        plane_input_size = columns * (size_t)transpose_col_width * (size_t)ysize;
    } else {
        // expect input rows padded to Amiga word boundary
        plane_input_size = row_bytes * ysize;
    }
    size_t plane_size = row_bytes * ysize; // final plane size after padding rows to word boundary
    size_t expected_size = plane_input_size * bplnum;
    
    // Check if file might contain a color map at the end
    uint32_t num_colors = 1u << bplnum;
    if (num_colors > 256) num_colors = 256;
    size_t palette_size = num_colors * 2; // 2 bytes per color in Amiga format
    size_t expected_size_with_palette = expected_size + palette_size;
    int has_custom_palette = 0;
    uint16_t* custom_palette = NULL;

    FILE* inf = fopen(infile, "rb");
    if (!inf) {
        fprintf(stderr, "Failed to open input file: %s\n", infile);
        return 1;
    }
    // get size
    fseek(inf, 0, SEEK_END);
    long fsize = ftell(inf);
    fseek(inf, 0, SEEK_SET);

    // Check if file contains custom palette
    if (fsize == (long)expected_size_with_palette) {
        has_custom_palette = 1;
    } else if (fsize == (long)expected_size) {
        has_custom_palette = 0;
    } else {
        fprintf(stderr, "Input file size mismatch: expected %zu bytes (or %zu with palette), got %ld\n", 
                expected_size, expected_size_with_palette, fsize);
        fclose(inf);
        return 1;
    }

    uint8_t* data = (uint8_t*)malloc(expected_size);
    if (!data) {
        fprintf(stderr, "Out of memory\n");
        fclose(inf);
        return 1;
    }

    if (fread(data,1,expected_size,inf) != expected_size) {
        fprintf(stderr, "Failed to read input file\n");
        free(data);
        fclose(inf);
        return 1;
    }
    
    // Read custom palette if present
    if (has_custom_palette) {
        custom_palette = (uint16_t*)malloc(palette_size);
        if (!custom_palette) {
            fprintf(stderr, "Out of memory (palette)\n");
            free(data);
            fclose(inf);
            return 1;
        }
        // Read palette as bytes and convert to big-endian uint16_t
        uint8_t* palette_bytes = (uint8_t*)custom_palette;
        if (fread(palette_bytes, 1, palette_size, inf) != palette_size) {
            fprintf(stderr, "Failed to read palette data\n");
            free(custom_palette);
            free(data);
            fclose(inf);
            return 1;
        }
        // Convert from big-endian bytes to host uint16_t
        for (uint32_t i = 0; i < num_colors; i++) {
            custom_palette[i] = ((uint16_t)palette_bytes[i*2] << 8) | palette_bytes[i*2 + 1];
        }
        printf("Found palette with %u colours at index %zu in the file.\n", num_colors, expected_size);
        // printf("Palette: ");
        // for (uint32_t i = 0; i < num_colors; i++) {
        //     printf("%04X", custom_palette[i]);
        //     if (i < num_colors - 1) printf(" ");
        // }
        // printf("\n");
    }
    fclose(inf);

    // Prepare output file
    FILE* out = fopen(outfilename, "wb");
    if (!out) {
        fprintf(stderr, "Failed to open output file: %s\n", outfilename);
        free(data);
        return 1;
    }

    // Write FORM header with placeholder size, then patch it later
    fwrite("FORM",1,4,out);
    write_be32(out, 0); // placeholder for FORM size
    fwrite("ILBM",1,4,out);

    // Prepare BMHD chunk
    // BMHD chunk is 20 bytes
    fwrite("BMHD",1,4,out);
    write_be32(out, 20);
    BMHD bmhd;
    memset(&bmhd,0,sizeof(bmhd));
    bmhd.width = xsize;
    bmhd.height = ysize;
    bmhd.x = 0;
    bmhd.y = 0;
    bmhd.numPlanes = (uint8_t)bplnum;
    bmhd.masking = 0; // none
    bmhd.compression = 0; // no compression
    bmhd.pad1 = 0;
    bmhd.transparentColor = 0;
    bmhd.xAspect = 1;
    bmhd.yAspect = 1;
    bmhd.pageWidth = xsize;
    bmhd.pageHeight = ysize;

    // BMHD fields must be big-endian when written. Write each field explicitly.
    write_be16(out, bmhd.width);
    write_be16(out, bmhd.height);
    write_be16(out, bmhd.x);
    write_be16(out, bmhd.y);
    fwrite(&bmhd.numPlanes,1,1,out);
    fwrite(&bmhd.masking,1,1,out);
    fwrite(&bmhd.compression,1,1,out);
    fwrite(&bmhd.pad1,1,1,out);
    write_be16(out, bmhd.transparentColor);
    fwrite(&bmhd.xAspect,1,1,out);
    fwrite(&bmhd.yAspect,1,1,out);
    write_be16(out, bmhd.pageWidth);
    write_be16(out, bmhd.pageHeight);

    // CMAP chunk
    size_t cmap_size = num_colors * 3;
    fwrite("CMAP",1,4,out);
    write_be32(out, (uint32_t)cmap_size);
    
    if (has_custom_palette && custom_palette) {
        // Use custom palette from file
        // Format is 0RGB: first byte = 0000RRRR, second byte = GGGGBBBB
        int palette_warning_shown = 0;
        for (uint32_t i = 0; i < num_colors; i++) {
            uint16_t color = custom_palette[i];
            // Check if the leading 4 bits are zero (valid 0RGB format)
            if ((color & 0xF000) != 0 && !palette_warning_shown) {
                fprintf(stderr, "Warning: Color %u has non-zero leading bits (0x%04X). Palette format might be incorrect.\n", i, color);
                palette_warning_shown = 1;
            }
            // Extract 4-bit components and expand to 8-bit
            uint8_t r = ((color >> 8) & 0x0F) * 17; // 0x0F -> 0xFF (multiply by 17)
            uint8_t g = ((color >> 4) & 0x0F) * 17;
            uint8_t b = (color & 0x0F) * 17;
            uint8_t rgb[3] = {r, g, b};
            fwrite(rgb, 1, 3, out);
        }
    } else {
        // Default palette: first color black, others white
        for (uint32_t i = 0; i < num_colors; i++) {
            if (i == 0) {
                uint8_t rgb[3] = {0x00,0x00,0x00};
                fwrite(rgb,1,3,out);
            } else {
                uint8_t rgb[3] = {0xFF,0xFF,0xFF};
                fwrite(rgb,1,3,out);
            }
        }
    }
    // pad CMAP to even size
    if ((cmap_size & 1) != 0) {
        uint8_t zero = 0;
        fwrite(&zero,1,1,out);
    }

    // Write BODY later after preparing plane buffers to optionally compress with RLE.

    // Must produce interleaved data in BODY regardless of input layout.
    // Normalize input into per-plane buffers where each row is padded to 'row_bytes'.
    uint8_t* plane_buffers = (uint8_t*)malloc((size_t)bplnum * plane_size);
    if (!plane_buffers) {
        fprintf(stderr, "Out of memory (plane buffers)\n");
        free(data);
        fclose(out);
        return 1;
    }

    if (transpose_cols) {
        // Input layout per plane: column-major bytes: for c=0..columns-1, for y=0..ysize-1, for b=0..transpose_col_width-1 -> byte
        // Transpose bytes: dst_row[y][c*transpose_col_width + b] = src_plane[(c*ysize + y)*transpose_col_width + b]
        for (int p = 0; p < bplnum; p++) {
            uint8_t* dst_plane = plane_buffers + (size_t)p * plane_size;
            uint8_t* src_plane = data + (size_t)p * plane_input_size;
            // Zero destination plane
            memset(dst_plane, 0, plane_size);
            for (size_t c = 0; c < columns; c++) {
                for (int y = 0; y < ysize; y++) {
                    for (int b = 0; b < transpose_col_width; b++) {
                        size_t sidx = (c * (size_t)ysize + (size_t)y) * (size_t)transpose_col_width + (size_t)b;
                        if (sidx >= plane_input_size) continue;
                        uint8_t val = src_plane[sidx];
                        size_t didx = (size_t)y * row_bytes + c * (size_t)transpose_col_width + (size_t)b;
                        if (didx < plane_size) dst_plane[didx] = val;
                    }
                }
            }
            // rows are padded automatically since dst_plane was zeroed
        }
    } else {
        // No transpose requested. Input layout may be interleaved or per-plane sequential (non-interleaved).
        if (interleaved) {
            // Input is interleaved rows per plane already but may have minimal bytes per row or padded rows.
            // Read row-by-row from input and write into plane_buffers, expanding to padded row_bytes if necessary.
            size_t in_row_bytes = bytes_per_row_min;
            size_t src_offset = 0;
            for (int y = 0; y < ysize; y++) {
                for (int p = 0; p < bplnum; p++) {
                    uint8_t* dst = plane_buffers + (size_t)p * plane_size + (size_t)y * row_bytes;
                    uint8_t* src = data + src_offset;
                    memcpy(dst, src, in_row_bytes);
                    // zero pad remainder of the row if row_bytes > in_row_bytes
                    if (row_bytes > in_row_bytes) memset(dst + in_row_bytes, 0, row_bytes - in_row_bytes);
                    src_offset += in_row_bytes;
                }
            }
        } else {
            // Input is non-interleaved: plane0 rows..., plane1 rows...
            for (int p = 0; p < bplnum; p++) {
                uint8_t* src_plane = data + (size_t)p * plane_input_size;
                uint8_t* dst_plane = plane_buffers + (size_t)p * plane_size;
                // For each row, copy minimal bytes and pad to row_bytes
                for (int y = 0; y < ysize; y++) {
                    size_t sidx = (size_t)y * bytes_per_row_min;
                    size_t didx = (size_t)y * row_bytes;
                    memcpy(dst_plane + didx, src_plane + sidx, bytes_per_row_min);
                    if (row_bytes > bytes_per_row_min) memset(dst_plane + didx + bytes_per_row_min, 0, row_bytes - bytes_per_row_min);
                }
            }
        }
    }

    // Assemble interleaved BODY into a contiguous buffer
    size_t body_uncomp_size = row_bytes * (size_t)ysize * (size_t)bplnum;
    uint8_t* body_uncomp = (uint8_t*)malloc(body_uncomp_size);
    if (!body_uncomp) {
        fprintf(stderr, "Out of memory (body buffer)\n");
        free(plane_buffers);
        free(data);
        fclose(out);
        return 1;
    }
    size_t bo = 0;
    for (int y = 0; y < ysize; y++) {
        for (int p = 0; p < bplnum; p++) {
            uint8_t* src = plane_buffers + (size_t)p * plane_size + (size_t)y * row_bytes;
            memcpy(body_uncomp + bo, src, row_bytes);
            bo += row_bytes;
        }
    }

    // Write BODY chunk (optionally RLE compressed)
    if (use_rle) bmhd.compression = 1; else bmhd.compression = 0;

    // encode if requested
    uint8_t* body_to_write = body_uncomp;
    size_t body_to_write_size = body_uncomp_size;
    uint8_t* packed = NULL;
    size_t packed_size = 0;
    if (use_rle) {
        // Compress each scanline (for each row y and plane p) separately and concatenate.
        size_t est_cap = body_uncomp_size + (body_uncomp_size / 128) + 16;
        packed = (uint8_t*)malloc(est_cap);
        if (!packed) {
            fprintf(stderr, "Out of memory (packed buffer)\n");
            free(body_uncomp);
            free(plane_buffers);
            free(data);
            fclose(out);
            return 1;
        }
        packed_size = 0;
        for (int y = 0; y < ysize; y++) {
            for (int p = 0; p < bplnum; p++) {
                uint8_t* seg = body_uncomp + ((size_t)y * (size_t)bplnum + (size_t)p) * row_bytes;
                size_t enc_len = 0;
                uint8_t* enc = packbits_encode(seg, row_bytes, &enc_len);
                if (!enc) {
                    fprintf(stderr, "RLE compression failed (row %d plane %d)\n", y, p);
                    free(packed);
                    free(body_uncomp);
                    free(plane_buffers);
                    free(data);
                    fclose(out);
                    return 1;
                }
                // ensure capacity
                if (packed_size + enc_len > est_cap) {
                    est_cap = (packed_size + enc_len) * 2;
                    uint8_t* tmp = (uint8_t*)realloc(packed, est_cap);
                    if (!tmp) { free(enc); free(packed); free(body_uncomp); free(plane_buffers); free(data); fclose(out); return 1; }
                    packed = tmp;
                }
                memcpy(packed + packed_size, enc, enc_len);
                packed_size += enc_len;
                free(enc);
            }
        }
        // shrink packed
        uint8_t* shr = (uint8_t*)realloc(packed, packed_size);
        if (shr) packed = shr;
        body_to_write = packed;
        body_to_write_size = packed_size;
    }

    // Write BODY chunk header and data
    fwrite("BODY",1,4,out);
    write_be32(out, (uint32_t)body_to_write_size);
    fwrite(body_to_write,1,body_to_write_size,out);
    // pad BODY chunk to even size
    if (body_to_write_size & 1) {
        uint8_t zero = 0;
        fwrite(&zero,1,1,out);
        body_to_write_size++;
    }

    // cleanup
    if (packed) free(packed);
    free(body_uncomp);
    free(plane_buffers);

    // Record end of file now, before moving file pointer to update BMHD
    long endpos = ftell(out);
    uint32_t form_size = (uint32_t)(endpos - 8);

    // Update BMHD.compression field in file: compression is at offset 30 (start of file + 30)
    fseek(out, 30, SEEK_SET);
    uint8_t comp_byte = (use_rle ? 1 : 0);
    fwrite(&comp_byte,1,1,out);

    // Patch FORM size using the recorded endpos
    fseek(out, 4, SEEK_SET);
    write_be32(out, form_size);
    fclose(out);
    if (custom_palette) free(custom_palette);
    free(data);

    printf("Wrote ILBM file: %s (size %u bytes)\n", outfilename, form_size + 8);
    return 0;
}
