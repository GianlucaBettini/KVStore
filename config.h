#ifndef CONFIG_H
#define CONFIG_H

#define MAX_BUF_SIZE 32768		// 2^15
#define INIT_READ_BUF_SIZE 8192 // 2^13
#define INIT_WRITE_BUF_SIZE 8192
#define PORT "8080"
#define BACKLOG 4096
#define NUM_BUCKETS 10000
#define MAX_EVENTS 128
#define MAX_CLIENTS                                                            \
	10024 // the real max number of clients is MAX_CLIENTS - 5 because fd 0,1,2
		  // are stdin,stdout,stderr, fd 3,4 are listensock, epollfd
#define MAX_VAL_LEN 1023

#endif