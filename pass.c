/* pass.c */
#include "common.h"
#include "pass.h"
#define P_BTN_W 120
#define P_BTN_H 62
#define P_GAP_X 24
#define P_GAP_Y 14
#define P_PAD_W (3*P_BTN_W+2*P_GAP_X)
#define P_PAD_X ((800-P_PAD_W)/2)
#define P_PAD_Y 148
#define IDLE_TIME 5

static char pass_hit(int tx,int ty)
{char k[4][3]={{'1','2','3'},{'4','5','6'},{'7','8','9'},{'D','0','O'}};
 for(int r=0;r<4;r++)for(int c=0;c<3;c++){int bx=P_PAD_X+c*(P_BTN_W+P_GAP_X),by=P_PAD_Y+r*(P_BTN_H+P_GAP_Y);
  if(tx>=bx&&tx<bx+P_BTN_W&&ty>=by&&ty<by+P_BTN_H)return k[r][c];}return 0;}
static void pass_btn(int bx,int by,char*lb,int bg,int fg,int sz)
{Clean_Area(bx,by,P_BTN_W,P_BTN_H,bg);int tw=(lb[0]>=0x80)?(strlen(lb)/2*sz*16):(strlen(lb)*sz*8);
 Display_characterX(bx+(P_BTN_W-tw)/2,by+(P_BTN_H-sz*16)/2,lb,fg,sz);}
static void pass_keypad(void)
{char*k[4][3]={{"1","2","3"},{"4","5","6"},{"7","8","9"},{"DEL","0","OK"}};
 for(int r=0;r<4;r++)for(int c=0;c<3;c++){int bx=P_PAD_X+c*(P_BTN_W+P_GAP_X),by=P_PAD_Y+r*(P_BTN_H+P_GAP_Y);
  int bg=BTN_GRAY,fg=TEXT_WHITE;if(r==3&&c==0){bg=BTN_ACCENT;fg=RED_DEL;}if(r==3&&c==2){bg=GREEN_OK;fg=TEXT_WHITE;}pass_btn(bx,by,k[r][c],bg,fg,3);}}
static void pass_dots(int len,int st)
{Clean_Area(0,60,800,80,BG_DARK);if(len==0){Display_characterX(336,70,"\xCA\xE4\xC8\xEB\xC3\xDC\xC2\xEB",0x00888888,2);return;}
 int cl=(st==1)?GREEN_OK:((st==2)?RED_ERR:0x00FFFFFF);int gp=44,sx=(800-len*gp)/2;for(int i=0;i<len;i++)Clean_Area(sx+i*gp,85,22,22,cl);}
static void pass_ui(void)
{Clean_Area(0,0,800,480,BG_DARK);Display_characterX(336,18,"\xCA\xE4\xC8\xEB\xC3\xDC\xC2\xEB",TEXT_WHITE,2);pass_dots(0,0);pass_keypad();}

int pass_run(void)
{char pw[]="123456",in[20]={0};int len=0,cnt=0;char ch;int bx,by;
 pass_ui();g_last_touch=time(NULL);
 while(1){
  if(time(NULL)-g_last_touch>=IDLE_TIME)return 0;
  if(touch_get_tm()==-1)continue;
  g_last_touch=time(NULL);int dx=x2-x1,dy=y2-y1;
  if(abs(dy)>SWIPE_MIN&&abs(dy)>abs(dx)&&dy<0)return 0;
  ch=pass_hit(x2,y2);if(!ch)continue;
  // 高亮
  {char kk[4][3]={{'1','2','3'},{'4','5','6'},{'7','8','9'},{'D','0','O'}};
   for(int r=0;r<4;r++)for(int c=0;c<3;c++)if(kk[r][c]==ch){
    bx=P_PAD_X+c*(P_BTN_W+P_GAP_X);by=P_PAD_Y+r*(P_BTN_H+P_GAP_Y);
    Clean_Area(bx-2,by-2,P_BTN_W+4,P_BTN_H+4,TEXT_WHITE);usleep(100000);
    Clean_Area(bx-2,by-2,P_BTN_W+4,2,BG_DARK);Clean_Area(bx-2,by+P_BTN_H,P_BTN_W+4,2,BG_DARK);
    Clean_Area(bx-2,by,2,P_BTN_H,BG_DARK);Clean_Area(bx+P_BTN_W,by,2,P_BTN_H,BG_DARK);
    char*lb=(r==3&&c==0)?"DEL":((r==3&&c==2)?"OK":(char[]){ch,0});
    int bg=BTN_GRAY,fg=TEXT_WHITE;if(r==3&&c==0){bg=BTN_ACCENT;fg=RED_DEL;}if(r==3&&c==2){bg=GREEN_OK;fg=TEXT_WHITE;}
    pass_btn(bx,by,lb,bg,fg,3);}}
  if(ch=='D'){if(len>0){len--;in[len]=0;}pass_dots(len,0);}
  else if(ch=='O'){if(!len)continue;
   if(!strcmp(in,pw)){pass_dots(len,1);sleep(1);return 1;}
   else{cnt++;pass_dots(len,2);if(cnt==3){Clean_Area(0,0,800,480,BG_DARK);Display_characterX(250,180,"\xD2\xD1\xCB\xF8\xB6\xA8",RED_ERR,4);while(1)sleep(1);}
    sleep(1);memset(in,0,sizeof(in));len=0;pass_dots(0,0);}}
  else if(ch>='0'&&ch<='9'){if(len<12){in[len++]=ch;in[len]=0;pass_dots(len,0);}}}}
