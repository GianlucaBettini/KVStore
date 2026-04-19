# C-KV: A High-Performance Key-Value Store

A POSIX-compliant, in-memory Key-Value store written entirely in C from scratch. Designed with a strict focus on zero-dependency architecture, manual memory management, and blocking TCP network I/O (with epoll-based concurrency planned for v1.0).

## Architecture v0.1 (MVP)

The current version implements a baseline single-threaded, in-memory storage engine supporting basic string operations.

### Supported Commands
* `SET <key> <value>`: Stores or overwrites a string value.
* `GET <key>`: Retrieves the value associated with the key.
* `DEL <key>`: Removes the key and its value from memory.

### Core Components
The system is decoupled into three independent modules:
1. **Network/Transport Layer:** Handles TCP socket binding, connection acceptance, and raw byte buffering.
2. **Protocol Parser:** Tokenizes raw byte streams into structured commands, keys, and payloads.
3. **Storage Engine:** A custom-built, dynamically resizing Hash Table for O(1) average time complexity lookups.

## Build & Run
```bash
make clean
make all
./kvstore
```

## Run unit tests
```bash
make clean_tests
make test 
```

## Run integration tests
```bash
make clean
make all
python3 tests/test_e2e.py
```

## Interacting with the Server
Once the server is running, open a new terminal and connect using Netcat:
```bash
nc 127.0.0.1 8080
```
Then type your command directly:
```bash
SET mykey myval
GET mykey
DEL mykey
```