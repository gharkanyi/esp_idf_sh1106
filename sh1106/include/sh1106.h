
#ifndef SH1106_H
#define SH1106_H

#include <stdbool.h>

#define Byte uint8_t

#define SH1106_HEIGHT    64
#define SH1106_NUM_PAGES 8
#define SH1106_WIDTH     128

//device structure

typedef struct SH1106_t{
    Byte pages[SH1106_NUM_PAGES][SH1106_WIDTH];
    int cursor_row; 
    int cursor_col; 
}SH1106_t;

//init
SH1106_t* sh1106_init();

//drawing funcs
int sh1106_draw_pixel(SH1106_t* dev, int x, int y, bool on);
int sh1106_draw_rect(SH1106_t* dev, int x1, int y1, int x2, int y2, bool on);
int sh1106_draw_line(SH1106_t* dev, int x1, int y1, int x2, int y2, bool on);
int sh1106_clear_buffer(SH1106_t* dev);
int sh1106_update_display(SH1106_t* dev);

//typing funcs
int sh1106_set_cursor(SH1106_t* dev, int row, int col);
int sh1106_write_char(SH1106_t* dev, char c);
int sh1106_write_string(SH1106_t* dev, const char* string, int n);

//scrolling funcs
int sh1106_set_display_start_line(int line_addr);

#endif