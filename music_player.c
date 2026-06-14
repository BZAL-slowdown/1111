/* music_player.c */
#include "common.h"
#include "music_player.h"
#include <dirent.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/time.h>

#define MUSIC_DIR "/nine/mp3"
#define COVER_DIR "/nine/photo"
#define BG1 "/nine/photo/music_bg.bmp"
#define BG2 "/nine/photo/b1.bmp"
#define LRC_DIR "/nine/lrc"
#define SONG_COUNT 5
#define IDLE_TIME 5
#define LRC_MAX 80
#define LRC_ADJUST_MS 0

#define PANEL 0x003b5f92
#define PANEL2 0x004d6fa8
#define LINE 0x00b9d4ff
#define PINK 0x00ff5f8d
#define MUTED 0x00b7c2df
#define SOFT 0x00eef6ff
#define LRC_RED 0x00ff3b45

#define PREV_CX 224
#define PLAY_CX 314
#define NEXT_CX 404
#define CTRL_CY 390
#define SWIPE_MIN 40

static char g_song_paths[SONG_COUNT][128];
static char g_lrc_text[LRC_MAX][96];
static int g_lrc_time[LRC_MAX];
static int g_song_total;
static int g_cur, m_playing, g_paused, g_repeat, g_random, g_volume = 68, g_spin, g_lrc_count, g_lrc_idx = -1;
static int g_duration = 240, g_elapsed, g_elapsed_ms, g_bg[800 * 480], g_frame[800 * 480];
static long g_start_ms;

static int clamp(int v, int a, int b){return v<a?a:(v>b?b:v);}
static int hit(int x,int y,int rx,int ry,int rw,int rh){return x>=rx&&x<rx+rw&&y>=ry&&y<ry+rh;}

static long now_ms(void)
{struct timeval tv;gettimeofday(&tv,NULL);return tv.tv_sec*1000L+tv.tv_usec/1000;}

static const char* song_name(void)
{const char *s=strrchr(g_song_paths[g_cur],'/');return s?s+1:g_song_paths[g_cur];}

static void song_base(char *out,int n)
{const char *s=song_name();int i=0;while(s[i]&&s[i]!='.'&&i<n-1){out[i]=s[i];i++;}out[i]=0;}

static void trim_line(char *s)
{int n=strlen(s);while(n>0&&(s[n-1]=='\n'||s[n-1]=='\r'||s[n-1]==' ')){s[--n]=0;}}

static void fit_text(char *s,int max)
{int i=0;while(s[i]&&i<max){if((unsigned char)s[i]&0x80){if(i+1>=max||!s[i+1])break;i+=2;}else i++;}s[i]=0;}

static int parse_lrc_time(const char *s,int *ms)
{int m=0,ss=0,frac=0,scale=100;const char *dot;
 if(sscanf(s,"[%d:%d",&m,&ss)!=2)return 0;
 dot=strchr(s,'.');
 if(dot){frac=atoi(dot+1);if(frac>=100)scale=1;else if(frac>=10)scale=10;}
 *ms=(m*60+ss)*1000+frac*scale;return 1;}

static void load_lrc(void)
{char path[64],line[160],base[48];FILE *fp;g_lrc_count=0;g_lrc_idx=-1;
 song_base(base,sizeof(base));snprintf(path,sizeof(path),"%s/%s.lrc",LRC_DIR,base);fp=fopen(path,"r");
 if(!fp){snprintf(path,sizeof(path),"%s/%d.lrc",LRC_DIR,g_cur+1);fp=fopen(path,"r");}
 if(!fp){printf("lrc not found for: %s\n",song_name());return;}
 while(fgets(line,sizeof(line),fp)&&g_lrc_count<LRC_MAX){
  int ms;char *p=strchr(line,']');trim_line(line);
  if(!p||!parse_lrc_time(line,&ms)||!p[1])continue;
  g_lrc_time[g_lrc_count]=clamp(ms+LRC_ADJUST_MS,0,3600000);
  strncpy(g_lrc_text[g_lrc_count],p+1,sizeof(g_lrc_text[0])-1);
  g_lrc_text[g_lrc_count][sizeof(g_lrc_text[0])-1]=0;
  fit_text(g_lrc_text[g_lrc_count],22);
  g_lrc_count++;
 }
 fclose(fp);printf("lrc loaded: %s lines=%d\n",path,g_lrc_count);}

