/**
 * main.c — 状态机调度: TIME → PASS → ALBUM
 */
#include "common.h"
#include "time_bg.h"
#include "pass.h"
#include "album.h"
#include "music_player.h"

#define STATE_TIME  0
#define STATE_PASS  1
#define STATE_MAIN  2
#define STATE_MUSIC 3

int main(void)
{Init_Font(); // 字库常驻全程
 int state=STATE_TIME;
 int lcd_fd=open("/dev/fb0",O_RDWR);
 if(lcd_fd==-1){perror("open /dev/fb0");return 1;}
 int*lcd=(int*)mmap(NULL,800*480*4,PROT_READ|PROT_WRITE,MAP_SHARED,lcd_fd,0);
 if(lcd==(void*)-1){perror("mmap /dev/fb0");close(lcd_fd);return 1;}
 char*ps[PHOTO_COUNT]={"y1.bmp","y2.bmp","y3.bmp","y4.bmp","y5.bmp","y6.bmp"};
 printf("====== TIME-PASS-MAIN-MUSIC ======\n");

 while(1){
  if(state==STATE_TIME){if(time_bg_run())state=STATE_PASS;}
  else if(state==STATE_PASS){int r=pass_run();state=r?STATE_MAIN:STATE_TIME;}
  else if(state==STATE_MAIN){int r=album_run(lcd,ps);state=r?STATE_MUSIC:STATE_TIME;}
  else if(state==STATE_MUSIC){int r=music_run(lcd);state=r?STATE_MAIN:STATE_TIME;}
 }
 munmap(lcd,800*480*4);close(lcd_fd);UnInit_Font();return 0;}
