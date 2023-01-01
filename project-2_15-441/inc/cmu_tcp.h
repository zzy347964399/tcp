/**
 * Copyright (C) 2022 Carnegie Mellon University
 *
 * This file is part of the TCP in the Wild course project developed for the
 * Computer Networks course (15-441/641) taught at Carnegie Mellon University.
 *
 * No part of the project may be copied and/or distributed without the express
 * permission of the 15-441/641 course staff.
 *
 *
 * This file defines the API for the CMU TCP implementation.
 */

#ifndef PROJECT_2_15_441_INC_CMU_TCP_H_
#define PROJECT_2_15_441_INC_CMU_TCP_H_

#include <netinet/in.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "cmu_packet.h"
#include "global.h"
#include "grading.h"
//理论上只有server能调用，但是为了让client用上check_for_data所以写到这里来
// #include "backend.h"

#define EXIT_SUCCESS 0
#define EXIT_ERROR -1
#define EXIT_FAILURE 1
#define MAXSEQ 100
#define MAX_BUFFER_SIZE 1000000

/* 滑窗的下标 */
typedef int SWPSeq;  /* slide window protocol序列号 */ 

/* 发送者的状态 */
typedef enum {
	SS_DEFAULT = 1,   /* 默认状态 */
	SS_TIME_OUT = 2,   /* 超时事件 */
	SS_RESEND = 3,   /* 重发事件 */
	SS_SEND_OVER = 4,  /* 当前数据发送完毕 */
	// SS_WAIT_ACK = 6  /* 窗口满了，需要等待ACK */ t不想做2333
} send_state;

//以链表形式发送滑动窗口
typedef struct {
	uint8_t used;  /* slot是否还在使用，0标识没有使用，1标识正在使用 */
	uint8_t time_flag;  /* 定时器 */
	char *msg;  /* 已经打包好的packet */
} sendQ_slot;

/* 滑窗接收窗口单位 */
typedef struct RecvQ_slot {
	uint8_t recv_or_not;
	char *msg;
	struct RecvQ_slot *next; /* 组织成链表的形式 */
} recvQ_slot;


/* 定义滑窗协议的窗口结构 */
typedef struct {
  uint32_t next_seq_expected; /* 上一个seq序列 */
  uint32_t last_ack_received; /* 上一个ack序列 */
  pthread_mutex_t ack_lock;   /* ack的锁（因为ack会增加） */
  uint32_t last_seq_received;   //其实在接受的时候可以用nextseq代替，但是这样比较符合直觉
  uint32_t adv_window;  //发件人可以接受的窗口大小，就是WINDOW_INITIAL_WINDOW_SIZE
	send_state state;  /* 发送方所处的状态 */
  // sem_t sendlock;  /* 用信号量控制窗口大小，如果窗口满了会堵塞 */
	SWPSeq LAR; /* last ack recv */ /* |----------LAR+++++++++LFS--------| */
	SWPSeq LFS; /* last frame/byte send */
	SWPSeq DAT; /* 数据的最大下标 */
  char send_buffer[MAX_BUFFER_SIZE+1];  /* 发送者缓冲 */
	uint32_t seq_expect;  /* 接收下一个包的seq序列 */
  uint32_t next_send_seq; //将发送的下一个包的seq序列
  uint32_t dup_ack_num; /* 当前收到ack的数量 */

	// recvQ_slot recv_buffer_header;  /* 缓存已收到的数据 */
	// uint8_t timer_flag;  /* 滑窗的计时器是否设置 */
	// FILE *log;
	/* 以下数据结构用于计算超时重传间隔 */
	// struct timeval time_send;  /* 发送包的时间 */
	// SWPSeq send_seq;
	long TimeoutInterval;  /* 超时时间 */
	long EstimatedRTT;  /* （加权）平均RTT时间 */
	long DevRTT;  /* RTT偏差时间 */
  
	// uint32_t CWND; /* 拥塞窗口大小,单位是字节数 */
	// uint32_t Ssthresh; /* 慢启动阈值，单位是字节数 */
	// CongestionState congestionState;
} window_t;