static int lrc_current_index(void)
{int idx=-1;for(int i=0;i<g_lrc_count;i++){if(g_elapsed_ms>=g_lrc_time[i])idx=i;else break;}return idx;}

static void draw_lrc_line(int x,int y,const char *text,int color,int size,int max)
{char tmp[96];strncpy(tmp,text,sizeof(tmp)-1);tmp[sizeof(tmp)-1]=0;fit_text(tmp,max);
 Display_characterX(x,y,(unsigned char*)tmp,color,size);}

static int touch_poll_music(int fd,int ms)
{struct input_event ev;
 while(1){fd_set fds;struct timeval tv={ms/1000,(ms%1000)*1000};FD_ZERO(&fds);FD_SET(fd,&fds);
  if(select(fd+1,&fds,NULL,NULL,&tv)<=0)return-1;
  read(fd,&ev,sizeof(ev));
  if(ev.type==EV_ABS){if(ev.code==ABS_X)touch_x=ev.value*800/1024;if(ev.code==ABS_Y)touch_y=ev.value*480/600;}
  if(ev.type==EV_KEY&&ev.code==BTN_TOUCH&&ev.value==1){x1=touch_x;y1=touch_y;}
  if(ev.type==EV_KEY&&ev.code==BTN_TOUCH&&ev.value==0){x2=touch_x;y2=touch_y;return 0;}}}

static void copy_bg(int x,int y,int w,int h)
{for(int yy=y;yy<y+h;yy++)memcpy(g_frame+yy*800+x,g_bg+yy*800+x,w*4);}

static void blit(int *lcd,int x,int y,int w,int h)
{for(int yy=y;yy<y+h;yy++)memcpy(lcd+yy*800+x,g_frame+yy*800+x,w*4);}

static int blend(int bg,int fg,int a)
{int br=(bg>>16)&255,bg2=(bg>>8)&255,bb=bg&255,fr=(fg>>16)&255,fg2=(fg>>8)&255,fb=fg&255;
 int r=(br*(255-a)+fr*a)/255,g=(bg2*(255-a)+fg2*a)/255,b=(bb*(255-a)+fb*a)/255;return(r<<16)|(g<<8)|b;}

static void glass(int*l,int x,int y,int w,int h)
{for(int yy=y;yy<y+h;yy++)for(int xx=x;xx<x+w;xx++){int*p=l+yy*800+xx;*p=blend(*p,0x00ffffff,32);*p=blend(*p,0x002a4f88,24);}
 lcd_frame(l,x,y,w,h,1,LINE);}

static int is_mp3(const char*n)
{const char*d=strrchr(n,'.');return d&&tolower(d[1])=='m'&&tolower(d[2])=='p'&&tolower(d[3])=='3'&&d[4]==0;}

static void scan_songs(void)
{DIR*d=opendir(MUSIC_DIR);struct dirent*e;g_song_total=0;
 if(d){while((e=readdir(d))&&g_song_total<SONG_COUNT){if(e->d_name[0]=='.'||!is_mp3(e->d_name))continue;
   snprintf(g_song_paths[g_song_total],sizeof(g_song_paths[0]),"%s/%s",MUSIC_DIR,e->d_name);
   printf("song %d: %s\n",g_song_total+1,g_song_paths[g_song_total]);g_song_total++;}closedir(d);}
 if(!g_song_total){for(int i=0;i<SONG_COUNT;i++)snprintf(g_song_paths[i],sizeof(g_song_paths[0]),"%s/%d.mp3",MUSIC_DIR,i+1);g_song_total=SONG_COUNT;}}

