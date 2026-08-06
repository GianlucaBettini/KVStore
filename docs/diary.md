# Engineering Logbook

## [26-03-2026] - Architecture Design & MVP Scoping

### 🎯 Goal
Define the macro-architecture for v0.1 and set up the build environment.

### ⚖️ Architectural Decisions & Scope
* **Decision:** Keep the MVP strictly single-threaded and in-memory.
* **Why:** To avoid premature optimization, which can be the root of big problems. Introducing multithreading (pthreads) or disk I/O now would distract from the primary goal: mastering POSIX socket APIs and building a leak-free Hash Table in C. I will introduce concurrency in v0.2.
* **Decision:** Decouple the Network Layer from the Protocol Parser and Storage Engine.
* **Why:** Separation of concerns. The Hash Table should not care whether the data comes from a TCP socket, a UDP packet, or a local file. This modularity will allow for easier unit testing later.

### 🧠 Mental Model: The Data Flow
To ensure I understand the system boundaries before writing code, I mapped out the lifecycle of a `SET user:1 mario` command:
1. **Network:** `read()` pulls bytes from the OS TCP buffer.
2. **Parser:** Tokenizes the raw array into `SET`, `user:1`, `mario`.
3. **Engine:** Hashes `user:1`, calculates the index, `malloc`s memory for the nodes, and wires the pointers.
4. **Network:** Pushes `+OK\n` back to the client socket.

P.S. The server is already started and bound to a port; the client is already connected. 


## [27-03-2026] - Hash Table Design: Collision Resolution

### 🎯 Goal
Define the core data structure and memory layout for the Key-Value Store.

### ⚖️ Architectural Decisions
* **Decision:** Use Separate Chaining (array of linked lists) to manage collisions. 
* **Why:** 
    1. It degrades gracefully and I need operational simplicity and stability. If a collision occurs, chaining simply appends the entry to a linked list at that index. So the table never fills up completely, but it just becomes slightly slower to travers. 
    2. It's simple to implement delete operations (just unlink a node). 
    3. Immune to clustering.

    Cons of Separate Chaining: 

    1. High memory overhead (every key-value pair requires an extra pointer for the linked list).
    2. Poor cache locality; because you are calling malloc for every new node, the nodes are scattered randomly across the heap, meaning the CPU cache cannot pre-fetch them efficiently.

    Open Addressing is not used here. Its cons are: 

    1. Prone to "clustering" (chains of collisions that kill performance).
    2. Deletion is notoriously difficult. You can't simply remove an item, or you break the probing chain. 

    although the pros are: 

    1. Cache locality. The data sits in one contiguous array in memory, which is good for CPU. 
    2. No memory overhead for pointers. 
* **Decision:** Hash function: djb2 by Dan Bernstein
* **Why:** It's just a few lines of standard code; it requires zero external libraries; it's a simple algorithm and its hash space isn't perfectly uniform, which allows to easily test if the collision resolution code actually works.

## [29-03-2026] - Protocol Parser Layer implementation

### 🎯 Goal
Build a local REPL with stdin and the parsing pipeline. 

### ⚖️ Architectural Decisions
* **Decision:** Zero-copy parsing
* **Why:** To avoid allocating new memory. Instead it mutates the input buffer by replacing delimitations with the null terminator '\0' and passes pointers to those substrings. This keeps CPU cache locality high and latency low. 
* **How:** Used strtok_r (thread-safe) function to mutate the string in place. 
* **Decision:** Separation of tokenization and execution of the command (dispatcher).
* **Why:** To keep concerns separated. 
* **Workflow:** The parser translates the input string into a struct with the command type (enum), the key and eventually the val. 
The enum allows to use a fast switch-case instead of using strcmp (which is slower). 
To keep it simple I decided not to worry about the extra args (e.g. `GET key extra1 extra2`), but in the future it must be corrected. 

## [31-03-2026] - Unit and integration testing of hash table ops

### 🎯 Goal
Implement the unit (assert.h) and integration (python) testing of hash table operations and update conseguently the docs.


