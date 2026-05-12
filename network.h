#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

#define ANET_OK 0
#define ANET_ERR -1

/* It must be called by the server to: get the address info, create a socket,
 * bind it to a port.
 *
 * It takes the IP address @host (NULL if localhost), the @port and the address
 * of the @sockfd (whose value will be filled).
 * Return @ANET_OK on success, @ANET_ERR on error.
 *
 * TODO This function must be splitted into two function: one for the info and
 * one for the initialization. */
// TODO
int net_info(char *host, char *port, int *sockfd);

/* It creates the socket, filling @sockfd with the assigned file descriptor.
 * Return @ANET_OK on success, @ANET_ERR on error. */
int net_socket(struct addrinfo *p, int *sockfd);

/* It sets the reuse address option on @sockfd.
 * Return @ANET_OK on success, @ANET_ERR on error. */
int net_setsockopt(int sockfd);

int net_bind(int sockfd, struct addrinfo *p);

int net_listen(int sockfd, int backlog);

int net_accept(int sockfd, int *clientfd, struct sockaddr_storage *client_addr);

/* Recv bytes from @clientfd, putting them into @server_buf starting from the
 * value pointed by @curr_buf_len. The max amount of bytes read is
 * (@max_buf_len - *@curr_buf_len), which is the space left in @server_buf.
 * It increments *@curr_buf_len by the amount of bytes received.
 * Return num_bytes read on success, -1 on error, 0 on client disconnection. */
ssize_t net_recv(int clientfd, char *server_buf, size_t len);

/* Send @str_to_send to @clientfd.
 * Return @ANET_OK on success, @ANET_ERR on error. */
ssize_t net_send(int clientfd, char *buf, size_t len); // char *sender_buf ???

/* Store into @buf_to_parse the bytes in @server_buf until @target is found and
 * shift @server_buf of the read bytes times. Return @ANET_OK.
 * If not found in the first *@curr_buf_len bytes, returns @ANET_ERR. */
// TODO Consider to change int *curr_buf_len into size_t *curr_buf_len.
int get_command_to_scan(char *server_buf, size_t *curr_buf_len,
						char *buf_to_parse, char target);
