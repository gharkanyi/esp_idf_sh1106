#include <stdio.h>
#include "sh1106.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "math.h"
#include "sh1106_chars_8x8.h"


//SH1106 i2c address
#define SH1106_I2C_ADDR 0x78

//SH1106 control byte masks
#define SH1106_CONTROL_NOT_LAST 0x80
#define SH1106_CONTROL_RAM_OP   0x40

//SH1106 commands
#define SH1106_COMMAND_DISPLAY_ON  0xAF
#define SH1106_COMMAND_DISPLAY_OFF 0xAE
#define SH1106_COMMAND_RMW         0xE0
#define SH1106_COMMAND_END         0xEE

//SH1106 Misc

//these commands are meant to be ORed with values 0x00 - 0x0F
#define SH1106_COMMAND_SET_PAGE_ADDR      0xB0
#define SH1106_COMMAND_SET_UPPER_COL_ADDR 0x10
#define SH1106_COMMAND_SET_LOWER_COL_ADDR 0x00

//this command is meant to be ORed with values 0x00 - 0x3F
#define SH1106_COMMAND_SET_DISPLAY_START_LINE  0x40

//Helper Functions-----------------------------------------------------
int sh1106_send_command(Byte com){
    esp_err_t err;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    //send slave address and r/w bit
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, SH1106_I2C_ADDR | I2C_MASTER_WRITE, true);

    //send control byte and command
    i2c_master_write_byte(cmd, 0x00, true); //last control byte, command op
    i2c_master_write_byte(cmd, com, true); 
    i2c_master_stop(cmd);
    err = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return err;
}

int set_page_num(Byte page_num){
    if(page_num > SH1106_NUM_PAGES - 1){
        ESP_LOGE("set_page_num", "page number out of range");
        return 1;
    }
    sh1106_send_command(SH1106_COMMAND_SET_PAGE_ADDR | page_num);
    return 0;
}

int set_col_num(Byte col_num){
    if(col_num > SH1106_WIDTH - 1){
        ESP_LOGE("set_col_num", "column number out of range");
        return 1;
    }
    sh1106_send_command(SH1106_COMMAND_SET_UPPER_COL_ADDR | (col_num >> 4));
    sh1106_send_command(SH1106_COMMAND_SET_LOWER_COL_ADDR | (col_num & 0x0F));
    return 0;
}

bool point_on_screen(int x, int y){
    if( x >= SH1106_WIDTH  || x < 0 || y >= SH1106_HEIGHT || y < 0){
        return false;
    }
    return true;
}

bool cursor_on_screen(int row, int col){
    if(row < 0 || row > (SH1106_WIDTH / 8) * 7){
        return false;
    }
    return true;
}

int cursor_next_row(SH1106_t* dev){
    if(++(dev->cursor_row) == (SH1106_HEIGHT / 8) * 7){
        dev->cursor_row = 0;
    }
    return dev->cursor_row;
}

int cursor_next_col(SH1106_t* dev){
    if(++(dev->cursor_col) == (SH1106_WIDTH / 8) * 7){
        dev->cursor_col = 0;
    }
    return dev->cursor_col;
}

//Public Functions----------------------------------------------------
SH1106_t* sh1106_init(){
    sh1106_send_command(SH1106_COMMAND_DISPLAY_ON);
    SH1106_t* dev = (SH1106_t*) malloc(sizeof(SH1106_t)); 
    dev->cursor_col = 0;
    dev->cursor_row = 0;
    sh1106_clear_buffer(dev);
    return dev;
}

int sh1106_draw_pixel(SH1106_t* dev, int x, int y, bool on){
    if(!point_on_screen(x, y)){
        return 1; 
    }
    int page_num = y / 8;
    int row_num = y % 8;
    int col_num = x;

    if(on){
        dev->pages[page_num][col_num] |= (0x01 << row_num);
    }
    else{
        dev->pages[page_num][col_num] &= ~(0x01 << row_num);
    }
    return 0; 
}