## [19-04-2026] - Network Layer implementation

### 🎯 Goal
Replace the local stdin/stdout REPL with a TCP socket server to make the database accessible over the network.

### ⚖️ Architectural Decisions
* **Decision:** Custom POSIX Socket Abstraction API
* **Why:** To enforce Separation of Concerns.
* **How:** Built network.c and network.h to wrap socket creation, binding, listening, accepting, and data transfer into clean functions (e.g., net_listen, net_recv).
* **Decision:** Stateful Linear Buffer (Accumulator) for TCP Fragmentation
* **Why:** TCP is a continuous byte stream, not a datagram protocol. A single recv() call might return half a command or multiple commands glued together (pipelining). Passing incomplete strings directly to the zero-copy parser would break the tokenizer.
* **How:** Implemented get_command_to_scan. It uses memchr to scan the raw network buffer for the \n delimiter, extracts a clean and complete frame, and uses memmove to shift any leftover bytes to the beginning of the buffer for the next read cycle.
* **Decision:** Single-threaded, Iterative, Blocking Model
* **Why:** To validate the core network pipeline and logic (MVP) before introducing the complexity of concurrency (like threads or event loops).
* **Workflow:** The main.c orchestrates everything: an outer loop accepts a connection, and an inner loop reads bytes, extracts frames, passes them to the parser, executes the command against the Hash Table and pushes the formatted response back via net_send.
* **Decision:** Stack Allocation for Network Buffers
* **Why:** To prevent memory leaks and maximize speed.
* **How:** Instead of allocating sender_buf and server_buf on the heap with malloc/free inside the connection loop, they are statically allocated on the stack combined with snprintf to guarantee buffer overflow protection. Result: 0 memory leaks in Valgrind.

## Review of the state of play

## Core storage engine (hash table)
### Overview
The data structure must guarantee O(1) average time complexity for GET, SET, DEL ops, managing all heap allocations manually without memory leaks.
N.B. O(k) in the worst case, with k := #elements in that bucket. 
At the moment, each new node is allocated in the heap, increasing the size of the table by 1 every time we add a new node. 
When the value of a node already in the table is changed, a new char* in allocated in the heap for the value and the one of the old value is freed. 
So in the future it is going to change: there must be a mechanism to use the stack for better performance and to use amortized analysis, for instance doubling the number of buckets when the table grows too much. 

### Decisions
* Array of pointers with separate chaining (linked lists).
Why: while open addressing is cache-friendly, it suffers from severe performance degradation at high load factors and complicated DEL operations (requiring tombstone markers).
Instead, separate chaining handles collisions gracefully by appending nodes to a linked list at the hashed index, keeping implementation robust and deletion clean.

* Hash function: djb2
Why: I need, at least at the moment, a fast and simple hash function with excellent avalanche properties to distribute string keys uniformly across the array and minimize linked list clustering.

### Future proofing
* First dynamic stack resizing, after incremental rehashing 
* Eviction policies (e.g. LRU) to delete old keys when a memory limit is reached

## Protocol parser (zero-copy architecture)

### Decisions
* Zero-copy parsing via in-place mutation
Why: the naive approach is to use malloc and strncpy to extract the command, key and (eventually) value into new memory blocks, implying heap allocations as a bottleneck.
How: the parser treats the input buffer as a mutable string. It scans for delimiters (spaces, \t, \n) and replaces them with the null terminator. It then assigns pointers to the start of these newly substrings.
This keeps cache locality high, requires no heap allocations and therefore is fast.

* Reentrant tokenization (strtok_r)
Why: standard strtok() relies on internal static state, making it thread-unsafe and therefore dangerous in concurrent (or reentrant) environments. 
By enforcing the use of strtok_r (passing an explicit saveptr), the parsing function remains pure and reentrant. 

* Decoupling tokenization from dispatching (separation of concerns)
Why: mixing string parsing logic with database execution logic creates unmaintainable code. 
How: the parser's sole responsibility is to populate a parsed_input_t struct containing the parsed strings (key and, eventually, value) in addition to an enum representing the command type (CMD_SET, CMD_GET, CMD_DEL). Then, it passes this struct to the dispatcher.