static long fsize(const char*p){struct stat st;return stat(p,&st)==-1?0:(long)st.st_size;}
static int duration_of(const char*p){long s=fsize(p);return s<=0?240:clamp((int)(s*8/128000),45,600);}

static void stop_music(void){system("killall -SIGKILL madplay >/dev/null 2>&1");}
static int madplay_on(void){return system("pgrep madplay >/dev/null 2>&1")==0;}

static void set_vol(int v)
{char cmd[96];const char*ns[]={"Master","PCM","Speaker","Headphone","Playback"};g_volume=clamp(v,0,100);
 for(int i=0;i<5;i++){snprintf(cmd,sizeof(cmd),"amixer set %s %d%% unmute >/dev/null 2>&1",ns[i],g_volume);system(cmd);}}

static void start_song(int idx)
{char cmd[220];idx=clamp(idx,0,g_song_total-1);g_cur=idx;g_elapsed=0;g_elapsed_ms=0;g_duration=duration_of(g_song_paths[idx]);
 load_lrc();
 if(access(g_song_paths[idx],R_OK)!=0){printf("music not found: %s\n",g_song_paths[idx]);m_playing=0;g_paused=0;return;}
 stop_music();usleep(100000);snprintf(cmd,sizeof(cmd),"madplay \"%s\" >/tmp/music_player_madplay.log 2>&1 &",g_song_paths[idx]);system(cmd);usleep(200000);
 m_playing=madplay_on();g_paused=0;g_start_ms=now_ms();if(!m_playing)printf("madplay start failed, see /tmp/music_player_madplay.log\n");}

static void toggle_play(void)
{if(!m_playing)start_song(g_cur);else if(g_paused){system("killall -SIGCONT madplay >/dev/null 2>&1");g_paused=0;g_start_ms=now_ms()-g_elapsed_ms;}
 else{system("killall -SIGSTOP madplay >/dev/null 2>&1");g_elapsed_ms=now_ms()-g_start_ms;g_elapsed=g_elapsed_ms/1000;g_paused=1;}}

static void next_song(void){start_song(g_random?(rand()%g_song_total):((g_cur+1)%g_song_total));}
static void prev_song(void){start_song(g_cur?g_cur-1:g_song_total-1);}

static void draw_tri_r(int*l,int x,int y,int w,int h,int c)
{for(int yy=0;yy<h;yy++){int len=yy<h/2?yy*w/h*2:(h-yy)*w/h*2;for(int xx=0;xx<len;xx++)if(x+xx>=0&&x+xx<800&&y+yy>=0&&y+yy<480)l[(y+yy)*800+x+xx]=c;}}
static void draw_tri_l(int*l,int x,int y,int w,int h,int c)
{for(int yy=0;yy<h;yy++){int len=yy<h/2?yy*w/h*2:(h-yy)*w/h*2;for(int xx=0;xx<len;xx++)if(x+w-xx>=0&&x+w-xx<800&&y+yy>=0&&y+yy<480)l[(y+yy)*800+x+w-xx]=c;}}
static void circle(int*l,int cx,int cy,int r,int c)
{for(int y=-r;y<=r;y++)for(int x=-r;x<=r;x++)if(x*x+y*y<=r*r&&cx+x>=0&&cx+x<800&&cy+y>=0&&cy+y<480)l[(cy+y)*800+cx+x]=c;}

static void draw_bg(int*lcd)
{lcd_fill(lcd,0,0,800,480,0x002a4f88);draw_bmp(lcd,BG2,0,0,800,480);draw_bmp(lcd,BG1,0,0,800,480);memcpy(g_bg,lcd,800*480*4);}

static void spin_xy(int x,int y,int k,int*sx,int*sy)
{static int c[16]={256,237,181,98,0,-98,-181,-237,-256,-237,-181,-98,0,98,181,237};
 static int s[16]={0,98,181,237,256,237,181,98,0,-98,-181,-237,-256,-237,-181,-98};
 k&=15;*sx=(x*c[k]+y*s[k])/256;*sy=(-x*s[k]+y*c[k])/256;}

