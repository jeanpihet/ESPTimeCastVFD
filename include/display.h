#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <PT6302.h>

/**
 * @class display class - Vacuum Fluorescent Display on Belgacom TV Box
 */
class display
{

public:

    /**
     * Constructor for the vfd class
     * @param CLKB Serial clock pin, connect to CLKB pin of IC
     * @param RSTB Reset pin pin, connect to RSTB pin of IC
     * @param CSB Chip select pin, connect to CSB pin of IC
     * @param DIN Data out pin, connect to DIN pin of IC
     */
    display(unsigned int clkb, unsigned int rstb, unsigned int csb, unsigned int din);

    // Print time on small digits, 1st line left
    void print_time_small(int hour, int min, bool on);

    // Print time on big digits, 1st line middle
    void print_time_big(int hour, int min, bool on);

    // Print on the 2nd line, which has 12 fully graphical digits
    void print_2nd_line(const char *c, bool overwrite);

    // Red clock icon on/off
    void clock_icon(bool on);

    void test_digits(int start);


private:

    // Instance of the PT6302 VFD controler
    PT6302 *vfd;

    // Num 0-9 to 7 segments. Bits order is MSB - g, f, e, d, c, b, a - LSB
    uint8_t to_7_seg(uint8_t num);

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
    void set_segment(uint seg_nr, uint8_t *cgdata);

    // Fill in VFD segments CGData from num 0-9
    void to_seg_number(uint8_t num, uint *seg, uint8_t *cgdata);

    // Print time on digits, 1st line.
    void print_time(int hour, int min, int start, int cgr_idx, bool on, bool dot_or_column);

};

#endif