#ifndef _SLIDE_WINDOW_H_
#define _SLIDE_WINDOW_H_
#include "cmu_tcp.h"

/* 初始化一个滑窗 */
int slide_window_init(slide_window_t *win, cmu_socket_t *sock);

/* 发送数据 */
void slide_window_send(slide_window_t *win, cmu_socket_t *sock);

/* 收到数据 */
void slide_window_check_for_data(slide_window_t * win, cmu_socket_t *sock, int flags);

/* 滑动窗口激活 */
void slide_window_activate(slide_window_t *win, cmu_socket_t *sock);

// void set_timer(int sec, int usec, void (*handler)(int));

// void unset_timer();

/* 关闭滑窗 */
void slide_window_close(slide_window_t *win);

void slide_window_handle message(window_t* win, cmu_socket_t *socket, char* pkt);

#endif //_WINDOW_H_