static void draw_cover(int*lcd)
{char p[64];static unsigned char b[800*480*3];static int sw,sh,last=-1,ok=0;int cx=157,cy=205,r=93;
 snprintf(p,sizeof(p),"%s/c%d.bmp",COVER_DIR,g_cur+1);
 if(last!=g_cur){ok=(read_bmp(p,b,&sw,&sh)!=-1);last=g_cur;}
 circle(lcd,cx,cy,r+3,0x00eef6ff);
 if(ok){
  for(int yy=-r;yy<=r;yy++)for(int xx=-r;xx<=r;xx++)if(xx*xx+yy*yy<=r*r){
   int rx,ry;spin_xy(xx,yy,(m_playing&&!g_paused)?g_spin:0,&rx,&ry);
   if(rx*rx+ry*ry<=r*r){int sx=clamp((rx+r)*sw/(2*r+1),0,sw-1),sy=clamp((ry+r)*sh/(2*r+1),0,sh-1),si=(sy*sw+sx)*3;
    lcd[(cy+yy)*800+(cx+xx)]=b[si]|(b[si+1]<<8)|(b[si+2]<<16);}
  }
 }else circle(lcd,cx,cy,r,0x004d6fa8);
 circle(lcd,cx,cy,22,0x00edf4ff);circle(lcd,cx,cy,9,0x003b5f92);
}

static void draw_info_gfx(int*lcd)
{glass(lcd,276,100,224,216);}

static void draw_info_text(void)
{char s[64];
 sprintf(s,"\xB8\xE8\xC7\xFA %d",g_cur+1);Display_characterX(284,116,s,CLR_WHITE,2);
 Display_characterX(284,170,m_playing?(g_paused?"\xD4\xDD\xCD\xA3":"\xB2\xA5\xB7\xC5\xD6\xD0"):"\xD7\xBC\xB1\xB8",m_playing&&!g_paused?PINK:MUTED,1);
 Display_characterX(284,206,(unsigned char*)song_name(),CLR_WHITE,1);
 if(g_lrc_count>0){
  int idx=lrc_current_index();
  if(idx>0)draw_lrc_line(284,228,g_lrc_text[idx-1],CLR_WHITE,1,22);
  if(idx>=0)draw_lrc_line(284,248,g_lrc_text[idx],LRC_RED,2,12);
  else draw_lrc_line(284,248,"\xB5\xC8\xB4\xFD\xB8\xE8\xB4\xCA",LRC_RED,2,12);
  if(idx+1<g_lrc_count)draw_lrc_line(284,286,g_lrc_text[idx+1],CLR_WHITE,1,22);
 }else{
  Display_characterX(284,238,g_random?"\xCB\xE6\xBB\xFA\xB2\xA5\xB7\xC5":"\xCB\xB3\xD0\xF2\xB2\xA5\xB7\xC5",MUTED,1);
  Display_characterX(284,266,g_repeat?"\xB5\xA5\xC7\xFA\xD1\xAD\xBB\xB7":"\xD7\xD4\xB6\xAF\xCF\xC2\xD2\xBB\xCA\xD7",MUTED,1);
 }}

static void draw_list_gfx(int*lcd)
{glass(lcd,516,96,248,240);
 for(int i=0;i<g_song_total;i++){int y=154+i*34;if(i==g_cur){for(int yy=y-8;yy<y+20;yy++)for(int xx=530;xx<748;xx++){int*p=lcd+yy*800+xx;*p=blend(*p,0x00ffffff,70);}lcd_fill(lcd,534,y-7,4,26,PINK);}
 }}

static void draw_list_text(void)
{Display_characterX(548,114,"\xB2\xA5\xB7\xC5\xC1\xD0\xB1\xED",CLR_WHITE,1);
 for(int i=0;i<g_song_total;i++){int y=154+i*34;char s[32];sprintf(s,"SONG %d",i+1);
  Display_characterX(542,y,s,i==g_cur?PINK:CLR_WHITE,1);Display_characterX(704,y,"MP3",MUTED,1);}}

