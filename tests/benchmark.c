#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <windows.h>

#define ENABLE_LCD 1
#define ENABLE_SOUND 1

#include "../minigb_apu/minigb_apu.h"
#include "../peanut_gb/peanut_gb.h"

// 4x4 spatial dither patterns matching utility.c
static const uint8_t PGB_patterns_ref[4][4][4] = {
    { {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1}, {1, 1, 1, 1} },
    { {0, 1, 0, 1}, {1, 1, 1, 1}, {0, 1, 0, 1}, {1, 1, 1, 1} },
    { {1, 0, 1, 0}, {0, 1, 0, 1}, {1, 0, 1, 0}, {0, 1, 0, 1} },
    { {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0} }
};

static uint8_t PGB_bitmask_ref[4][4][4];

static void init_reference_bitmask(void)
{
    for(int palette = 0; palette < 4; palette++) {
        for(int y = 0; y < 4; y++) {
            int x_offset = 0;
            for(int i = 0; i < 4; i++) {
                int mask = 0x00;
                for(int x = 0; x < 2; x++) {
                    if(PGB_patterns_ref[palette][y][x_offset + x] == 1) {
                        int n = i * 2 + x;
                        mask |= (1 << (7 - n));
                    }
                }
                PGB_bitmask_ref[palette][i][y] = mask;
                x_offset += 2;
                if(x_offset == 4) x_offset = 0;
            }
        }
    }
}

// Reference dither function from original game_scene.c
static void reference_dither_frame(uint8_t *framebuffer, uint8_t (*front_fb)[160], uint8_t (*back_fb)[160], bool back_fb_enabled)
{
    int skip_counter = 0;
    bool single_line = false;
    int y2 = 0;
    int lcd_rows = 0;
    int row_offset = 52;
    int row_offset2 = 52 * 2;
    int y_offset;
    int next_y_offset = 2;

    for(int y = 0; y < (LCD_HEIGHT - 1); y++)
    {
        y_offset = next_y_offset;
        if(skip_counter == 5) {
            y_offset = 1;
            next_y_offset = 1;
            skip_counter = 0;
            single_line = true;
        } else if(single_line) {
            next_y_offset = 2;
            single_line = false;
        }

        uint8_t *pixels = back_fb_enabled ? front_fb[y] : back_fb[y];

        int d_row1 = y2 & 3;
        int d_row2 = (y2 + 1) & 3;
        int fb_index1 = lcd_rows;
        int fb_index2 = lcd_rows + row_offset;

        memset(&framebuffer[fb_index1], 0x00, 40);
        if(y_offset == 2) {
            memset(&framebuffer[fb_index2], 0x00, 40);
        }

        uint8_t bit = 0;
        for(int x = 0; x < LCD_WIDTH; x++)
        {
            uint8_t pixel = pixels[x] & 3;
            framebuffer[fb_index1] |= PGB_bitmask_ref[pixel][bit][d_row1];
            if(y_offset == 2) {
                framebuffer[fb_index2] |= PGB_bitmask_ref[pixel][bit][d_row2];
            }
            bit++;
            if(bit == 4) {
                bit = 0;
                fb_index1++;
                fb_index2++;
            }
        }

        y2 += y_offset;
        lcd_rows += (y_offset == 1) ? row_offset : row_offset2;
        if(!single_line) {
            skip_counter++;
        }
    }
}

static uint8_t PGB_dither_lut[4][256];

static void init_fast_dither_lut(void)
{
    for(int row = 0; row < 4; row++) {
        for(int idx = 0; idx < 256; idx++) {
            uint8_t p0 = idx & 3;
            uint8_t p1 = (idx >> 2) & 3;
            uint8_t p2 = (idx >> 4) & 3;
            uint8_t p3 = (idx >> 6) & 3;

            uint8_t b7 = PGB_patterns_ref[p0][row][0] << 7;
            uint8_t b6 = PGB_patterns_ref[p0][row][1] << 6;
            uint8_t b5 = PGB_patterns_ref[p1][row][2] << 5;
            uint8_t b4 = PGB_patterns_ref[p1][row][3] << 4;
            uint8_t b3 = PGB_patterns_ref[p2][row][0] << 3;
            uint8_t b2 = PGB_patterns_ref[p2][row][1] << 2;
            uint8_t b1 = PGB_patterns_ref[p3][row][2] << 1;
            uint8_t b0 = PGB_patterns_ref[p3][row][3] << 0;

            PGB_dither_lut[row][idx] = (uint8_t)(b7 | b6 | b5 | b4 | b3 | b2 | b1 | b0);
        }
    }
}

