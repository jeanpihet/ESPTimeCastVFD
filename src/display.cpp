#include "display.h"

display::display(unsigned int clkb, unsigned int rstb, unsigned int csb, unsigned int din)
{
    vfd = new PT6302((PT6302::Pin) clkb, (PT6302::Pin) rstb, (PT6302::Pin) csb, (PT6302::Pin) din);

    vfd->init();

    // Set the general purpose output pins. GP1 enables the -30V power supply to the vfd->
    Serial.println("GPOP set");
    vfd->setGPOP(true, false);

    // Set to normal operation mode
    Serial.println("Mode set");
    vfd->setMode(PT6302::Mode::NORMAL);

    // Set the amount of digits
    Serial.println("Digit set");
    vfd->setDigitNo(16);

    // Set the amount of duty cycles
    Serial.println("Duty set");
    vfd->setDuty(15);

    // Clear the display after a reset otherwise it will show garbage
    Serial.println("Clear");
    vfd->clear();

    // Print a string at a specific position
    //Serial.println("Print at position");
    //vfd->print(3, "43", false);
}

// Print time on small digits, 1st line left
// Segments are at position 15
// Uses CGRAM char 0
// Other segments are:
//  'TITLE' : 20
//  'CH' : 32
//  'TRACK' : 15
void display::print_time_small(int hour, int min, bool on)
{
    print_time(hour, min, 15, 0, on, true);
}

// Print time on big digits, 1st line middle
// Segments are at position 14
// Uses CGRAM char 1
// Other segments are:
//  'AM' : 20
//  'PM' : 34
void display::print_time_big(int hour, int min, bool on)
{
    print_time(hour, min, 14, 1, on, false);
}

// Print on the 2nd line, which has 12 (1..12) fully graphical digits.
void display::print_2nd_line(const char *c, bool overwrite)
{
    char l[16];
    int len = strlen(c);

    memset(l, 0x20, 12);
    if (len > 12)
        len = 12;

    // The digits order is reversed (right to left) by the HW and PT6302 lib (???)
    for (int i = 0; i < len; i++)
        l[12 - i - 1] = c[i];
    l[12] = 0;

    vfd->print(1, l, overwrite);
}

// Red clock icon on/off
// Segments are at position 13
// Uses CGRAM char 2
// Red clock icon is segment 20
void display::clock_icon(bool on)
{
    uint8_t CGdata[5] = { 0, 0, 0, 0, 0};

    if (on)
        set_segment(20, CGdata);

    vfd->writeCGRAM(2, CGdata);
    vfd->writeDCRAM(13, 2);
}

// Test segments on a given position. Used to decode the icons on top.
// Positions of the icons are 13..15.
//  Uses CGRAM char 0, 1
void display::test_digits(int start)
{
    uint8_t CGdata[5];
    char msg[32];

#define DELAY_MULT     10

    // Full character
    CGdata[0] = CGdata[1] = CGdata[2] = CGdata[3] = CGdata[4] = 0x7F;
    vfd->writeCGRAM(1, CGdata);

    for (int i = 0; i < 5; i++) {
        CGdata[0] = CGdata[1] = CGdata[2] = CGdata[3] = CGdata[4] = 0;
        for (int j = 0; j < 7; j++) {
            vfd->writeDCRAM(start, 1);
            delay(100 * DELAY_MULT);
            sprintf(msg, "Seg %d    ", i * 7 + j);
            print_2nd_line(msg, false);
            // Only one segment
            CGdata[i] = 1 << j;
            sprintf(msg, "Seg %d (%d=%02x) ", i * 7 + j, i, CGdata[i]);
            Serial.println(msg);
            vfd->writeCGRAM(0, CGdata);
            vfd->writeDCRAM(start, 0);
            delay(100 * DELAY_MULT);
            vfd->writeDCRAM(start, 1);
            delay(30 * DELAY_MULT);
            vfd->writeDCRAM(start, 0);
            delay(100 * DELAY_MULT);
            vfd->writeDCRAM(start, 1);
            delay(30 * DELAY_MULT);
            vfd->writeDCRAM(start, 0);
            delay(100 * DELAY_MULT);
            vfd->writeDCRAM(start, 1);
            delay(30 * DELAY_MULT);
            vfd->writeDCRAM(start, 0);
            delay(100 * DELAY_MULT);
        }
    }
    vfd->print(start, " ", false);
}

// Num 0-9 to 7 segments. Bits order is MSB - g, f, e, d, c, b, a - LSB
uint8_t display::to_7_seg(uint8_t num)
{
    uint8_t seg[10] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };

    return seg[num % 10];
}

// Segments a..g to VFD segment numbers. Order is MSB - g, f, e, d, c, b, a - LSB
//  ':' : segments 3 (up), 10 (down)
//  100 hour '1': segment 27
//  10 hour
uint seg_10_hour[7] = { 13, 33, 12, 5, 19, 6, 26 };
//  1 hour
uint seg_1_hour[7] = { 25, 11, 24, 17, 31, 18, 4 };
//  10 min
uint seg_10_min[7] = { 30, 16, 29, 22, 2, 23, 9 };
// 1 min
uint seg_1_min[7] = { 8, 28, 7, 0, 14, 1, 21 };

// Set segment in CGData bitmask
//  CGData is 5 bytes with 7 bits each = 35 segments
void display::set_segment(uint seg_nr, uint8_t *cgdata)
{
    seg_nr %= 35;
    cgdata[seg_nr / 7] |= 1 << (seg_nr % 7);
}

// Fill in VFD segments CGData from num 0-9
void display::to_seg_number(uint8_t num, uint *seg, uint8_t *cgdata)
{
    // Look up 7-segments to display
    for (int i = 0; i < 7; i++) {
        // If lit, fill in bit for VFD segment number
        if (to_7_seg(num) & (1 << i)) {
            set_segment(seg[i], cgdata);
        }
    }
}

// Print time on digits, 1st line.
//  start is the position (13..15)
//  cgr_idx is the CGRAM char to use (0..7)
void display::print_time(int hour, int min, int start, int cgr_idx, bool on, bool dot_or_column)
{
    uint8_t CGdata[5] = { 0, 0, 0, 0, 0};
    int tmp;

    // If off, clear display
    if (!on)
        goto display;

    // Convert time in segments
    //  1 hour
    tmp = hour % 10;
    to_seg_number(tmp, seg_1_hour, CGdata);
    //  10 hour
    tmp = (hour / 10) % 10;
    to_seg_number(tmp, seg_10_hour, CGdata);
    //  1 min
    tmp = min % 10;
    to_seg_number(tmp, seg_1_min, CGdata);
    //  10 min
    tmp = (min / 10) % 10;
    to_seg_number(tmp, seg_10_min, CGdata);
    // Column points: only down (.) or up-down (:)
    if (dot_or_column)
        set_segment(3, CGdata);
    set_segment(10, CGdata);

display:
    vfd->writeCGRAM(cgr_idx, CGdata);
    vfd->writeDCRAM(start, cgr_idx);
}