static void draw_progress_gfx(int*lcd)
{if(m_playing&&!g_paused){g_elapsed_ms=now_ms()-g_start_ms;g_elapsed=g_elapsed_ms/1000;}g_elapsed=clamp(g_elapsed,0,g_duration);g_elapsed_ms=clamp(g_elapsed_ms,0,g_duration*1000);
 glass(lcd,52,304,448,36);char a[16],b[16];sprintf(a,"%02d:%02d",g_elapsed/60,g_elapsed%60);sprintf(b,"%02d:%02d",g_duration/60,g_duration%60);
 int x=118,w=300,fill=g_elapsed*w/(g_duration?g_duration:240);lcd_fill(lcd,x,321,w,5,SOFT);lcd_fill(lcd,x,321,fill,5,PINK);circle(lcd,x+fill,323,6,SOFT);}

static void draw_progress_text(void)
{char a[16],b[16];sprintf(a,"%02d:%02d",g_elapsed/60,g_elapsed%60);sprintf(b,"%02d:%02d",g_duration/60,g_duration%60);
 Display_characterX(58,312,a,CLR_WHITE,1);Display_characterX(442,312,b,CLR_WHITE,1);}

static void draw_ctrl_gfx(int*lcd)
{glass(lcd,38,348,452,82);lcd_frame(lcd,54,356,128,28,2,g_repeat?PINK:LINE);lcd_frame(lcd,54,388,128,28,2,g_random?PINK:LINE);
 circle(lcd,PREV_CX,CTRL_CY,30,0x006782bd);circle(lcd,PLAY_CX,CTRL_CY,42,PINK);circle(lcd,NEXT_CX,CTRL_CY,30,0x006782bd);
 lcd_fill(lcd,PREV_CX-19,CTRL_CY-14,5,28,CLR_WHITE);draw_tri_l(lcd,PREV_CX-7,CTRL_CY-14,17,28,CLR_WHITE);
 if(m_playing&&!g_paused){lcd_fill(lcd,PLAY_CX-14,CTRL_CY-24,10,48,CLR_WHITE);lcd_fill(lcd,PLAY_CX+10,CTRL_CY-24,10,48,CLR_WHITE);}else draw_tri_r(lcd,PLAY_CX-14,CTRL_CY-28,40,54,CLR_WHITE);
 draw_tri_r(lcd,NEXT_CX-10,CTRL_CY-14,17,28,CLR_WHITE);lcd_fill(lcd,NEXT_CX+14,CTRL_CY-14,5,28,CLR_WHITE);
 lcd_fill(lcd,548,376,174,5,SOFT);lcd_fill(lcd,548,376,g_volume*174/100,5,PINK);circle(lcd,548+g_volume*174/100,378,6,SOFT);}

static void draw_ctrl_text(void)
{Display_characterX(78,363,"REPEAT",CLR_WHITE,1);Display_characterX(78,395,"RANDOM",CLR_WHITE,1);
 Display_characterX(505,366,"\xD2\xF4\xC1\xBF",CLR_WHITE,1);}

static void draw_all(int*lcd)
{draw_bg(g_frame);for(int y=0;y<72;y++)for(int x=0;x<800;x++)g_frame[y*800+x]=blend(g_frame[y*800+x],0x002c4388,135);
 memcpy(g_bg,g_frame,800*480*4);draw_cover(g_frame);draw_info_gfx(g_frame);draw_list_gfx(g_frame);draw_progress_gfx(g_frame);draw_ctrl_gfx(g_frame);
 memcpy(lcd,g_frame,800*480*4);draw_top();Display_characterX(92,32,"music",CLR_WHITE,2);
 draw_info_text();draw_list_text();draw_progress_text();draw_ctrl_text();}

static void refresh_body(int *lcd)
{copy_bg(38,96,726,334);draw_cover(g_frame);draw_info_gfx(g_frame);draw_list_gfx(g_frame);draw_progress_gfx(g_frame);draw_ctrl_gfx(g_frame);
 blit(lcd,38,96,726,334);draw_info_text();draw_list_text();draw_progress_text();draw_ctrl_text();}

