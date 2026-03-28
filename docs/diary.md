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