### Lesson learned

* Incomplete command "trap":
Problem: if a client sends an incomplete command (like GET\n), the subsequent calls to strtok_r will return NULL. Deferencing these pointers in the execution phase would cause segfault.
Solution: implemented validation checks post-tokenization. The parser verifies that the required number of args are non-null. 

### Future proofing

* RESP, parsing length-prefixed binary-safe strings. 

* Variadic arguments (to support for instance commands like MSET k1 v1 k2 v2 and TTL args like SET k v EX 60 in Redis)

## Network layer 

Build a POSIX socket wrapper (network.c module) to handle TCP connections.
Up to now, it is blocking and has the read/write buf on the stack.

### Lesson learned

* TCP streaming (fragmentation and coalescence)
Problem: TCP is a byte-stream protocol, not a message-oriented one. A client sending "SET user 1\n" might result in recv() returning "SET us" on the first call and "er 1\n" on the second one (fragmentation). Conversely, multiple commands might arrive in a single recv() call (coalescence).
Solution: introduce a curr_buf_len tracker to remember how many bytes were currently in the buffer across multiple recv() calls. Use memchr() to scan for the '\n' delimiter. If no delimiter is found, loops back to the recv(), waiting for the remain bytes. If delimiter is found, the command is extracted and passed to the parser. 

* "Sliding window" extraction (with memmove)
Problem: after extracting a valid command from the buf, there might be leftover bytes from the next command sitting behind it.
Solution: shifting mechanism using memmove(). After extraction, the remaining bytes are shifted to the head of the array and curr_buf_len is updated accordingly. (memmove was chosen over memcpy because the source and destination memory areas could overlap).

### Future proofing

* Concurrency: the v0.1 can only handle one client at a time. It is needed an upgrade to the socket descriptors to O_NONBLOCK via fcntl() and replace the synchronous accept/recv loop with a single thread asynchronous event-loop engine using epoll.

* Sliding window extraction -> circular buffer to avoid memmove

## Single thread asynchronous event-loop (epoll), I/O multiplexing. Theory.

### From synchronous to epoll edge-triggered
Having as many threads as clients would consume too much RAM just for overhead and CPU for context switching. -> event-loop single thread: epoll.
Very briefly: 
* epoll_wait(): the server sleeps here. When the network interface card receives some packets, the Linux Kernel wakes up the server and passes to it the "events" array, which contains the fds ready for reading or writing. 
* non-blocking sockets: the important thing here is that every socket is been modified when accepted, using fcntl(conn_sock, F_SETFL, O_NONBLOCK), making each one of them non-blocking when recv()s and send()s are reached. Instead of blocking there, it fails instantly, setting the errno state to EAGAIN (or EWOULDBLOCK). 

### Edge-triggered (EPOLLET)
Epoll can be set as "Level-triggered" or "Edge-triggered". 
* Level-triggered (default): when there is a packet in the buffer of the kernel, epoll keeps waking you up until you have read it entirely. It is easy to use, but slow under stress (high load).
* Edge-triggered: the kernel wakes you up just once, that is when the socket state changes (from "empty" to "have data"). If you don't read all the data at that moment (draining the buffer completely), the kernel will not wake you up and the client will remain "hang" forever (the remaining data will sit in the buffer forever and therefore no state changes -> no epoll notification).

I chose ET because it is the stardard for high performance systems: 
* less syscalls
* ready list much smaller
* less context switches (if you have a pool of epoll threads)
TODO: get info about "thundering herd" problem. (that ET mitigates)
also for future improvements (using a pool of epoll threads)

How it is implemented:
when a new client is accepted, it is added to the interest list of epoll in this way:
```c
ev.events = EPOLLIN | EPOLLET;
ev.data.fd = conn_sock;
epoll_ctl(epollfd, EPOLL_CTL_ADD, conn_sock, &ev);
```