static void refresh_info_ctrl(int *lcd)
{copy_bg(276,100,224,216);copy_bg(38,348,452,82);draw_info_gfx(g_frame);draw_ctrl_gfx(g_frame);
 blit(lcd,276,100,224,216);blit(lcd,38,348,452,82);draw_info_text();draw_ctrl_text();}

static void refresh_info(int *lcd)
{copy_bg(276,100,224,216);draw_info_gfx(g_frame);blit(lcd,276,100,224,216);draw_info_text();}

static void refresh_ctrl(int *lcd)
{copy_bg(38,348,712,82);draw_ctrl_gfx(g_frame);blit(lcd,38,348,712,82);draw_ctrl_text();}

static void refresh_progress(int *lcd)
{copy_bg(52,304,448,36);draw_progress_gfx(g_frame);blit(lcd,52,304,448,36);draw_progress_text();}

static void refresh_cover_progress(int *lcd)
{copy_bg(52,100,214,216);copy_bg(52,304,448,36);draw_cover(g_frame);draw_progress_gfx(g_frame);
 blit(lcd,52,100,214,216);blit(lcd,52,304,448,36);draw_progress_text();}

int music_run(int *lcd)
{scan_songs();srand(time(NULL));g_cur=0;m_playing=0;g_paused=0;g_repeat=0;g_random=0;g_elapsed=0;g_elapsed_ms=0;g_spin=0;load_lrc();set_vol(g_volume);draw_all(lcd);g_last_touch=time(NULL);
 int touch_fd=open("/dev/input/event0",O_RDWR);
 if(touch_fd==-1){perror("open touch");return 0;}
 int tick=0;
 while(1){if(m_playing&&!g_paused&&!madplay_on()){if(g_repeat)start_song(g_cur);else next_song();refresh_body(lcd);}
  if(touch_poll_music(touch_fd,50)==-1){tick++;
   if(m_playing&&!g_paused&&tick%4==0){g_spin++;copy_bg(52,100,214,216);draw_cover(g_frame);blit(lcd,52,100,214,216);}
   if(tick%4==0){int li;if(m_playing&&!g_paused){g_elapsed_ms=now_ms()-g_start_ms;g_elapsed=g_elapsed_ms/1000;}li=lrc_current_index();if(li!=g_lrc_idx){g_lrc_idx=li;refresh_info(lcd);}}
   if(tick%20==0){refresh_progress(lcd);draw_top();}
   if((!m_playing||g_paused)&&time(NULL)-g_last_touch>=IDLE_TIME){close(touch_fd);stop_music();return 0;}continue;}
  tick=0;
  g_last_touch=time(NULL);int x=x2,y=y2;
  int dx=x2-x1,dy=y2-y1;
  if((y1<64&&x1<100&&dx>SWIPE_MIN&&abs(dx)>abs(dy))||(x<90&&y<80)){close(touch_fd);stop_music();return 1;}
  if(hit(x,y,548,358,180,42)){set_vol((x-548)*100/174);refresh_ctrl(lcd);continue;}
  if(hit(x,y,PREV_CX-34,CTRL_CY-34,68,68)){prev_song();refresh_body(lcd);continue;}
  if(hit(x,y,PLAY_CX-46,CTRL_CY-46,92,92)){toggle_play();refresh_info_ctrl(lcd);continue;}
  if(hit(x,y,NEXT_CX-34,CTRL_CY-34,68,68)){next_song();refresh_body(lcd);continue;}
  if(hit(x,y,54,356,128,28)){g_repeat=!g_repeat;refresh_info_ctrl(lcd);continue;}
  if(hit(x,y,54,388,128,28)){g_random=!g_random;refresh_info_ctrl(lcd);continue;}
  if(hit(x,y,530,145,218,184)){int idx=(y-148)/34;if(idx>=0&&idx<g_song_total){start_song(idx);refresh_body(lcd);}}}
}
