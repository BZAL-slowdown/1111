/* time_bg.c */
#include "common.h"
#include "time_bg.h"
static int g_clean[800*480];

static void bg_init(void)
{static unsigned char raw[800*480*3];int w,h,x,y;
 if(read_bmp("./background1.bmp",raw,&w,&h)==-1){w=800;h=480;memset(raw,0,sizeof(raw));}
 for(y=0;y<480;y++)for(x=0;x<800;x++){int si=((y*h/480)*w+(x*w/800))*3;
  g_clean[y*800+x]=raw[si]|(raw[si+1]<<8)|(raw[si+2]<<16);}}

static void text_update(int *l)
{for(int y=50;y<98;y++)memcpy(l+y*800,g_clean+y*800,800*4);
 for(int y=130;y<230;y++)memcpy(l+y*800,g_clean+y*800,800*4);
 time_t n=time(NULL);struct tm*t=localtime(&n);char d[32],m[32];
 sprintf(d,"%04d %02d %02d",t->tm_year+1900,t->tm_mon+1,t->tm_mday);
 sprintf(m,"%02d %02d %02d",t->tm_hour,t->tm_min,t->tm_sec);
 Display_characterX((800-10*16)/2,65,d,0x00333333,2);
 Display_characterX((800-8*40)/2,140,m,0x00111111,5);}

int time_bg_run(void)
{int lcd_fd=open("/dev/fb0",O_RDWR);if(lcd_fd==-1)return 0;
 int*l=(int*)mmap(NULL,800*480*4,PROT_READ|PROT_WRITE,MAP_SHARED,lcd_fd,0);
 bg_init();memcpy(l,g_clean,800*480*4);text_update(l);
 int fd=open("/dev/input/event0",O_RDWR);if(fd==-1){munmap(l,800*480*4);close(lcd_fd);return 0;}
 int ls=-1,td=0,tx1=0,ty1=0,tx2=0,ty2=0,has_x=0,has_y=0;struct input_event ev;
 while(1){fd_set fds;struct timeval tv={1,0};FD_ZERO(&fds);FD_SET(fd,&fds);
  int sr=select(fd+1,&fds,NULL,NULL,&tv);
  if(sr==0){time_t n=time(NULL);struct tm*t=localtime(&n);if(t->tm_sec!=ls){ls=t->tm_sec;text_update(l);}continue;}
  if(sr<0)continue;
  if(read(fd,&ev,sizeof(ev))!=sizeof(ev))continue;
  if(ev.type==EV_ABS){
   if(ev.code==ABS_X||ev.code==ABS_MT_POSITION_X){touch_x=ev.value*800/1024;has_x=1;}
   if(ev.code==ABS_Y||ev.code==ABS_MT_POSITION_Y){touch_y=ev.value*480/600;has_y=1;}
   if(has_x&&has_y){
    if(!td){td=1;tx1=touch_x;ty1=touch_y;}
    tx2=touch_x;ty2=touch_y;
    int dx=tx2-tx1,dy=ty2-ty1;
    if(abs(dy)>SWIPE_THRESH&&abs(dy)>abs(dx)){
     printf("time swipe move: (%d,%d)->(%d,%d), dx=%d dy=%d\n",tx1,ty1,tx2,ty2,dx,dy);
     close(fd);munmap(l,800*480*4);close(lcd_fd);return 1;
    }}}
  if(ev.type==EV_KEY&&ev.code==BTN_TOUCH&&ev.value==1){
   td=0;has_x=0;has_y=0;
  }
  if(ev.type==EV_KEY&&ev.code==BTN_TOUCH&&ev.value==0){
   tx2=touch_x;ty2=touch_y;int dx=tx2-tx1,dy=ty2-ty1;
   printf("time swipe release: (%d,%d)->(%d,%d), dx=%d dy=%d\n",tx1,ty1,tx2,ty2,dx,dy);
   close(fd);munmap(l,800*480*4);close(lcd_fd);
   return(abs(dy)>SWIPE_THRESH&&abs(dy)>abs(dx))?1:0;
  }}}
