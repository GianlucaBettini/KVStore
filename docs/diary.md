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
