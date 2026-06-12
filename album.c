/* album.c — 主界面 + 相册网格 + 全屏详情 */
#include "common.h"
#include "album.h"

#define TOP_H    22
#define TITLE_Y  22
#define TITLE_H  42
#define ICON_MAIN 100
#define ICON_GAP  80
#define ICONS_TW  (3*ICON_MAIN+2*ICON_GAP)
#define ICON_X    ((800-ICONS_TW)/2)
#define ICON_Y    140
#define LABEL_Y   (ICON_Y+ICON_MAIN+12)
#define GRID_Y   64
#define GRID_H   (480-GRID_Y)
#define THUMB_W  236
#define THUMB_H  175
#define THUMB_F  3
#define INNER_TW (THUMB_W-2*THUMB_F)
#define INNER_TH (THUMB_H-2*THUMB_F)
#define GAP_X    25
#define GAP_Y    20
#define GRID_TOTW (3*THUMB_W+2*GAP_X)
#define GRID_TOTH (2*THUMB_H+GAP_Y)
#define GRID_X   ((800-GRID_TOTW)/2)
#define GRID_STY (GRID_Y+(GRID_H-GRID_TOTH)/2)
#define DETAIL_PHOTO_H 351
#define DETAIL_PHOTO_Y 64
#define DETAIL_BOT_Y   415
#define DETAIL_BOT_H   65
#define ICON_SZ  40
#define BOT_GAP   65
#define BOT_PAD   15
#define FRAME_B  5
#define FRAME_H  (DETAIL_PHOTO_H-10)
#define FRAME_W  (710*FRAME_H/480+2*FRAME_B)
#define FRAME_X  ((800-FRAME_W)/2)
#define FRAME_Y  (DETAIL_PHOTO_Y+(DETAIL_PHOTO_H-FRAME_H)/2)
#define INNER_X  (FRAME_X+FRAME_B)
#define INNER_Y  (FRAME_Y+FRAME_B)
#define INNER_W  (FRAME_W-2*FRAME_B)
#define INNER_H  (FRAME_H-2*FRAME_B)
#define SWIPE_MIN 40
#define STATE_MAIN   0
#define STATE_GRID   1
#define STATE_DETAIL 2

static int g_state, g_current;
static int *g_lcd;
static char **g_ps;

// ---- 主界面 ----
static void main_draw(void)
{Clean_Area(0,TOP_H,800,480-TOP_H,CLR_WHITE);
 draw_bmp(g_lcd,"./set.bmp",ICON_X,ICON_Y,ICON_MAIN,ICON_MAIN);
 draw_bmp(g_lcd,"./photo.bmp",ICON_X+ICON_MAIN+ICON_GAP,ICON_Y,ICON_MAIN,ICON_MAIN);
 draw_bmp(g_lcd,"./music.bmp",ICON_X+2*(ICON_MAIN+ICON_GAP),ICON_Y,ICON_MAIN,ICON_MAIN);
 Display_characterX(ICON_X+38,LABEL_Y,"Set",CLR_LABEL,1);
 Display_characterX(ICON_X+ICON_MAIN+ICON_GAP+30,LABEL_Y,"Photo",CLR_LABEL,1);
 Display_characterX(ICON_X+2*(ICON_MAIN+ICON_GAP)+28,LABEL_Y,"Music",CLR_LABEL,1);}
static int hit_main(int tx,int ty)
{if(ty>=ICON_Y&&ty<ICON_Y+ICON_MAIN)for(int i=0;i<3;i++)if(tx>=ICON_X+i*(ICON_MAIN+ICON_GAP)&&tx<ICON_X+i*(ICON_MAIN+ICON_GAP)+ICON_MAIN)return i;return-1;}

// ---- 网格 ----
static void draw_title(void)
{Clean_Area(0,TITLE_Y,800,TITLE_H,CLR_TTL_BG);Display_characterX((800-96)/2,TITLE_Y+5,"Photos",CLR_TTL,2);}
static void grid_draw(void)
{lcd_fill(g_lcd,0,GRID_Y,800,GRID_H,CLR_WHITE);
 for(int i=0;i<PHOTO_COUNT;i++){int c=i%3,r=i/3,bx=GRID_X+c*(THUMB_W+GAP_X),by=GRID_STY+r*(THUMB_H+GAP_Y);
  lcd_frame(g_lcd,bx,by,THUMB_W,THUMB_H,THUMB_F,CLR_THUMB_FRM);draw_bmp(g_lcd,g_ps[i],bx+THUMB_F,by+THUMB_F,INNER_TW,INNER_TH);}}
static int hit_grid(int tx,int ty)
{for(int i=0;i<PHOTO_COUNT;i++){int c=i%3,r=i/3,bx=GRID_X+c*(THUMB_W+GAP_X),by=GRID_STY+r*(THUMB_H+GAP_Y);
  if(tx>=bx&&tx<bx+THUMB_W&&ty>=by&&ty<by+THUMB_H)return i;}return-1;}

// ---- 详情 ----
static void detail_frame(void){lcd_fill(g_lcd,0,DETAIL_PHOTO_Y,800,DETAIL_PHOTO_H,CLR_WHITE);lcd_frame(g_lcd,FRAME_X,FRAME_Y,FRAME_W,FRAME_H,FRAME_B,CLR_FRM);}
static void detail_photo(void){draw_bmp(g_lcd,g_ps[g_current],INNER_X,INNER_Y,INNER_W,INNER_H);}
static void detail_arrows(void)
{int ay=DETAIL_PHOTO_Y+(DETAIL_PHOTO_H-ICON_SZ)/2,lx=(FRAME_X-ICON_SZ)/2,rx=FRAME_X+FRAME_W+(800-FRAME_X-FRAME_W-ICON_SZ)/2;
 draw_bmp(g_lcd,"./last.bmp",lx,ay,ICON_SZ,ICON_SZ);draw_bmp(g_lcd,"./next.bmp",rx,ay,ICON_SZ,ICON_SZ);}