// Fast word-parallel 4-pixel/8-pixel LUT dithering
static void fast_dither_frame(uint8_t *framebuffer, uint8_t (*front_fb)[160], uint8_t (*back_fb)[160], bool back_fb_enabled)
{
    int skip_counter = 0;
    bool single_line = false;
    int y2 = 0;
    int lcd_rows = 0;
    int row_offset = 52;
    int row_offset2 = 52 * 2;
    int y_offset;
    int next_y_offset = 2;

    for(int y = 0; y < (LCD_HEIGHT - 1); y++)
    {
        y_offset = next_y_offset;
        if(skip_counter == 5) {
            y_offset = 1;
            next_y_offset = 1;
            skip_counter = 0;
            single_line = true;
        } else if(single_line) {
            next_y_offset = 2;
            single_line = false;
        }

        const uint8_t *pixels = back_fb_enabled ? front_fb[y] : back_fb[y];

        int d_row1 = y2 & 3;
        int d_row2 = (y2 + 1) & 3;
        uint32_t *dst1_32 = (uint32_t*)&framebuffer[lcd_rows];
        uint32_t *dst2_32 = (uint32_t*)&framebuffer[lcd_rows + row_offset];

        const uint8_t *lut1 = PGB_dither_lut[d_row1];
        const uint8_t *lut2 = PGB_dither_lut[d_row2];

        if(y_offset == 2) {
            for(int x = 0; x < LCD_WIDTH; x += 16) {
                uint8_t q0 = (pixels[x] & 3) | ((pixels[x+1] & 3) << 2) | ((pixels[x+2] & 3) << 4) | ((pixels[x+3] & 3) << 6);
                uint8_t q1 = (pixels[x+4] & 3) | ((pixels[x+5] & 3) << 2) | ((pixels[x+6] & 3) << 4) | ((pixels[x+7] & 3) << 6);
                uint8_t q2 = (pixels[x+8] & 3) | ((pixels[x+9] & 3) << 2) | ((pixels[x+10] & 3) << 4) | ((pixels[x+11] & 3) << 6);
                uint8_t q3 = (pixels[x+12] & 3) | ((pixels[x+13] & 3) << 2) | ((pixels[x+14] & 3) << 4) | ((pixels[x+15] & 3) << 6);

                uint32_t w1 = (uint32_t)lut1[q0] | ((uint32_t)lut1[q1] << 8) | ((uint32_t)lut1[q2] << 16) | ((uint32_t)lut1[q3] << 24);
                uint32_t w2 = (uint32_t)lut2[q0] | ((uint32_t)lut2[q1] << 8) | ((uint32_t)lut2[q2] << 16) | ((uint32_t)lut2[q3] << 24);

                *dst1_32++ = w1;
                *dst2_32++ = w2;
            }
        } else {
            for(int x = 0; x < LCD_WIDTH; x += 16) {
                uint8_t q0 = (pixels[x] & 3) | ((pixels[x+1] & 3) << 2) | ((pixels[x+2] & 3) << 4) | ((pixels[x+3] & 3) << 6);
                uint8_t q1 = (pixels[x+4] & 3) | ((pixels[x+5] & 3) << 2) | ((pixels[x+6] & 3) << 4) | ((pixels[x+7] & 3) << 6);
                uint8_t q2 = (pixels[x+8] & 3) | ((pixels[x+9] & 3) << 2) | ((pixels[x+10] & 3) << 4) | ((pixels[x+11] & 3) << 6);
                uint8_t q3 = (pixels[x+12] & 3) | ((pixels[x+13] & 3) << 2) | ((pixels[x+14] & 3) << 4) | ((pixels[x+15] & 3) << 6);

                uint32_t w1 = (uint32_t)lut1[q0] | ((uint32_t)lut1[q1] << 8) | ((uint32_t)lut1[q2] << 16) | ((uint32_t)lut1[q3] << 24);

                *dst1_32++ = w1;
            }
        }

        y2 += y_offset;
        lcd_rows += (y_offset == 1) ? row_offset : row_offset2;
        if(!single_line) {
            skip_counter++;
        }
    }
}

