"""
Edge Cases Test Suite for the KV Store.

This script tests the resilience of the custom length-prefixed binary protocol 
against malformed packets, buffer overflows, and sudden disconnections.
The server is expected to gracefully handle these scenarios without crashing 
or leaking memory (to be verified via Valgrind).

Test scenarios:
1. Invalid Command: Parses correctly and replies with BAD_REQUEST.
2. Out of Bounds Payload: Payload size lies about key length.
3. Buffer Overflow: Exceeds MAX_BUF_SIZE. Server must drop connection.
4. Unexpected EOF: Client drops connection mid-payload.
"""


import socket
import struct
import threading
import time

HOST = '127.0.0.1'
PORT = 8080

CMD_SET = 0
CMD_GET = 1
CMD_DEL = 2

CMD_SIZE = 1
KEY_SIZE = 2
VAL_SIZE = 2
HEADER_SIZE = 4

STATUS_SUCCESS = 0
STATUS_ERROR = 1
STATUS_NOT_FOUND = 2
STATUS_BAD_REQUEST = 3


def main():
    # === invalid command ===
    # Try to send a packet with a non valid command (first byte of the payload) 
    # the server must reply with a BAD_STATUS_REQUEST 
    # the connection must remain open
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((HOST, PORT))

    cmd_type = 85
    key = b"don't care"
    key_len = len(key)
    payload_size = CMD_SIZE + KEY_SIZE + key_len
    fmt = f">IBH{key_len}s"
    packet = struct.pack(fmt, payload_size, cmd_type, key_len, key)
    s.sendall(packet)

    header_bytes = s.recv(HEADER_SIZE)

    assert len(header_bytes) == HEADER_SIZE, "The server closed without replying"

    reply_payload_size = struct.unpack(">I", header_bytes)[0]
    reply_payload_bytes = s.recv(reply_payload_size)
    status = struct.unpack(">B", reply_payload_bytes[:1])[0]

    assert status == STATUS_BAD_REQUEST, f"expected status: {STATUS_BAD_REQUEST}, received: {status}"
    # print("Invalid command managed correctly. \nChecking if the connection is still open...")

    valid_cmd = CMD_GET
    fmt = f">IBH{key_len}s"
    packet_valid = struct.pack(fmt, payload_size, valid_cmd, key_len, key)

    try:
        s.sendall(packet_valid)
        header_bytes = s.recv(HEADER_SIZE)
        assert len(header_bytes) == HEADER_SIZE, "server closed the connection after the error"
        # print("Connection still open")
        print("------------------------------")
        print("Invalid command test passed")
        print("------------------------------\n\n")
    except Exception as e:
        assert False, f"the connection died after the invalid command: {e}"

    s.close()

    # === out of bounds payload ===
    # Try to send a GET request indicating a payload size shorter than the actual payload size
    # that is 1 + 2 + key_size
    # the server must reply with a STATUS_BAD_REQUEST packet
    # the connection must remain open
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((HOST, PORT))

    cmd_type = CMD_GET
    fake_key_len = 50000
    payload_size = 10
    fmt = f">IBH7s"
    packet = struct.pack(fmt, payload_size, cmd_type, fake_key_len, b"X"*7) # 14 bytes that is 4 + 10
    s.sendall(packet)

    header_bytes = s.recv(HEADER_SIZE)
    
    assert len(header_bytes) == HEADER_SIZE, "The server closed without replying"

    reply_payload_size = struct.unpack(">I", header_bytes)[0]
    reply_payload_bytes = s.recv(reply_payload_size)
    status = struct.unpack(">B", reply_payload_bytes[:1])[0]
    returned_val = reply_payload_bytes[1:] if reply_payload_size > 1 else b""

    assert status == STATUS_BAD_REQUEST, f"expected status: {STATUS_BAD_REQUEST}, received: {status}"
    assert returned_val == b"", f"no value expected, instead received: {returned_val}"
    # print("Out of bounds managed correctly. \nChecking if the connection is still open...")

    valid_cmd = CMD_GET
    valid_key = b"test"
    valid_key_len = len(valid_key)
    payload_size = CMD_SIZE + KEY_SIZE + valid_key_len
    fmt = f">IBH{valid_key_len}s"
    packet_valid = struct.pack(fmt, payload_size, valid_cmd, valid_key_len, valid_key)

    try:
        s.sendall(packet_valid)
        header_bytes = s.recv(HEADER_SIZE)
        assert len(header_bytes) == HEADER_SIZE, "server closed the connection after the error"
        # print("Connection still open")
        print("------------------------------")
        print("Out of bounds test passed")
        print("------------------------------\n\n")
    except Exception as e:
        assert False, f"the connection died after the out of bounds command: {e}"

    s.close()

    # === Buffer overflow ===
    # Try to send a packet which is bigger than the maximum possible size of the client buffer
    # the server must not reply 
    # the connection must be closed

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((HOST, PORT))

    packet = b"X" * 40000
    s.sendall(packet)

    try:
        res = s.recv(4)
        assert res == b"", "the server did not close the connection after the buffer overflow!"
    except ConnectionResetError: 
        # expected behavior: server closed abruptly due to unread data in its buffer
        pass

    print("------------------------------")
    print("Buffer overflow test passed")
    print("------------------------------\n\n")
    s.close()



    # === Sudden disconnection - unexpected EOF ===
    # The client wants to send a packet of 104 bytes
    # the first 4 bytes arrive, indicating an 100 bytes payload
    # then, 10 bytes out of the 100 of the payload are sent by the client 
    # and the client disconnect in the middle of the communication
    # the server must disconnect the client 

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((HOST, PORT))

    s.sendall(struct.pack(">I", 1000))
    s.sendall(b"A" * 10)
    s.close()

    # check if the server survived the sudden EOF without crashing
    time.sleep(0.1) # brief pause to let the server OS process the EOF

    s_ping = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s_ping.connect((HOST, PORT))

    valid_cmd = CMD_GET
    valid_key = b"ping_key"
    valid_key_len = len(valid_key)
    payload_size = CMD_SIZE + KEY_SIZE + valid_key_len
    fmt = f">IBH{valid_key_len}s"
    packet_valid = struct.pack(fmt, payload_size, valid_cmd, valid_key_len, valid_key)

    try:
        s_ping.sendall(packet_valid)
        header_bytes = s_ping.recv(HEADER_SIZE)
        assert len(header_bytes) == HEADER_SIZE, "Server crashed after sudden EOF!"
        print("------------------------------")
        print("Sudden disconnection test passed (Server is still alive)")
        print("------------------------------\n\n")
    except Exception as e:
        assert False, f"Server did not survive the sudden EOF: {e}"
    
    s_ping.close()


    # === TCP Fragmentation ===
    # Send a packet split in more than one send call, to test the packet TCP fragmentation management
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((HOST, PORT))

    valid_cmd = CMD_GET
    valid_key = b"not_inserted_key"
    valid_key_len = len(valid_key)
    payload_size = CMD_SIZE + KEY_SIZE + valid_key_len
    fmt = f">IBH{valid_key_len}s"
    packet_valid = struct.pack(fmt, payload_size, valid_cmd, valid_key_len, valid_key)
    
    packet = struct.pack(fmt, payload_size, valid_cmd, valid_key_len, valid_key)
    
    s.sendall(packet[:2]) # Send just the first 2 bytes of the header
    time.sleep(0.1)
    s.sendall(packet[2:6]) # Send the rest of the header + part of the payload
    time.sleep(0.1)
    s.sendall(packet[6:]) # Send the remaining bytes

    header_bytes = s.recv(HEADER_SIZE)
    assert len(header_bytes) == HEADER_SIZE, "Fragmentation NOT properly handled"
    reply_payload_size = struct.unpack(">I", header_bytes)[0]
    reply_payload_bytes = s.recv(reply_payload_size)
    status = struct.unpack(">B", reply_payload_bytes[:1])[0]
    
    assert status == STATUS_NOT_FOUND, f"expected status: {STATUS_NOT_FOUND}, received: {status}"
    print("------------------------------")
    print("TCP fragmentation test passed")
    print("------------------------------\n\n")
    s.close()

    print("[ALL TESTS PASSED]")

    return

main()