To safisfy the ET rule (draining the whole buffer) every net call is in a loop:
when the server is woken up by an 'EPOLLIN' event, we enter a loop in which net_recv() is performed, looping until the function returns '-1' and 'errno == EAGAIN'. This is the signal that the kernel buffer was totally drained and therefore we can continue with the next client.

### The problem of the asynchronous writes

If the kernel write buffer (outgoing data buffer) is full, the send() does one of these two things:
* partial write: if I ask to send 1000 bytes but there is space only for 200, the send() returns 200 and the other 800 bytes remain there;
* EAGAIN: if there is no space even for 1 byte, the send() returns '-1' and sets errno = EAGAIN (or EWOULDBLOCK). It means a kind of "try later". 

When I register a socket in the interest list (with epoll_ctl()) I should not do it in EPOLLOUT mode. This is because generally a socket is almost always ready to write. 
-> toggle switch pattern

### Toggle switch pattern

Listen to writing events only when the kernel did not complete a send(). 

Workflow:

* Default state: EPOLLIN
when I accept a connection, I add it to the interest list in read mode
`ev.events = EPOLLIN | EPOLLET;`

* after reading and parsing, exec_cmd() enqueues the result in the write_buf of the client. Then send() is called.
If the send() wrote all the data (`nbytes == write_len`), reset the write_len and we are done.
Else, if partial write or EAGAIN, keep reading the next points of this list

* memory shift (memmove):
if only 100 out of 500 bytes were sent -> memmove (shift the remaining bytes at the head of the buffer)

* setting EPOLLOUT:
after the shift (or EAGAIN), we call the kernel and modify the flag of the socket in the interest list, adding EPOLLOUT.
```c
ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
epoll_ctl(epollfd, EPOLL_CTL_MOD, evfd, &ev);
```

* EPOLLOUT event:
when the buffer of the kernel of the client get spaces, epoll_wait wakes you up with an EPOLLOUT event.
If everything is now sent, you switch off the EPOLLOUT flag.

### Client_state_t 

Because of the fragmentation and coalescence of TCP, the server cannot be stateless. Therefore, every single client has a state.
We have a global array:
```c
client_state_t clients[MAX_CLIENTS]; // MAX_CLIENTS = 10024
```
in which the index of the array is the fd of the element at that index (if present). (Access in O(1)) 

Reading and writing are separated in the state.

```c
typedef struct {
    int fd;
    char *read_buf;       
    char *write_buf;      
    size_t read_len;      // Valid bytes in read_buf
    size_t write_len;     // Valid bytes in write_buf
    size_t read_size;     // Max length of read_buf (allocated memory block size)
    size_t write_size;    // Max length of write_buf (allocated memory block size)
} client_state_t;
```
Writing and reading are separated and independent to avoid conflicts.

The client state buffers are dynamimcally resized (*2), up to an upper bound to avoid DoS attacks.

### Finite State Automata

4 sequential phases:

* Network -> read_buf
When there is an EPOLLIN event (something to read), we enter into a while(1).
Calling net_recv() to drain the buffer of the kernel and to enqueue the bytes into read_buf of the client, the loop stops when:
the client disconnects (nbytes == 0);
the kernel is empty (errno == EAGAIN).
When there is EAGAIN, the phase 2 begins. 

the next 2 phases compose the parser loop

* read_buf -> command
get_command_to_scan() is used to extract the command. 
It looks for '\n', the delimiter. 
If not found, break. 
If found, extract the entire command, places '\0' at the tail of it to make it a valid string and does the shifting, through memmove, of all the bytes after the found delimiter and shift them at the head of the array, updating the read_len. 

* parser -> write_buf
the extracted command is passed to the parser, that tokenizes it into command_type, key and, eventually, value.
Then, it's sent to exec_cmd().
In this phase there is no network call. 
exex_cmd() enqueues the answer in the write_buf (using, e.g. strcpy or snprintf) and the write_len is increased.
So, the loop (phase 2 + 3) keeps iterating consuming every command present in the read_buf and enqueing the replies into the write_buf.

