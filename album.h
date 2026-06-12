/* album.h — 相册(主界面+网格+详情) */
#ifndef ALBUM_H
#define ALBUM_H
#define PHOTO_COUNT 6
#define PLAY_TICK  40
#define IDLE_TIME  5
int album_run(int *lcd, char *photos[]); // 返回0=退出到TIME
#endif
