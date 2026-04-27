#include "hash_table.h"
#include "network.h"
#include "parser.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define BUF_SIZE 256
#define PORT "8080"
#define BACKLOG 10

bool exec_cmd(parsed_input_t *, hash_table_t *, char *);

int main(void) {
  hash_table_t *ht;
  int rv;
  int sockfd, clientfd;
  struct sockaddr_storage client_addr;

  if ((rv = net_info(NULL, PORT, &sockfd)) == ANET_ERR) {
    return 1; // you can create an "error layer"
  }

  if ((rv = net_listen(sockfd, BACKLOG)) == ANET_ERR) {
    close(sockfd);
    return 1; // you can create an "error layer"
  }
  printf("server listening on port %s\n", PORT);

  ht = ht_create(10);

  parsed_input_t parsed;
  parsed.key = NULL;
  parsed.val = NULL;

  while (1) {
    if ((rv = net_accept(sockfd, &clientfd, &client_addr)) == ANET_ERR) {
      continue;
    }

    char sender_buf[BUF_SIZE];
    char server_buf[BUF_SIZE];
    int curr_buf_len = 0;
    char buf_to_parse[BUF_SIZE];

    while (1) {

      int num_bytes = net_recv(clientfd, server_buf, &curr_buf_len, BUF_SIZE);

      if (num_bytes == -1) {
        // error
        break;
      } else if (num_bytes == 0) {
        // client disconnected
        break;
      }

      while ((rv = get_command_to_scan(server_buf, &curr_buf_len, buf_to_parse,
                                       '\n')) == ANET_OK) {
        if (!parse_input(buf_to_parse, &parsed)) {
          net_send(clientfd, "Invalid syntax\n");
          continue;
        }

        if (exec_cmd(&parsed, ht, sender_buf)) {
          net_send(clientfd, sender_buf);
        } else {
          net_send(clientfd, "Error\n"); // to fix the error str
        }
      }
    }

    close(clientfd);
  }

  close(sockfd);

  ht_destroy(ht);
  return 0;
}

bool exec_cmd(parsed_input_t *parsed, hash_table_t *ht, char *sender_buf) {
  char *val = NULL;
  switch (parsed->type) {
  case CMD_SET:
    if (ht_set(ht, parsed->key, parsed->val))
      strcpy(sender_buf, "OK\n");
    else
      strcpy(sender_buf, "Not OK\n");
    break;
  case CMD_GET:
    val = ht_get(ht, parsed->key);
    if (val == NULL)
      strcpy(sender_buf, "Not found\n");
    else
      snprintf(sender_buf, BUF_SIZE, "%s\n", val);
    break;
  case CMD_DEL:
    if (ht_del(ht, parsed->key)) {
      strcpy(sender_buf, "Deleted\n");
    } else {
      strcpy(sender_buf, "Not found\n");
    }
    break;
  default:
    return false;
  }

  return true;
}