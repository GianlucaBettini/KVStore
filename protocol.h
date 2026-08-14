#ifndef PROTOCOL_H
#define PROTOCOL_H

#define STATUS_LEN 1
#define STATUS_SUCCESS 0
#define STATUS_ERROR 1
#define STATUS_NOT_FOUND 2
#define STATUS_BAD_REQUEST 3
#define HEADER_LEN 4
#define CMD_HEADER 1
#define KEY_HEADER 2
#define VAL_HEADER 2

typedef enum command_type { CMD_SET, CMD_GET, CMD_DEL, CMD_INVALID } cmd_type_t;

typedef enum buf_state { READING_HEADER, READING_PAYLOAD } buf_state_t;

#endif