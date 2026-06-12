/* common.c — 共享实现 */
#include "common.h"
int touch_x, touch_y, x1, y1, x2, y2;
int g_cmd = 0, g_playing = 0;
time_t g_last_touch;

void lcd_fill(int *l, int x, int y, int w, int h, int c)
{for(int i=y;i<y+h;i++)for(int j=x;j<x+w;j++)l[i*800+j]=c;}
void lcd_frame(int *l, int x, int y, int w, int h, int t, int c)
{lcd_fill(l,x,y,w,t,c);lcd_fill(l,x,y+h-t,w,t,c);lcd_fill(l,x,y,t,h,c);lcd_fill(l,x+w-t,y,t,h,c);}
int read_bmp(const char *p, unsigned char *b, int *ow, int *oh)
{int fd=open(p,O_RDONLY);if(fd==-1)return-1;short bit=0;
 lseek(fd,18,SEEK_SET);if(read(fd,ow,4)!=4||read(fd,oh,4)!=4){close(fd);return-1;}
 lseek(fd,28,SEEK_SET);read(fd,&bit,2);
 if(*ow<=0||*oh<=0||*ow>800||*oh>480||bit!=24){printf("skip bmp: %s %dx%d %dbit\n",p,*ow,*oh,bit);close(fd);return-1;}
 lseek(fd,54,SEEK_SET);
 int rb=*ow*3,pad=(4-rb%4)%4;unsigned char r[2404];for(int i=*oh-1;i>=0;i--){if(read(fd,r,rb+pad)!=rb+pad)break;memcpy(b+i*rb,r,rb);}close(fd);return 0;}
void draw_scaled(unsigned char *b, int sw, int sh, int *l, int dx, int dy, int dw, int dh)
{for(int y=0;y<dh;y++)for(int x=0;x<dw;x++){int si=((y*sh/dh)*sw+(x*sw/dw))*3;l[(dy+y)*800+(dx+x)]=b[si]|(b[si+1]<<8)|(b[si+2]<<16);}}
void draw_bmp(int *l, const char *p, int dx, int dy, int dw, int dh)
{int sw,sh;static unsigned char b[800*480*3];if(read_bmp(p,b,&sw,&sh)!=-1)draw_scaled(b,sw,sh,l,dx,dy,dw,dh);}
void draw_top(void)
{Clean_Area(0,0,800,22,CLR_BAR);time_t n=time(NULL);struct tm*t=localtime(&n);
 char s[32];sprintf(s,"%04d-%02d-%02d  %02d:%02d:%02d",t->tm_year+1900,t->tm_mon+1,t->tm_mday,t->tm_hour,t->tm_min,t->tm_sec);
 Display_characterX(14,3,s,CLR_TIME,1);}
int touch_get(void)
{struct input_event ev;int fd=open("/dev/input/event0",O_RDWR);if(fd==-1)return-1;
 while(1){read(fd,&ev,sizeof(ev));
  if(ev.type==EV_ABS){if(ev.code==ABS_X)touch_x=ev.value*800/1024;if(ev.code==ABS_Y)touch_y=ev.value*480/600;}
  if(ev.type==EV_KEY&&ev.code==BTN_TOUCH&&ev.value==1){x1=touch_x;y1=touch_y;}
  if(ev.type==EV_KEY&&ev.code==BTN_TOUCH&&ev.value==0){x2=touch_x;y2=touch_y;close(fd);return 0;}}}
int touch_get_tm(void)
{struct input_event ev;int fd=open("/dev/input/event0",O_RDWR);if(fd==-1)return-1;
 while(1){fd_set fds;struct timeval tv={1,0};FD_ZERO(&fds);FD_SET(fd,&fds);
  if(select(fd+1,&fds,NULL,NULL,&tv)<=0){close(fd);return-1;}
  read(fd,&ev,sizeof(ev));
  if(ev.type==EV_ABS){if(ev.code==ABS_X)touch_x=ev.value*800/1024;if(ev.code==ABS_Y)touch_y=ev.value*480/600;}
  if(ev.type==EV_KEY&&ev.code==BTN_TOUCH&&ev.value==1){x1=touch_x;y1=touch_y;}
  if(ev.type==EV_KEY&&ev.code==BTN_TOUCH&&ev.value==0){x2=touch_x;y2=touch_y;close(fd);return 0;}}}
