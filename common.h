/* common.h — 共享定义 */
#ifndef COMMON_H
#define COMMON_H
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include <linux/input.h>
#include <pthread.h>
#include <time.h>
#include "font.h"

// 颜色
#define CLR_WHITE  0x00FFFFFF
#define CLR_BLACK  0x00000000
#define CLR_BAR    0x00F0F0F0
#define CLR_TIME   0x00222222
#define CLR_TTL_BG 0x00EBEEF2
#define CLR_TTL    0x00000000
#define CLR_FRM    0x00D9D9D9
#define CLR_THUMB_FRM 0x00CCCCCC
#define CLR_LABEL  0x00888888
#define BG_DARK    0x001C1C1E
#define BTN_GRAY   0x00333335
#define BTN_ACCENT 0x00636366
#define TEXT_WHITE 0x00FFFFFF
#define GREEN_OK   0x0034C759
#define RED_DEL    0x00FF453A
#define RED_ERR    0x00FF453A

// 触摸全局
extern int touch_x, touch_y, x1, y1, x2, y2;
extern int g_cmd, g_playing;
extern time_t g_last_touch;

// LCD 函数
void lcd_fill(int *l, int x, int y, int w, int h, int c);
void lcd_frame(int *l, int x, int y, int w, int h, int t, int c);
int  read_bmp(const char *p, unsigned char *b, int *ow, int *oh);
void draw_scaled(unsigned char *b, int sw, int sh, int *l, int dx, int dy, int dw, int dh);
void draw_bmp(int *l, const char *p, int dx, int dy, int dw, int dh);
void draw_top(void); // 顶部时间栏
int  touch_get(void);
int  touch_get_tm(void); // 1s超时版
#endif