// Generate a synthetic benchmark Game Boy ROM
static uint8_t* generate_benchmark_rom(size_t *rom_size_out)
{
    size_t size = 32768; // 32KB
    uint8_t *rom = (uint8_t*)calloc(1, size);

    // Entry point: 0x0100
    rom[0x0100] = 0x00; // NOP
    rom[0x0101] = 0xC3; // JP 0x0150
    rom[0x0102] = 0x50;
    rom[0x0103] = 0x01;

    // Title: "BENCHMARK"
    memcpy(&rom[0x0134], "BENCHMARK", 9);
    rom[0x0147] = 0x00; // ROM ONLY
    rom[0x0148] = 0x00; // 32KB
    rom[0x0149] = 0x00; // No RAM

    // Compute header checksum at 0x014D
    uint8_t checksum = 0;
    for(uint16_t addr = 0x0134; addr <= 0x014C; addr++) {
        checksum = checksum - rom[addr] - 1;
    }
    rom[0x014D] = checksum;

    // VBLANK Interrupt Handler at 0x0040
    rom[0x0040] = 0xD9; // RETI

    // TIMA Interrupt Handler at 0x0050
    rom[0x0050] = 0xD9; // RETI

    // Code at 0x0150
    uint16_t pc = 0x0150;

    // Disable interrupts: DI (0xF3)
    rom[pc++] = 0xF3;

    // Set SP: LD SP, 0xFFFE
    rom[pc++] = 0x31; rom[pc++] = 0xFE; rom[pc++] = 0xFF;

    // Setup BGP = 0xE4 (11 10 01 00): LD A, 0xE4; LDH (0x47), A
    rom[pc++] = 0x3E; rom[pc++] = 0xE4;
    rom[pc++] = 0xE0; rom[pc++] = 0x47;

    // Setup OBP0 = 0xE4; LDH (0x48), A
    rom[pc++] = 0xE0; rom[pc++] = 0x48;

    // Setup OBP1 = 0xD2; LD A, 0xD2; LDH (0x49), A
    rom[pc++] = 0x3E; rom[pc++] = 0xD2;
    rom[pc++] = 0xE0; rom[pc++] = 0x49;

    // Fill VRAM tile patterns (0x8000 to 0x8100)
    rom[pc++] = 0x21; rom[pc++] = 0x00; rom[pc++] = 0x80;
    rom[pc++] = 0x01; rom[pc++] = 0x00; rom[pc++] = 0x01;
    uint16_t vram_loop = pc;
    rom[pc++] = 0x7D;       // LD A, L
    rom[pc++] = 0x22;       // LD (HL+), A
    rom[pc++] = 0x0B;       // DEC BC
    rom[pc++] = 0x78;       // LD A, B
    rom[pc++] = 0xB1;       // OR C
    int8_t vram_rel = (int8_t)(vram_loop - (pc + 2));
    rom[pc++] = 0x20; rom[pc++] = (uint8_t)vram_rel; // JR NZ

    // Fill Tilemap (0x9800 to 0x9A00)
    rom[pc++] = 0x21; rom[pc++] = 0x00; rom[pc++] = 0x98;
    rom[pc++] = 0x01; rom[pc++] = 0x00; rom[pc++] = 0x02;
    uint16_t map_loop = pc;
    rom[pc++] = 0x7D;       // LD A, L
    rom[pc++] = 0xE6; rom[pc++] = 0x0F; // AND 0x0F
    rom[pc++] = 0x22;       // LD (HL+), A
    rom[pc++] = 0x0B;       // DEC BC
    rom[pc++] = 0x78;       // LD A, B
    rom[pc++] = 0xB1;       // OR C
    int8_t map_rel = (int8_t)(map_loop - (pc + 2));
    rom[pc++] = 0x20; rom[pc++] = (uint8_t)map_rel; // JR NZ

    // Setup 10 OAM Sprites at 0xFE00
    rom[pc++] = 0x21; rom[pc++] = 0x00; rom[pc++] = 0xFE;
    for(int s = 0; s < 10; s++) {
        rom[pc++] = 0x3E; rom[pc++] = 16 + s * 12; // Y
        rom[pc++] = 0x22; // LD (HL+), A
        rom[pc++] = 0x3E; rom[pc++] = 8 + s * 14;  // X
        rom[pc++] = 0x22; // LD (HL+), A
        rom[pc++] = 0x3E; rom[pc++] = s & 7;       // Tile ID
        rom[pc++] = 0x22; // LD (HL+), A
        rom[pc++] = 0x3E; rom[pc++] = (s & 1) ? 0x10 : 0x00; // Flags
        rom[pc++] = 0x22; // LD (HL+), A
    }

    // Enable LCD: LCDC = 0x93
    rom[pc++] = 0x3E; rom[pc++] = 0x93;
    rom[pc++] = 0xE0; rom[pc++] = 0x40;

    // Enable Sound: NR52 = 0x80, NR50 = 0x77, NR51 = 0xFF
    rom[pc++] = 0x3E; rom[pc++] = 0x80; rom[pc++] = 0xE0; rom[pc++] = 0x26;
    rom[pc++] = 0x3E; rom[pc++] = 0x77; rom[pc++] = 0xE0; rom[pc++] = 0x24;
    rom[pc++] = 0x3E; rom[pc++] = 0xFF; rom[pc++] = 0xE0; rom[pc++] = 0x25;
    // Play Square 1: NR10 = 0x16, NR11 = 0x80, NR12 = 0xF3, NR13 = 0x83, NR14 = 0x87
    rom[pc++] = 0x3E; rom[pc++] = 0x16; rom[pc++] = 0xE0; rom[pc++] = 0x10;
    rom[pc++] = 0x3E; rom[pc++] = 0x80; rom[pc++] = 0xE0; rom[pc++] = 0x11;
    rom[pc++] = 0x3E; rom[pc++] = 0xF3; rom[pc++] = 0xE0; rom[pc++] = 0x12;
    rom[pc++] = 0x3E; rom[pc++] = 0x83; rom[pc++] = 0xE0; rom[pc++] = 0x13;
    rom[pc++] = 0x3E; rom[pc++] = 0x87; rom[pc++] = 0xE0; rom[pc++] = 0x14;

    // Main workload loop:
    uint16_t main_loop = pc;

    // Heavy CPU computation: 500 iterations of arithmetic
    rom[pc++] = 0x01; rom[pc++] = 0xF4; rom[pc++] = 0x01; // LD BC, 500
    uint16_t math_loop = pc;
    rom[pc++] = 0x78;       // LD A, B
    rom[pc++] = 0x81;       // ADD A, C
    rom[pc++] = 0x90;       // SUB B
    rom[pc++] = 0xCB; rom[pc++] = 0x37; // SWAP A
    rom[pc++] = 0x17;       // RLA
    rom[pc++] = 0x0B;       // DEC BC
    rom[pc++] = 0x78;       // LD A, B
    rom[pc++] = 0xB1;       // OR C
    int8_t math_rel = (int8_t)(math_loop - (pc + 2));
    rom[pc++] = 0x20; rom[pc++] = (uint8_t)math_rel; // JR NZ

    // Scroll Background: LDH A, (0x43); INC A; LDH (0x43), A
    rom[pc++] = 0xF0; rom[pc++] = 0x43;
    rom[pc++] = 0x3C;
    rom[pc++] = 0xE0; rom[pc++] = 0x43;

    // Wait for VBLANK: LDH A, (0x44); CP 144; JR NZ, VBlankWait
    uint16_t vbl_wait = pc;
    rom[pc++] = 0xF0; rom[pc++] = 0x44; // LDH A, (LY)
    rom[pc++] = 0xFE; rom[pc++] = 144;  // CP 144
    int8_t vbl_rel = (int8_t)(vbl_wait - (pc + 2));
    rom[pc++] = 0x20; rom[pc++] = (uint8_t)vbl_rel;

    // Loop back to main_loop: JP main_loop
    rom[pc++] = 0xC3;
    rom[pc++] = (uint8_t)(main_loop & 0xFF);
    rom[pc++] = (uint8_t)(main_loop >> 8);

    *rom_size_out = size;
    return rom;
}

