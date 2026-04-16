#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

#define ANET_OK 0
#define ANET_ERR -1

/* TODO This function must be splitted into two function: one for the info and
 * one for the initialization */
int net_info(char *host, char *port, int *sockfd);

int net_socket(struct addrinfo *p, int *sockfd);

int net_setsockopt(int sockfd);

int net_bind(int sockfd, struct addrinfo *p);

int net_listen(int sockfd, int backlog);

int net_accept(int sockfd, int *clientfd, struct sockaddr_storage *client_addr);

int net_recv(int clientfd, char *server_buf, int *curr_buf_len,
             int max_buf_len);

int net_send(int clientfd, char *str_to_send); // char *sender_buf ???

int get_command_to_scan(char *server_buf, int *curr_buf_len, char *buf_to_parse,
                        char target);