/**
 * CMU-TCP socket types. (DO NOT CHANGE.)
 */
typedef enum {
  TCP_INITIATOR = 0,
  TCP_LISTENER = 1,
} cmu_socket_type_t;



/**
 * This structure holds the state of a socket. You may modify this structure as
 * you see fit to include any additional state you need for your implementation.
 */
typedef struct {
  int socket;                /* socket端口号 */
  pthread_t thread_id;       /* 后端运行的线程号 */
  uint16_t my_port;          /* 本机端口 */
  uint16_t their_port;       /* 通讯端口 */
  struct sockaddr_in conn;   /* 通讯目标socket地址 */
  uint8_t* received_buf;     /* 接收数据缓冲，初始化为NULL */
  int received_len;          /* 接收数据大小，初始化为0 */
  pthread_mutex_t recv_lock; /* 缓冲区的锁 */
  pthread_cond_t wait_cond;
  uint8_t* sending_buf;      /* 发送区域的缓冲，初始化为NULL */
  int sending_len;           /* 发送区长度 */
  cmu_socket_type_t type;    /* 发送者0或者监听者1 */
  pthread_mutex_t send_lock; /* 发送缓冲的锁 */
  int dying;                 /* 连接是否关闭，默认为false */
  pthread_mutex_t death_lock;
  window_t window; /* 滑窗 */
  TCP_State state;
} cmu_socket_t;



/*
 * DO NOT CHANGE THE DECLARATIONS BELOW
 */

/**
 * Read mode flags supported by a CMU-TCP socket.
 */
typedef enum {
  NO_FLAG = 0,  // Default behavior: block indefinitely until data is available.
  NO_WAIT,      // Return immediately if no data is available.
  TIMEOUT,      // Block until data is available or the timeout is reached.
} cmu_read_mode_t;

/**
 * Constructs a CMU-TCP socket.
 *
 * An Initiator socket is used to connect to a Listener socket.
 *
 * @param sock The structure with the socket state. It will be initialized by
 *             this function.
 * @param socket_type Indicates the type of socket: Listener or Initiator.
 * @param port Port to either connect to, or bind to. (Based on socket_type.)
 * @param server_ip IP address of the server to connect to. (Only used if the
 *                 socket is an initiator.)
 *
 * @return 0 on success, -1 on error.
 */
int cmu_socket(cmu_socket_t* sock, const cmu_socket_type_t socket_type,
               const int port, const char* server_ip);

/**
 * Closes a CMU-TCP socket.
 *
 * @param sock The socket to close.
 *
 * @return 0 on success, -1 on error.
 */
int cmu_close(cmu_socket_t* sock);

/**
 * Reads data from a CMU-TCP socket.
 *
 * If there is data available in the socket buffer, it is placed in the
 * destination buffer.
 *
 * @param sock The socket to read from.
 * @param buf The buffer to read into.
 * @param length The maximum number of bytes to read.
 * @param flags Flags that determine how the socket should wait for data. Check
 *             `cmu_read_mode_t` for more information. `TIMEOUT` is not
 *             implemented for CMU-TCP.
 *
 * @return The number of bytes read on success, -1 on error.
 */
int cmu_read(cmu_socket_t* sock, void* buf, const int length,
             cmu_read_mode_t flags);

/**
 * Writes data to a CMU-TCP socket.
 *
 * @param sock The socket to write to.
 * @param buf The data to write.
 * @param length The number of bytes to write.
 *
 * @return 0 on success, -1 on error.
 */
int cmu_write(cmu_socket_t* sock, const void* buf, int length);

/*
 * You can declare more functions after this point if you need to.
 */

#endif  // PROJECT_2_15_441_INC_CMU_TCP_H_
