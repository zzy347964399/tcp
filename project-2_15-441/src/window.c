#include "window.h"
#include "cmu_tcp.h"
#include "cmu_packet.h"
#include <stdio.h>
#include <signal.h>
#include <unistd.h>


/* 初始化一个滑窗 */
// 窗口大小直接设置默认值了
int slide_window_init(slide_window_t *win, cmu_socket_t *sock){ 
    /* 握手和挥手的时候会处理好 */
    // win->last_ack_received = last_ack_received;
    // win->last_seq_received = last_seq_received;
    win->adv_window = WINDOW_INITIAL_WINDOW_SIZE;
    win->seq_expect = win->last_seq_received;
    win->dup_ack_num = 0;
    win->LAR = 0;
    win->LFS = 0;
    win->DAT = 0;
    win->state = SS_DEFAULT;
    /* 初始化窗口大小在握手中进行 */
    // win->adv_window = get_window_size(WINDOW_INITIAL_ADVERTISED);
    // win->my_adv_window = get_window_size(WINDOW_INITIAL_ADVERTISED);
    win->TimeoutInterval = 1000000;
    win->EstimatedRTT = 1000000;
    win->DevRTT = 0;

    return EXIT_SUCCESS;
}


static void copy_string_to_buffer(window_t *win, cmu_socket_t* sock){
    char *data = sock->sending_buf;
    int len = sock->sending_len;
    int start = win->DAT % MAX_BUFFER_SIZE;
    int buf_len = len;
    if(start + len > MAX_BUFFER_SIZE){
        buf_len = MAX_BUFFER_SIZE - start;
        memcpy(win->send_buffer+start,data,buf_len);
    }
    else{
        memcpy(win->send_buffer+start,data,buf_len);
    }
    sock->sending_len -= buf_len;
    if(sock->sending_len != 0){
        char *buf = malloc(sock->sending_len);
        memcpy(buf,data+buf_len,sock->sending_len);
        free(sock->sending_buf);
        sock->sending_buf = buf;
    }
    else{
        free(sock->sending_buf);
        sock->sending_buf = NULL;
    }
    win->DAT += buf_len;
}


static int copy_string_from_buffer(window_t *win, SWPSeq idx, char *data, int max_len){
    idx = idx % MAX_BUFFER_SIZE;
    int len = min(win->DAT-idx,max_len);
    int start = idx % MAX_BUFFER_SIZE;
    if(start + len > MAX_BUFFER_SIZE){
        int temp = MAX_BUFFER_SIZE-start;
        memcpy(data,win->send_buffer+start, temp);
        memcpy(data+temp,win->send_buffer,len-temp);
    }
    else{
        memcpy(data,win->send_buffer+start, len);
    }
    return len;
}


void slide_window_activate(window_t *win, cmu_socket_t *sock)
{
    /* 检查缓冲区是否有数据，如果有数据转移至发送窗口内 */
    int buf_len = sock->sending_len;
    fprintf(win->log,"activate %d, %d(DATA), %d(LAR), %d(LFS)\n",sock->state,win->DAT,win->LAR,win->LFS);
    fflush(win->log);
    if(buf_len > 0 && (win->DAT == win->LAR)){
        copy_string_to_buffer(win,sock);
    }
    fprintf(win->log,"123activate %d, %d(DATA), %d(LAR), %d(LFS)\n",sock->state,win->DAT,win->LAR,win->LFS);
    fflush(win->log);
     /* 有数据需要发送 */
    if(win->DAT > win->LAR){
        slide_window_send(win,sock);
    }
    //关闭滑窗前发送最后一个数据包
    if(win->DAT == win->LAR && sock->state == TCP_CLOSE_WAIT){
        char *rsp = create_packet(sock->my_port, sock->their_port, 
                sock->window.last_ack_received,
                sock->window.last_seq_received, 
                DEFAULT_HEADER_LEN, DEFAULT_HEADER_LEN, ACK_FLAG_MASK|FIN_FLAG_MASK,
                        win->adv_window, 0, NULL, NULL, 0);
        sendto(sock->socket, rsp, DEFAULT_HEADER_LEN, 0, 
                    (struct sockaddr*) &(sock->conn), sizeof(sock->conn));
        free(rsp);
        sock->state = TCP_LAST_ACK;
    }
}