* write_buf -> network
here the read_buf doesn't contain any (entire) command left -> we are out of the parsing loop.
At this point, the write_buf contains all the replies (concatenated).
A single syscall send() is performed, to send every reply to the client waiting for them:
if everything is sent -> write_len = 0 and done
if partial send (the kernel buf of the client is full) -> memmove of remaining bytes and set EPOLLOUT

### Challenge: zero-byte problem 
We are in the EPOLLOUT state in the FSM. In this state, we are in a while(1) in which send() is called until every byte is sent or errno set to EAGAIN (buffer full). 
At the beginning, I added the break to stop iterating only in the EAGAIN branch.
I had to write a "break" also in the other branch, of course, that is if every byte was sent, because, if not, in the next iterations of the loop the call would be `send(fd, buf, 0)`, implying a deadlock (infinite loop). This is because there will never be EAGAIN (0 bytes always succeed to be sent).
With the added break, when there is nothing left to send, the loop will stop iterating. 
In other words, I learned that if the write_buf is empty, I don't have to wait that the kernel sets errno to EAGAIN (like in the EPOLLIN state). I have to explicitely break the loop and turn off EPOLLOUT.


#### Challenge: off-by-one error
n.b. the hash table in queried (the command is executed) and the answer is written in the write_buf
when I checked if there was enough space in the write_buf to write there the reply, I made a mistake. 
Indeed, if I had to write, for instance, `"OK\n"`, I supposed 3 free bytes were needed. 
So my code was:
```c
// BUG
while (clients[fd].write_len + 3 > clients[fd].write_size) {
    // realloc...
}
strcpy(clients[fd].write_buf + clients[fd].write_len, "OK\n");
clients[fd].write_len += 3;

```
The error was that strcpy() (and snprintf()) adds a teminator char `\0` at the end of the string. So, the strcpy() would have written 4 bytes, not 3. 
So the code was fixed:
```c
while (clients[fd].write_len + 3 + 1 > clients[fd].write_size) {
    // realloc...
}
strcpy(clients[fd].write_buf + clients[fd].write_len, "OK\n");
// For the network: I don't count '\0'
clients[fd].write_len += 3; 

```

#### Challenge: hash table buckets
while dynamic rehashing is not implemented yet, I decided to raise, in a bruteforce manner, the number of initial buckets (10000). 

#### Note:
to let the kernel open thousands of file simultaneously (during the test) you have to use `ulimit -n 65535`

## Future proofing
### Clean the code (refactoring)
* helper functions 
* graceful shutdown: catch Ctrl+C to clean the RAM and graceful shutdown the server