static void detail_bottom(void)
{lcd_fill(g_lcd,0,DETAIL_BOT_Y,800,DETAIL_BOT_H,CLR_WHITE);int tw=2*ICON_SZ+BOT_GAP,sx=(800-tw)/2,sy=DETAIL_BOT_Y+(DETAIL_BOT_H-ICON_SZ)/2;
 draw_bmp(g_lcd,"./play.bmp",sx,sy,ICON_SZ,ICON_SZ);draw_bmp(g_lcd,"./stop.bmp",sx+ICON_SZ+BOT_GAP,sy,ICON_SZ,ICON_SZ);}

// ---- 触摸线程 ----
static void* album_touch(void *arg)
{while(1){touch_get();g_last_touch=time(NULL);int dx=x2-x1,dy=y2-y1;
 if(g_state==STATE_MAIN){int h=hit_main(x2,y2);if(h==1)g_cmd=1;else if(h==2)g_cmd=8;}
 else if(g_state==STATE_GRID){
  if(y1<TITLE_Y+TITLE_H&&x1<100&&dx>SWIPE_MIN&&abs(dx)>abs(dy)){g_cmd=5;continue;}
  int h=hit_grid(x2,y2);if(h>=0){g_current=h;g_cmd=2;}}
 else{
  // 标题栏右滑→回网格
  if(y1<TITLE_Y+TITLE_H&&x1<100&&dx>SWIPE_MIN&&abs(dx)>abs(dy)){g_cmd=4;continue;}
  if(abs(dx)>SWIPE_MIN&&abs(dx)>abs(dy)&&x1>=INNER_X&&x1<INNER_X+INNER_W&&x2>=INNER_X&&x2<INNER_X+INNER_W){g_cmd=(dx>0)?6:7;continue;}
  int ay=DETAIL_PHOTO_Y+(DETAIL_PHOTO_H-ICON_SZ)/2,lx=(FRAME_X-ICON_SZ)/2,rx=FRAME_X+FRAME_W+(800-FRAME_X-FRAME_W-ICON_SZ)/2;
  if(x2>=lx-BOT_PAD&&x2<lx+ICON_SZ+BOT_PAD&&y2>=ay-BOT_PAD&&y2<ay+ICON_SZ+BOT_PAD){g_cmd=6;continue;}
  if(x2>=rx-BOT_PAD&&x2<rx+ICON_SZ+BOT_PAD&&y2>=ay-BOT_PAD&&y2<ay+ICON_SZ+BOT_PAD){g_cmd=7;continue;}
  int tw=2*ICON_SZ+BOT_GAP,sx=(800-tw)/2,sy=DETAIL_BOT_Y+(DETAIL_BOT_H-ICON_SZ)/2;
  if(x2>=sx-BOT_PAD&&x2<sx+ICON_SZ+BOT_PAD&&y2>=sy-BOT_PAD&&y2<sy+ICON_SZ+BOT_PAD){g_playing=1;continue;}
  if(x2>=sx+ICON_SZ+BOT_GAP-BOT_PAD&&x2<sx+ICON_SZ+BOT_GAP+ICON_SZ+BOT_PAD&&y2>=sy-BOT_PAD&&y2<sy+ICON_SZ+BOT_PAD){g_playing=0;continue;}}}}

// ---- 主入口 ----
int album_run(int *lcd, char *photos[])
{g_lcd=lcd;g_ps=photos;g_state=STATE_MAIN;g_current=0;g_cmd=0;g_playing=0;g_last_touch=time(NULL);
 Clean_Area(0,0,800,480,CLR_WHITE);draw_top();main_draw();
 pthread_t th;pthread_create(&th,NULL,album_touch,NULL);
 int pt=0,t1=0;
 while(1){
  if(g_state==STATE_MAIN&&g_cmd==1){Clean_Area(0,TITLE_Y,800,480-TITLE_Y,CLR_WHITE);draw_title();grid_draw();g_state=STATE_GRID;}
  else if(g_state==STATE_MAIN&&g_cmd==8){pthread_cancel(th);return 1;}
  else if(g_state==STATE_GRID){
   if(g_cmd==2){Clean_Area(0,DETAIL_PHOTO_Y,800,480-DETAIL_PHOTO_Y,CLR_WHITE);detail_frame();detail_photo();detail_arrows();detail_bottom();g_state=STATE_DETAIL;}
   else if(g_cmd==5){Clean_Area(0,TOP_H,800,480-TOP_H,CLR_WHITE);main_draw();g_state=STATE_MAIN;}}
  else if(g_state==STATE_DETAIL){
   if(g_cmd==6||g_cmd==7){g_current=(g_cmd==7)?((g_current<PHOTO_COUNT-1)?g_current+1:0):((g_current>0)?g_current-1:PHOTO_COUNT-1);detail_photo();detail_arrows();}
   else if(g_cmd==4){Clean_Area(0,GRID_Y,800,GRID_H,CLR_WHITE);draw_title();grid_draw();g_state=STATE_GRID;}}
  g_cmd=0;
  if(g_state==STATE_DETAIL&&g_playing){pt++;if(pt>=PLAY_TICK){pt=0;g_current=(g_current<PHOTO_COUNT-1)?g_current+1:0;detail_photo();detail_arrows();}}else pt=0;
  if(!g_playing&&time(NULL)-g_last_touch>=IDLE_TIME){pthread_cancel(th);return 0;}
  t1++;if(t1>=20){t1=0;draw_top();}usleep(50000);}
 pthread_cancel(th);return 0;}
