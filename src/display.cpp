#include "display.h"

display::display(unsigned int clkb, unsigned int rstb, unsigned int csb, unsigned int din)
{
    vfd = new PT6302((PT6302::Pin) clkb, (PT6302::Pin) rstb, (PT6302::Pin) csb, (PT6302::Pin) din);
}

void display::init()
{
    vfd->init();

    // Set the general purpose output pins. GP1 enables the -30V power supply to the vfd
    vfd->setGPOP(true, false);

    // Set to normal operation mode
    vfd->setMode(PT6302::Mode::NORMAL);

    // Set the amount of digits
    vfd->setDigitNo(16);

    // Set the amount of duty cycles = brightness
    vfd->setDuty(15);

    // Clear the display after a reset otherwise it will show garbage
    vfd->clear();

    // Print a string at a specific position
    //vfd->print(3, "43", false);

    Serial.println("VFD init done!");
}

// Set brightness 8..15
void display::setIntensity(int brightness)
{
    vfd->setDuty(brightness);
}

// Print date on small digits, 1st line left
// Segments are at position 15
// Uses CGRAM char 0
// Other segments are:
//  'TITLE' : 20
//  'CH' : 32
//  'TRACK' : 15
void display::print_date_small(int day, int month, bool on)
{
    print_digits(day, month, 15, 0, on, false);
}

// Print time on big digits, 1st line middle
// Segments are at position 14
// Uses CGRAM char 1
// Other segments are:
//  'AM' : 20
//  'PM' : 34
void display::print_time_big(int hour, int min, bool on)
{
    print_digits(hour, min, 14, 1, on, true);
}


// Print on the 2nd line, which has 12 (1..12) fully graphical digits.
// Always print centered, excepted if the string is too long it will scroll R->L

//  Print unique char
void display::print(const char c)
{
    char l[16] = { 0x20 };

    // The digits order is reversed (right to left) by the HW and PT6302 lib (???)
    l[5] = c;
    l[DISP_MAX_LEN] = 0;

    vfd->print(1, l, false);
}

//  Print string
//  ToDo: scrolling if string too long
void display::print(std::string str)
{
    char l[DISP_MAX_LEN + 1];
    int len, start;

    memset(l, 0x20, sizeof(l));

    // Save string for later scrolling
    strncpy(print_str, str.c_str(), PRINT_MAX_LEN - 1);
    print_str[PRINT_MAX_LEN - 1] = 0;
    len = strlen(print_str);

    // Center string if too short
    if (len >= DISP_MAX_LEN) {
        len = DISP_MAX_LEN;
        start = 0;
    } else {
        start = (DISP_MAX_LEN - len) / 2;
    }

    // The digits order is reversed (right to left) by the HW and PT6302 lib (???)
    for (int i = 0; i < len; i++)
        l[DISP_MAX_LEN - i - 1 - start] = print_str[i];
    l[DISP_MAX_LEN] = 0;

    vfd->print(1, l, false);
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
            std::string str;
            vfd->writeDCRAM(start, 1);
            delay(100 * DELAY_MULT);
            sprintf(msg, "Seg %d    ", i * 7 + j);
            str = msg;
            print(str);
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

// 1st line segments: "1XX:YY   1XX:YY"
//
// Segments a..g to VFD segment numbers. Order is MSB - g, f, e, d, c, b, a - LSB
//
//  1-2 digits 1XX:
//      100 '1': segment 27
//      10
uint seg_10_xx[7] = { 13, 33, 12, 5, 19, 6, 26 };
//      1
uint seg_1_xx[7] = { 25, 11, 24, 17, 31, 18, 4 };
//
//  ':' : segments 3 (up), 10 (down)
//
//  3-4 digits YY:
//      10
uint seg_10_yy[7] = { 30, 16, 29, 22, 2, 23, 9 };
//      1
uint seg_1_yy[7] = { 8, 28, 7, 0, 14, 1, 21 };

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

// Print time or date on digits XX[.:]YY, 1st line.
//  start is the position (13..15)
//  cgr_idx is the CGRAM char to use (0..7)
void display::print_digits(int xx, int yy, int start, int cgr_idx, bool on, bool dot_or_column)
{
    uint8_t CGdata[5] = { 0, 0, 0, 0, 0};
    int tmp;

    // If off, clear display
    if (!on)
        goto display;

    // Limit numbers to 0..99
    if (xx < 0)
        xx = 0;
    if (xx > 99)
        xx = 99;
    if (yy < 0)
        yy = 0;
    if (yy > 99)
        yy = 99;

    // Convert time in segments
    //  2nd 7-segments
    tmp = xx % 10;
    to_seg_number(tmp, seg_1_xx, CGdata);
    //  1st 7-segments
    tmp = (xx / 10) % 10;
    to_seg_number(tmp, seg_10_xx, CGdata);
    //  4th 7-segments
    tmp = yy % 10;
    to_seg_number(tmp, seg_1_yy, CGdata);
    //  3rd 7-segments
    tmp = (yy / 10) % 10;
    to_seg_number(tmp, seg_10_yy, CGdata);

    // Column points: only down (.) or up-down (:)
    if (dot_or_column)
        set_segment(3, CGdata);
    set_segment(10, CGdata);

display:
    vfd->writeCGRAM(cgr_idx, CGdata);
    vfd->writeDCRAM(start, cgr_idx);
}