static void gb_error_cb(struct gb_s *gb, const enum gb_error_e gb_err, const uint16_t val)
{
    printf("[GB_ERROR] code %d, val 0x%04X\n", gb_err, val);
}

int main(int argc, char **argv)
{
    int num_frames = 1000;
    const char *rom_file_path = NULL;

    for(int i = 1; i < argc; i++) {
        if(isdigit(argv[i][0])) {
            num_frames = atoi(argv[i]);
        } else {
            rom_file_path = argv[i];
        }
    }
    if(num_frames <= 0) num_frames = 1000;

    init_reference_bitmask();
    init_fast_dither_lut();

    uint8_t *rom = NULL;
    size_t rom_size = 0;

    if(rom_file_path != NULL) {
        FILE *f = fopen(rom_file_path, "rb");
        if(!f) {
            printf("Error: Could not open ROM file '%s'\n", rom_file_path);
            return 1;
        }
        fseek(f, 0, SEEK_END);
        rom_size = ftell(f);
        fseek(f, 0, SEEK_SET);
        rom = (uint8_t*)malloc(rom_size);
        if(!rom || fread(rom, 1, rom_size, f) != rom_size) {
            printf("Error reading ROM file '%s'\n", rom_file_path);
            fclose(f);
            return 1;
        }
        fclose(f);
        printf("====================================================\n");
        printf(" PlayGB Benchmark: %s (%zu KB, %d frames)\n", rom_file_path, rom_size / 1024, num_frames);
        printf("====================================================\n");
    } else {
        printf("====================================================\n");
        printf(" PlayGB Synthetic Benchmark Suite (%d frames)\n", num_frames);
        printf("====================================================\n");
        rom = generate_benchmark_rom(&rom_size);
    }

    uint8_t *wram = (uint8_t*)calloc(1, WRAM_SIZE);
    uint8_t *vram = (uint8_t*)calloc(1, VRAM_SIZE);

    struct gb_s gb;
    enum gb_init_error_e ret = gb_init(&gb, wram, vram, rom, gb_error_cb, NULL);
    if(ret != GB_INIT_NO_ERROR) {
        printf("Failed to init GB emulator: %d\n", ret);
        return 1;
    }

    // Allocate cart RAM if game uses RAM
    uint_fast32_t save_size = gb_get_save_size(&gb);
    if(save_size > 0) {
        gb.gb_cart_ram = (uint8_t*)calloc(1, save_size);
        __gb_update_ram_bank(&gb);
    }

    gb.direct.sound = 1;
    audio_init();
    gb_init_lcd(&gb);

    uint8_t *ref_fb = (uint8_t*)calloc(1, 64 * 256);
    uint8_t *fast_fb = (uint8_t*)calloc(1, 64 * 256);

    LARGE_INTEGER freq, t_start, t_end;
    QueryPerformanceFrequency(&freq);

    double total_cpu_ppu_time_ms = 0.0;
    double total_ref_dither_time_ms = 0.0;
    double total_fast_dither_time_ms = 0.0;

    int verification_errors = 0;

    for(int frame = 0; frame < num_frames; frame++)
    {
        // 1. Emulate Frame (CPU + PPU)
        QueryPerformanceCounter(&t_start);
        gb_run_frame(&gb);
        QueryPerformanceCounter(&t_end);
        total_cpu_ppu_time_ms += (double)(t_end.QuadPart - t_start.QuadPart) * 1000.0 / (double)freq.QuadPart;

        // 2. Reference Dither
        QueryPerformanceCounter(&t_start);
        reference_dither_frame(ref_fb, gb_front_fb, gb_back_fb, gb.display.back_fb_enabled);
        QueryPerformanceCounter(&t_end);
        total_ref_dither_time_ms += (double)(t_end.QuadPart - t_start.QuadPart) * 1000.0 / (double)freq.QuadPart;

        // 3. Fast Dither
        QueryPerformanceCounter(&t_start);
        fast_dither_frame(fast_fb, gb_front_fb, gb_back_fb, gb.display.back_fb_enabled);
        QueryPerformanceCounter(&t_end);
        total_fast_dither_time_ms += (double)(t_end.QuadPart - t_start.QuadPart) * 1000.0 / (double)freq.QuadPart;

        // Verify exact bit-for-bit match on 1-bit framebuffer!
        if(memcmp(ref_fb, fast_fb, 52 * 240) != 0) {
            verification_errors++;
        }
    }

    double ref_total_ms = total_cpu_ppu_time_ms + total_ref_dither_time_ms;
    double fast_total_ms = total_cpu_ppu_time_ms + total_fast_dither_time_ms;

    printf("\n--- Benchmark & Verification Results (%d frames) ---\n", num_frames);
    printf("1-Bit Display Output Check : %s (%d mismatches)\n",
           verification_errors == 0 ? "PASSED (100% Bit-Exact Match)" : "FAILED", verification_errors);
    printf("CPU + PPU Emulation Time   : %8.2f ms (%6.3f ms/frame)\n",
           total_cpu_ppu_time_ms, total_cpu_ppu_time_ms / num_frames);
    printf("Original Dithering Time    : %8.2f ms (%6.3f ms/frame)\n",
           total_ref_dither_time_ms, total_ref_dither_time_ms / num_frames);
    printf("Optimized Dithering Time   : %8.2f ms (%6.3f ms/frame) -> [%4.1fx faster!]\n",
           total_fast_dither_time_ms, total_fast_dither_time_ms / num_frames,
           total_ref_dither_time_ms / total_fast_dither_time_ms);
    printf("Overall Frame Time (Old)   : %8.3f ms/frame (%6.1f FPS)\n",
           ref_total_ms / num_frames, 1000.0 / (ref_total_ms / num_frames));
    printf("Overall Frame Time (New)   : %8.3f ms/frame (%6.1f FPS)\n",
           fast_total_ms / num_frames, 1000.0 / (fast_total_ms / num_frames));
    printf("====================================================\n");

    fflush(stdout);
    return 0;
}