// TODO:没好

void slide_window_send(slide_window_t *win, cmu_socket_t *sock){
    // sleep(1);
    // fprintf(stdout,"# send ready\n");
    /* 如果状态不对不让发数据 */
    // if((sock->state != TCP_ESTABLISHED) && (sock->state != TCP_CLOSE_WAIT)){
    //     fprintf(stdout,"## send state error\n");
    //     return;
    // }
	
	char* msg;  /* 每一次发送的UDP包 */
    char *data; /* 从win send buffer中取出的数据 */
	int plen;
	size_t conn_len = sizeof(sock->conn);
	uint32_t ack;   /* 当前发送的序列号 */
    uint32_t mkpkt_seq = win->last_ack_received;    /* 当前打包的seq */
    int buf_len,adv_len = MAX_LEN;
    /* 所有数据是否全部发送 */
    if(win->DAT == win->LAR){
        fprintf(win->log,"# send: no data left\n");
        return;
    }
    ack = win->last_seq_received;
    /* 如果数据已经全部发送了，等待ACK或者超时 */
    if((win->DAT == win->LFS)&&(win->stat == SS_DEFAULT)){
        win->stat = SS_SEND_OVER;
        sleep(1);
    }
    // /* 检查接收窗口是否满了 */
    // // printf("LFS:%d, LAR:%d, adv_win:%d\n",win->LFS,win->LAR,win->adv_window);
    // // printf("flag: %d\n",win->LFS - win->LAR - MAX_DLEN);
    // if((win->LFS + MAX_DLEN - win->LAR > (int)win->adv_window)&&win->stat == SS_DEFAULT){
    //     if(win->adv_window == 0){
    //         adv_len = 1;
    //     }
    //     else if(win->adv_window < MAX_DLEN){
    //         adv_len =  win->adv_window;
    //     }
    //     else{
    //         /* 不能再发送数据 */
    //         win->state = SS_WAIT_ACK;
    //     }
    // }

    switch(win->state){
        case SS_DEFAULT:  /* 正常发包 */
            // buf_len = win->DAT - win->LFS;
            // /* 如果包太长,分多次发送,每次只发送最大包长 */
            // buf_len = (buf_len <= adv_len)?buf_len:adv_len;
            // /* 如果窗口大小过小 */
            // plen = DEFAULT_HEADER_LEN + buf_len;
            data = (char *)malloc(buf_len);
            copy_string_from_buffer(win,win->LFS,data,buf_len);
            // fprintf(win->log,"send msg: %s\nseq%d,ack%d,buf_len%d\n",data,mkpkt_seq,ack,buf_len);
            mkpkt_seq = (win->last_ack_received + (win->LFS - win->LAR))%MAX_SEQ_NUM;
            msg = create_packet_buf(sock->my_port, sock->their_port, 
                mkpkt_seq, ack, 
                DEFAULT_HEADER_LEN, plen, NO_FLAG, win->my_adv_window, 0, NULL,
                data, buf_len);
            
            /* 发送UDP的包 */
            sendto(sock->socket, msg, plen, 0, (struct sockaddr*) &(sock->conn), conn_len);
            // printf("send packet size: %d\n",buf_len);
            /* 注意，如果是特别小的包的话采用停等协议 */
            free(msg);
            free(data);
            msg = NULL;
            data = NULL;
            /* 如果没有设置计时器，设置时钟 */
            if(!win->timer_flag){
                set_timer(win->TimeoutInterval/1000000,win->TimeoutInterval%1000000,(void (*)(int))time_out);
                win->timer_flag = 1;
            }
            if(win->send_seq == -1){
                /* 设置发送时间 */
                win->send_seq = mkpkt_seq + buf_len;
                gettimeofday(&win->time_send,NULL);
            }
            /* 发送指针后移 */
            win->LFS += buf_len;
            break;
        // case SS_RESEND:  /* 马上重发 */
        //     /* 如果缓存中有数据 */
        //     if(win->LFS > win->LAR){
        //         buf_len = win->LFS - win->LAR;
        //         /* 如果包太长,分多次发送,每次只发送最大包长 */
        //         buf_len = (buf_len <= adv_len)?buf_len:adv_len;
        //         plen = DEFAULT_HEADER_LEN + buf_len;
        //         data = (char *)malloc(buf_len);
        //         copy_string_from_buffer(win,win->LAR,data,buf_len);
        //         mkpkt_seq = win->last_ack_received;
        //         msg = create_packet_buf(sock->my_port, sock->their_port, 
        //             mkpkt_seq, ack, 
        //             DEFAULT_HEADER_LEN, plen, NO_FLAG, win->my_adv_window, 0, NULL,
        //             data, buf_len);
        //         /* 发送UDP的包 */
        //         sendto(sock->socket, msg, plen, 0, (struct sockaddr*) &(sock->conn), conn_len);
        //         free(msg);
        //         free(data);
        //         msg = NULL;
        //         data = NULL;
        //         unset_timer();
        //         set_timer(win->TimeoutInterval/1000000,win->TimeoutInterval%1000000,(void (*)(int))time_out);
        //         win->timer_flag = 1;
        //         /* 设置发送时间 */
        //         if(win->send_seq == -1){
        //             win->send_seq = mkpkt_seq + buf_len;
        //             gettimeofday(&win->time_send,NULL);
        //         }
        //     }
        //     win->stat = SS_DEFAULT;
        //     break;
        // case SS_TIME_OUT:  /* 超时 */
        //     if(win->LFS > win->LAR){
        //         buf_len = win->LFS - win->LAR;
        //         /* 如果包太长,分多次发送,每次只发送最大包长 */
        //         buf_len = (buf_len <= adv_len)?buf_len:adv_len;
        //         /* 重新打包 */
        //         plen = DEFAULT_HEADER_LEN + buf_len;
        //         data = (char *)malloc(buf_len);
        //         copy_string_from_buffer(win,win->LAR,data,buf_len);
        //         mkpkt_seq = win->last_ack_received;
        //         msg = create_packet_buf(sock->my_port, sock->their_port, 
        //             mkpkt_seq, ack, 
        //             DEFAULT_HEADER_LEN, plen, NO_FLAG, win->my_adv_window, 0, NULL,
        //             data, buf_len);
        //         /* 发送UDP的包 */
        //         sendto(sock->socket, msg, plen, 0, (struct sockaddr*) &(sock->conn), conn_len);
        //         free(msg);
        //         free(data);
        //         msg = NULL;
        //         data = NULL;
        //         unset_timer();
        //         set_timer(win->TimeoutInterval/1000000,win->TimeoutInterval%1000000,(void (*)(int))time_out);
        //         win->timer_flag = 1;
        //         /* 设置发送时间 */
        //         if(win->send_seq == -1){
        //             win->send_seq = mkpkt_seq + buf_len;
        //             gettimeofday(&win->time_send,NULL);
        //         }
        //     }
        //     win->stat = SS_DEFAULT;
        //     break;

        case SS_SEND_OVER:
            win->stat = SS_DEFAULT;
            break;
        default:
            break;
    }
    sigprocmask(1 /* SIG_UNBLOCK */ ,&mask,NULL);
    // printf("slide window send over\n");
}




/* 处理数据 */
tatic void slide_window_handle_message(slide_window_t * win, cmu_socket_t *sock, char* pkt){
    
}



/* 关闭滑窗 */
void slide_window_close(slide_window_t *win){
    fclose(win->log);
    signal(SIGALRM,SIG_DFL);
}