int sh1106_draw_rect(SH1106_t* dev, int x1, int y1, int x2, int y2, bool on){
    if(!point_on_screen(x1,y1) || !point_on_screen(x2, y2)){
        return 1;
    }
    Byte y1_page_num = y1 / 8;
    Byte y2_page_num = y2 / 8;
    Byte y1_row_num = y1 % 8;
    Byte y2_row_num = y2 % 8;

    Byte new_data = 0x00;

    //work out initial data value for first page
    if(y1_page_num == y2_page_num){
        new_data = 0x00;
        for(int i = 0; i <= (y2 - y1); i++){
            new_data <<= 1;
            new_data |= 0x01; 
        }
        new_data <<= y1_row_num;
    }
    else{
        new_data = 0xFF << y1_row_num;
    }

    Byte curr_page = y1_page_num;

    //loop through pages and fill them up to column x2
    while(curr_page <= y2_page_num){
        Byte curr_col = x1;
        while(curr_col <= x2){
            dev->pages[curr_page][curr_col] = new_data;
            curr_col++;
        }
        curr_page++;

        new_data = 0xFF;
        if(curr_page == y2_page_num){
            new_data >>= (7 - y2_row_num);
        }
    }
    return 0;
}

int sh1106_draw_line(SH1106_t* dev, int x1, int y1, int x2, int y2, bool on){
    if(!point_on_screen(x1,y1) || !point_on_screen(x2, y2)){
        return 1;
    }

    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);

    if(dx > dy){
        double slope = ((double)y2 - y1) / ((double)x2 - x1);

        int unit = 1;

        int curr_x = x1;
        int endpoint = x2;
        double curr_y = y1;

        if(x2 < x1){
            curr_y = y2;
            curr_x = x2;
            endpoint = x1;
        }

        while(curr_x <= endpoint){
            sh1106_draw_pixel(dev, curr_x, (int)curr_y, on);
            curr_x += unit;
            curr_y += slope;
        }
    }
    else{
        double slope = ((double)x2 - x1) / ((double)y2 - y1);

        int unit = 1;

        double curr_x = x1;
        double curr_y = y1;
        int endpoint = y2;

        if(y2 < y1){
            curr_y = y2;
            endpoint = y1;
            curr_x = x2;
        }

        while(curr_y <= endpoint){
            sh1106_draw_pixel(dev, curr_x, (int)curr_y, on);
            curr_y += unit;
            curr_x += slope;
        }

    }
    return 0;
}

int sh1106_update_display(SH1106_t* dev){
    //matrix dimensions are 132x64 but the screen dimensions are 128x64.
    //The screen ignores the first and last two columns to acheive this.
    //An offset of 2 will draw things where they should be.
    set_col_num(2);
    set_page_num(0);

    for(int i = 0; i < SH1106_NUM_PAGES; i++){
        set_page_num(i);
        sh1106_send_command(SH1106_COMMAND_RMW);
        
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, SH1106_I2C_ADDR | I2C_MASTER_WRITE, true);
        i2c_master_write_byte(cmd, SH1106_CONTROL_RAM_OP, true);
        i2c_master_write(cmd, dev->pages[i], SH1106_WIDTH, true);

        if(i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(1000)) != ESP_OK){
            i2c_cmd_link_delete(cmd);
            return 1;
        }

        i2c_cmd_link_delete(cmd);
        
        sh1106_send_command(SH1106_COMMAND_END);
    }
    return 0;
}

int sh1106_clear_buffer(SH1106_t* dev){
    for(int i = 0; i < SH1106_NUM_PAGES; i++){
        for(int j = 0; j < SH1106_WIDTH; j++){
            dev->pages[i][j] = 0;
        }
    }
    return 0;
}

//Typing Functios-----------------------------------------------------
int sh1106_set_cursor(SH1106_t* dev, int row, int col){
    if(!cursor_on_screen(row, col)){
        return 1;
    }
    dev->cursor_row = row;
    dev->cursor_col = col;
    return 0;
}

int sh1106_write_char(SH1106_t* dev, char c){
    if(c == '\n'){
        cursor_next_row(dev);
        dev->cursor_col = 0;
        return 0;
    }

    if((int)c > 0x7F){
        c = ' ';
    }

    int col_num = dev->cursor_col * 8;

    for(int i = 0; i < 8; i++){
        dev->pages[dev->cursor_row][col_num + i] = font8x8_basic_tr[(int)c][i];
    }

    if(cursor_next_col(dev) == 0){
        cursor_next_row(dev);
    }

    return 0; 
}

int sh1106_write_string(SH1106_t* dev, const char* string, int n){
    for(int i = 0; i < n; i++){
        if(sh1106_write_char(dev, string[i])){
            return 1; 
        }
    }
    return 0;
}

//Scrolling Functions-------------------------------------------------
int sh1106_set_display_start_line(int line_addr){
    if(line_addr > 0x3F){
        ESP_LOGE("sh1106_set_display_start_line", "line_addr out of valid range.");
        return 1;
    }
    sh1106_send_command(SH1106_COMMAND_SET_DISPLAY_START_LINE | line_addr);
    return 0;
}