### DB improvements
#### dynamic rehashing
* problem: 
the hash table has a fixed number of buckets (e.g. 10000). When the number of keys exceeds the number of buckets, each bucket tends to be a list (due to collisions), meaning that the search time degrades from O(1) to O(n), degrading heavily performance.
* solution:
keep track of the load factor (that is #nodes / #buckets). When it exceeds a threshold (usually 0.75), the db must allocate a new array of buckets (twice as bigger as before in terms of number of buckets), rehash every single existing key and place it into the new array. 
The cost of this rehashing operation can be considered as an amortized cost, meaning that the time complexity will be constant, independently of how many keys we're dealing with.

#### Length-prefixed parser protocol
* problem:
at the moment it's implemented a delimiter-based parser: when a message enters in the system, the buffer is scanned looking for a delimiter, performing N iterations.
* solution:
to substitute the delimiter-based parsing with a length-prefixed parser, thus knowing exactly how long the command to parse is. 
In this way messages can also contain special characters because the bytes are counted and not interpreted.
#### TTL (Time To Live), e.g. SETEX key time value
* problem: 
sometimes, e.g. to preserve the space in RAM, we need that old data are evicted.
* solution:
to implement a command like SETEX (Set with Expiration), to add a field "timestamp" into the hash table nodes and to implement a lazy eviction mechanism (e.g. if I perform a GET over an expired key, just delete it and return not found), combined with an algorithm of cleaning to avoid Out Of Memory (OOM).

### Storage (persistence)
#### AOF (Append-Only File)
* problem:
if the server crashes or is restart, the RAM data are lost and therefore all the db data are permanently lost.
* solution:
to implement an AOF system. Every time the server performs a state changing action (SET, DEL), write the string of that command in a txt file place in the lower level memory (database.aof). At the start up of the server, before managing network connections, the program reads that file and performs all the commands in there, rebuilding the exact state of the hash table in RAM.

### Advanced optimization
#### circular buffers (ring buffers)
* problem:
at this stage of the implementation, every time that a command is processed (or some data are sent), it is used `memmove()` to shift the remaining bytes at the beginning of the buffer. This is expensive (O(n) , where n is the number of bytes to copy).
* solution:
to implement a circular/ring buffer for read_buf and write_buf. Using 2 indices (e.g. head and tail), the data are never physically moved. Instead, the head and tail indices are changed, moving them in a circular manner (module buf_len).
In this way, from O(n) we move to O(1) (performing just a few math ops).

#### memory arena / pool
* problem:
memory fragmentation and a lot of malloc.
At this point, for each key inserted, I use `malloc` for the node, for the key (strdup) and for the value. 
* solution:
to pre-allocate large blocks of contiguous memory at the start up of the server, therefore a memory manager module is needed.

#### multi-core / process pool
* problem:
due to the fact that the architecture is single-threaded, the server uses just one core of the CPU. the best thing to do here would be to use every core.
* solution:
to create N processes (or threads) that are "workers", each one of them having its own independent event-loop epoll. 
To avoid race conditions and RAM corruction, it will be needed to implement some Mutex or to perform "sharding" of the hash table (i.e. split the db in N independent "banks")

## [4-08-2026] - Clean the code (refactoring)
### Refactoring of main.c
Added some helper functions. 
### File Descriptor security fix
I noticed that when I called "net_accept()" I did not perform any validation on the returned file descriptor. 
Considering that the system can have at maximum MAX_CLIENTS clients, and that the index of the client in the `clients` array represents the fd number, I had to prevent the access to an index out of bound of the array. 
Therefore I placed a security check so that, if the fd >= MAX_CLIENTS -> disconnect_client() (maximum number of clients reached). 

Important thing to consider about the fd arrays: 
the index in the `clients` array represents a fd. The array goes from 0 to MAX_CLIENTS - 1. 
Therefore, we can have at maximum MAX_CLIENTS - 5 clients connected to the system. That is due to the fact that some fd are already set by default:
fd = 0, 1, 2 respectively stdin, stdout, stderr. 
fd = 3, 4 respectively listen_sock, epollfd (these are set by me at the beginning of the program). 

### Graceful shutdown
When the server is closed in a bruteforce way (e.g. Ctrl+C), I want to free everything and disconnect all the clients (closing the sockets) before shutting down the server. 
I implemented this through sigaction(), by intercepting SIGINT and SIGTERM and by using a volatile sig_atomic_t server_running var (volatile -> not to be placed in a CPU cache, because the kernel could suddenly modify it; atomic -> writing and reading this var in just one clock cycle).
When one of these 2 signals is catched, errno is set to EINTR and epoll_wait is waken up with -1. 
So, in this way, the server can finish to serve all the clients already waken up (in-flight requests), ending by freeing the allocated memory locations and closing all the opened sockets. 
Valgrind validates this strategy with 0 bytes in 0 blocks in use at exit, no leaks are possible (ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)).

## [5-08-2026] - Dynamic rehashing
Implemented, in hash_table.c, the function ht_resize() to double up the number of buckets of the ht when load factor > 0.75.
This function just allocates a new array of buckets (and frees the old one) whose size is double w.r.t the old one, and rehashes every key of the old ht inserting every one of them into the new ht. 
The function does not free any node, it just modifies pointers. 
After adding a new element into the ht, the load factor is checked and, if large enough, the ht is resized. 
Load factor $\alpha$ = #entries / #buckets. 
If $\alpha > 0.75$ the resize function is performed. 


