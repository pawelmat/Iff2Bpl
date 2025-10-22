/*
    IFF to BPL converter

    Converts ILBM (IFF) files to raw (interleaved) bitplane format for use in Amiga applications. 
    It assumes the input file is a valid ILBM file and does not perform extensive error checking.
    Writes the BPL data to a .bpl file, and the palette to a .pal file.
    Optionally creates a chunky format file (.chk) for software-based pixel manipulation.
    Optionally creates a non-interleaved planar format file (.bpf) for specific development needs.
    The output files will be named based on the input file name, with .bpl, .pal, .chk, and .bpf extensions.

    Usage: iff2bpl [-o output_name] [-c] [-cd] [-ni] <input.iff>
    Options:
      -o output_name  Specify custom base name for output files
      -c              Also create chunky format output (.chk file)
      -cd             Also create chunky format with bit doubling (.chk file)
      -ni             Also create non-interleaved planar format (.bpf file)

    Examples: 
      iff2bpl myimage.iff                Creates myimage.bpl and myimage.pal
      iff2bpl -o sprite myimage.iff      Creates sprite.bpl and sprite.pal
      iff2bpl -c myimage.iff             Creates myimage.bpl, myimage.pal and myimage.chk
      iff2bpl -cd myimage.iff            Creates myimage.bpl, myimage.pal and myimage.chk (bit doubled)
      iff2bpl -ni myimage.iff            Creates myimage.bpl, myimage.pal and myimage.bpf
      iff2bpl -c -ni -o sprite myimage.iff Creates sprite.bpl, sprite.pal, sprite.chk and sprite.bpf

    Output: 
      .bpl file - Raw bitplane data (interleaved format for Amiga hardware - default)
      .pal file - Palette data (16-bit words in Amiga color register format)
      .chk file - Chunky pixel data (8-bit per pixel, optional with -c or -cd flags)
      .bpf file - Non-interleaved planar data (all rows of plane 0, then plane 1, etc., optional with -ni flag)

    The -cd option creates chunky data where each bit of the 4 least significant bits is doubled.
    For example: 00000001 becomes 00000011, 00000010 becomes 00001100, 00001101 becomes 11110011.

    Compiles with: gcc iff2bpl.c -o iff2bpl.exe
    You can also use VS Code with the included configuration files to build this project.

    Copyright (c) 2025 Kane/Suspect, provided under the GNU GPLv3 License. 
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>

#pragma pack(push, 1)
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

void print_hex(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        printf("%02X ", data[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    if (len % 16 != 0) printf("\n");
}

long get_file_size(FILE* f) {
    long cur = ftell(f);
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, cur, SEEK_SET);
    return size;
}

void write_bin(const char* filename, const uint8_t* data, size_t len) {
    FILE* f = fopen(filename, "wb");
    if (!f) {
        fprintf(stderr, "Failed to open %s for writing\n", filename);
        return;
    }
    fwrite(data, 1, len, f);
    fclose(f);
}

// Helper to read 4 bytes as big-endian uint32_t
uint32_t read_be32(FILE* f) {
    uint8_t b[4];
    if (fread(b, 1, 4, f) != 4) return 0;
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
}

// Helper to read 2 bytes as big-endian uint16_t
uint16_t read_be16(FILE* f) {
    uint8_t b[2];
    if (fread(b, 1, 2, f) != 2) return 0;
    return ((uint16_t)b[0] << 8) | b[1];
}

// Decompress ILBM RLE (PackBits) for a single scanline
size_t decompress_packbits(const uint8_t* src, size_t src_len, uint8_t* dst, size_t dst_len) {
    size_t si = 0, di = 0;
    while (si < src_len && di < dst_len) {
        int8_t n = (int8_t)src[si++];
        if (n >= 0) {
            // Copy next n+1 bytes literally
            int count = n + 1;
            if (si + count > src_len) count = src_len - si;
            if (di + count > dst_len) count = dst_len - di;
            memcpy(dst + di, src + si, count);
            si += count;
            di += count;
        } else if (n != -128) {
            // Repeat next byte (-n)+1 times
            int count = (-n) + 1;
            if (si >= src_len) break;
            uint8_t val = src[si++];
            if (di + count > dst_len) count = dst_len - di;
            memset(dst + di, val, count);
            di += count;
        }
        // else n == -128: NOP
    }
    return di;
}

void print_usage(const char* program_name) {
    printf("Usage: %s [-o output_name] [-c] [-cd] [-ni] <.iff file>\n", program_name);
    printf("  -o output_name  Specify custom base name for output files\n");
    printf("  -c              Also create chunky format output (.chk file)\n");
    printf("  -cd             Also create chunky format with doubled bits (.chk file)\n");
    printf("  -ni             Also create non-interleaved planar format (.bpf file)\n");
    printf("  <.iff file>     Input IFF/ILBM file to convert\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s image.iff              Creates image.bpl and image.pal\n", program_name);
    printf("  %s -o sprite image.iff    Creates sprite.bpl and sprite.pal\n", program_name);
    printf("  %s -c image.iff           Creates image.bpl, image.pal and image.chk\n", program_name);
    printf("  %s -cd image.iff          Creates image.bpl, image.pal and image.chk (doubled)\n", program_name);
    printf("  %s -ni image.iff          Creates image.bpl, image.pal and image.bpf\n", program_name);
    printf("  %s -c -ni -o sprite image.iff Creates sprite.bpl, sprite.pal, sprite.chk and sprite.bpf\n", program_name);
}

// Convert planar bitplane data to chunky format
void convert_to_chunky(const uint8_t* planar_data, uint8_t* chunky_data, 
                      uint16_t width, uint16_t height, uint8_t num_planes, int double_bits) {
    size_t row_bytes = ((width + 15) / 16) * 2; // bytes per row per plane
    
    for (uint16_t y = 0; y < height; y++) {
        for (uint16_t x = 0; x < width; x++) {
            uint8_t pixel_value = 0;
            
            // Extract bit from each plane to build pixel value
            for (uint8_t plane = 0; plane < num_planes; plane++) {
                size_t plane_offset = (y * num_planes + plane) * row_bytes;
                size_t byte_offset = x / 8;
                uint8_t bit_offset = 7 - (x % 8);
                
                if (plane_offset + byte_offset < (size_t)(height * num_planes * row_bytes)) {
                    uint8_t byte_val = planar_data[plane_offset + byte_offset];
                    uint8_t bit_val = (byte_val >> bit_offset) & 1;
                    pixel_value |= (bit_val << plane);
                }
            }
            
            if (double_bits) {
                // Double each bit of the 4 least significant bits
                uint8_t doubled_value = 0;
                for (int bit = 0; bit < 4; bit++) {
                    if (pixel_value & (1 << bit)) {
                        doubled_value |= (3 << (bit * 2)); // Set two consecutive bits
                    }
                }
                chunky_data[y * width + x] = doubled_value;
            } else {
                chunky_data[y * width + x] = pixel_value;
            }
        }
    }
}

// Convert interleaved planar data to non-interleaved planar format
void convert_to_noninterleaved(const uint8_t* interleaved_data, uint8_t* noninterleaved_data,
                              uint16_t width, uint16_t height, uint8_t num_planes) {
    size_t row_bytes = ((width + 15) / 16) * 2; // bytes per row per plane
    size_t plane_size = row_bytes * height; // bytes per complete plane
    
    for (uint8_t plane = 0; plane < num_planes; plane++) {
        for (uint16_t y = 0; y < height; y++) {
            // Source: interleaved format - plane data is mixed row by row
            size_t src_offset = (y * num_planes + plane) * row_bytes;
            // Destination: non-interleaved format - all rows of one plane together
            size_t dst_offset = (plane * plane_size) + (y * row_bytes);
            
            memcpy(noninterleaved_data + dst_offset, interleaved_data + src_offset, row_bytes);
        }
    }
}

int main(int argc, char* argv[]) {
    printf("IFF to Amiga BPL converter (c) Kane/Sct 2025\n");
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char* input_filename = NULL;
    const char* output_name = NULL;
    int create_chunky = 0;
    int create_chunky_doubled = 0;
    int create_noninterleaved = 0;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) {
                output_name = argv[++i];
            } else {
                fprintf(stderr, "Error: -o option requires an output filename\n");
                print_usage(argv[0]);
                return 1;
            }
        } else if (strcmp(argv[i], "-c") == 0) {
            create_chunky = 1;
        } else if (strcmp(argv[i], "-cd") == 0) {
            create_chunky_doubled = 1;
        } else if (strcmp(argv[i], "-ni") == 0) {
            create_noninterleaved = 1;
        } else {
            input_filename = argv[i];
        }
    }
    
    if (!input_filename) {
        fprintf(stderr, "Error: No input file specified\n");
        print_usage(argv[0]);
        return 1;
    }

    const char* filename = input_filename;
    FILE* f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Failed to open file: %s\n", filename);
        return 1;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    long filesize = ftell(f);
    fseek(f, 0, SEEK_SET);

    printf("Input file: %s\n", filename);
    printf("File size: %ld bytes\n", filesize);

    // Determine the base filename for output files
    char base_filename[512];
    if (output_name) {
        // Use the specified output name
        strncpy(base_filename, output_name, sizeof(base_filename) - 1);
        base_filename[sizeof(base_filename) - 1] = '\0';
    } else {
        // Strip extension from input filename if present
        strncpy(base_filename, filename, sizeof(base_filename) - 1);
        base_filename[sizeof(base_filename) - 1] = '\0';
        char* dot = strrchr(base_filename, '.');
        if (dot && dot != base_filename) {
            *dot = '\0';
        }
    }
    const char* output_base = base_filename;

    char chunk_id[5] = {0};
    uint32_t chunk_size;
    int found_bmhd = 0, found_cmap = 0, found_body = 0;
    BMHD bmhd;
    uint8_t* cmap_data = NULL;
    uint32_t cmap_size = 0;
    uint8_t* body_data = NULL;
    uint32_t body_size = 0;

    // Skip FORM header - assumes it's always present and in the same position at the start of the file
    fread(chunk_id, 1, 4, f); // "FORM"
    uint32_t form_size = read_be32(f); // FORM size (big-endian)
    fread(chunk_id, 1, 4, f); // "ILBM"

    while (ftell(f) < filesize) {
        if (fread(chunk_id, 1, 4, f) != 4) break;
        uint32_t chunk_size = read_be32(f); // chunk size (big-endian)
        uint32_t size = ((chunk_size + 1) & ~1); // even size
        //printf("Chunk ID: %.4s, Size: %u bytes\n", chunk_id, size);

        if (strncmp(chunk_id, "BMHD", 4) == 0) {
            // Read BMHD fields as big-endian
            bmhd.width = read_be16(f);
            bmhd.height = read_be16(f);
            bmhd.x = read_be16(f);
            bmhd.y = read_be16(f);
            fread(&bmhd.numPlanes, 1, 1, f);
            fread(&bmhd.masking, 1, 1, f);
            fread(&bmhd.compression, 1, 1, f);
            fread(&bmhd.pad1, 1, 1, f);
            bmhd.transparentColor = read_be16(f);
            fread(&bmhd.xAspect, 1, 1, f);
            fread(&bmhd.yAspect, 1, 1, f);
            bmhd.pageWidth = read_be16(f);
            bmhd.pageHeight = read_be16(f);
            found_bmhd = 1;
            fseek(f, size - sizeof(BMHD), SEEK_CUR);
        } else if (strncmp(chunk_id, "CMAP", 4) == 0) {
            cmap_data = (uint8_t*)malloc(size);
            cmap_size = size;
            fread(cmap_data, 1, size, f);
            found_cmap = 1;
        } else if (strncmp(chunk_id, "BODY", 4) == 0) {
            body_data = (uint8_t*)malloc(size);
            body_size = size;
            fread(body_data, 1, size, f);
            found_body = 1;
        } else {
            fseek(f, size, SEEK_CUR);
        }
    }

    if (found_bmhd) {
        printf("+BMHD:\n");
        printf("  width: %u (%u bytes)\n", bmhd.width, (bmhd.width/8));
        printf("  height: %u\n", bmhd.height);
        printf("  numPlanes: %u\n", bmhd.numPlanes);
        printf("  compression: %u\n", bmhd.compression);
    } else {
        printf("BMHD chunk not found.\n");
    }

    if (found_cmap) {
        char pal_filename[512];
        // Each palette entry is 3 bytes (R, G, B)
        size_t num_entries = cmap_size / 3;
        uint16_t* pal_words = (uint16_t*)malloc(num_entries * sizeof(uint16_t));
        if (!pal_words) {
            fprintf(stderr, "Failed to allocate memory for palette words\n");
        } else {
            for (size_t i = 0; i < num_entries; ++i) {
                // rescale the colours from 8 bits to 4 bits
                uint8_t r = (cmap_data[i * 3 + 0]*16/256);
                uint8_t g = (cmap_data[i * 3 + 1]*16/256);
                uint8_t b = (cmap_data[i * 3 + 2]*16/256);
                // Take lower 4 bits of each and place as described
                uint16_t word = 0;
                word |= ((r & 0x0F) << 8); // bits 12-9
                word |= ((g & 0x0F) << 4); // bits 8-5
                word |= ((b & 0x0F)); // bits 4-1
                word = (word >> 8) | (word << 8);
                pal_words[i] = word;
            }
            printf("+CMAP Pallette (%u colours):\n", num_entries);
            print_hex((uint8_t*)pal_words, num_entries * sizeof(uint16_t));

            snprintf(pal_filename, sizeof(pal_filename), "%s.pal", output_base);
            write_bin(pal_filename, (uint8_t*)pal_words, num_entries * sizeof(uint16_t));
            printf("Pallette written to: %s\n", pal_filename);
            free(pal_words);
        }
        free(cmap_data);
    } else {
        printf("CMAP chunk not found.\n");
    }

    if (found_body) {
        printf("+BODY (%u bytes):\n", body_size);
        char bpl_filename[512];
        snprintf(bpl_filename, sizeof(bpl_filename), "%s.bpl", output_base);

        uint8_t* final_planar_data = NULL;
        size_t final_planar_size = 0;

        if (bmhd.compression == 0) {
            // No compression, write as is
            write_bin(bpl_filename, body_data, body_size);
            printf("BODY (uncompressed), size %u bytes, written to: %s\n", body_size, bpl_filename);
            final_planar_data = body_data;
            final_planar_size = body_size;
        } else if (bmhd.compression == 1) {
            // RLE compression
            size_t row_bytes = ((bmhd.width + 15) / 16) * 2; // bytes per row per plane
            size_t total_rows = bmhd.height * bmhd.numPlanes;
            size_t out_size = row_bytes * bmhd.height * bmhd.numPlanes;
            uint8_t* out = (uint8_t*)malloc(out_size);
            if (!out) {
                fprintf(stderr, "Failed to allocate memory for decompressed BODY\n");
            } else {
                size_t src_offset = 0, dst_offset = 0;
                for (size_t row = 0; row < total_rows; ++row) {
                    size_t written = decompress_packbits(
                        body_data + src_offset,
                        body_size - src_offset,
                        out + dst_offset,
                        row_bytes
                    );
                    if (written != row_bytes) {
                        fprintf(stderr, "Warning: decompressed row %zu has %zu bytes, expected %zu\n", row, written, row_bytes);
                    }
                    // Find out how many bytes were consumed from src
                    // We have to scan the packbits stream for this row
                    size_t consumed = 0, produced = 0;
                    while (produced < row_bytes && (src_offset + consumed) < body_size) {
                        int8_t n = (int8_t)body_data[src_offset + consumed++];
                        if (n >= 0) {
                            int count = n + 1;
                            consumed += count;
                            produced += count;
                        } else if (n != -128) {
                            int count = (-n) + 1;
                            consumed += 1;
                            produced += count;
                        }
                        // else n == -128: NOP
                    }
                    src_offset += consumed;
                    dst_offset += row_bytes;
                }
                write_bin(bpl_filename, out, out_size);
                if (dst_offset != out_size) {
                    fprintf(stderr, "Warning: expected %zu bytes in output, got %zu\n", out_size, dst_offset);
                }
                if (out_size != (bmhd.height * row_bytes * bmhd.numPlanes)) {
                    fprintf(stderr, "Warning: output size does not match expected size (%zu != %zu)\n", out_size, bmhd.height * row_bytes * bmhd.numPlanes);
                }
                printf("BODY (decompressed), size %u bytes, written to: %s\n", out_size, bpl_filename);
                final_planar_data = out;
                final_planar_size = out_size;
            }
        } else {
            printf("Unknown compression type: %u\n", bmhd.compression);
        }

        // Create chunky format if requested
        if ((create_chunky || create_chunky_doubled) && final_planar_data && found_bmhd) {
            char chk_filename[512];
            snprintf(chk_filename, sizeof(chk_filename), "%s.chk", output_base);
            
            size_t chunky_size = bmhd.width * bmhd.height;
            uint8_t* chunky_data = (uint8_t*)malloc(chunky_size);
            if (!chunky_data) {
                fprintf(stderr, "Failed to allocate memory for chunky data\n");
            } else {
                convert_to_chunky(final_planar_data, chunky_data, bmhd.width, bmhd.height, bmhd.numPlanes, create_chunky_doubled);
                if (create_chunky_doubled) {
                    printf("Chunky format (doubled bits) written to: %s (%zu bytes)\n", chk_filename, chunky_size);
                } else {
                    printf("Chunky format written to: %s (%zu bytes)\n", chk_filename, chunky_size);
                }
                write_bin(chk_filename, chunky_data, chunky_size);
                free(chunky_data);
            }
        }

        // Create non-interleaved planar format if requested
        if (create_noninterleaved && final_planar_data && found_bmhd) {
            char bpf_filename[512];
            snprintf(bpf_filename, sizeof(bpf_filename), "%s.bpf", output_base);
            
            size_t noninterleaved_size = final_planar_size;
            uint8_t* noninterleaved_data = (uint8_t*)malloc(noninterleaved_size);
            if (!noninterleaved_data) {
                fprintf(stderr, "Failed to allocate memory for non-interleaved data\n");
            } else {
                convert_to_noninterleaved(final_planar_data, noninterleaved_data, bmhd.width, bmhd.height, bmhd.numPlanes);
                write_bin(bpf_filename, noninterleaved_data, noninterleaved_size);
                printf("Non-interleaved planar format written to: %s (%zu bytes)\n", bpf_filename, noninterleaved_size);
                free(noninterleaved_data);
            }
        }

        // Free decompressed data if it was allocated
        if (bmhd.compression == 1 && final_planar_data && final_planar_data != body_data) {
            free(final_planar_data);
        }
        free(body_data);
    } else {
        printf("BODY chunk not found.\n");
    }

    fclose(f);
    return 0;
}