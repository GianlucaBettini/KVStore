# --- Variables Setup ---
CC = gcc # Sets the compiler to use (GNU C Compiler)

# CFLAGS are the flags passed to the compiler:
# -Wall -Wextra: Turn on almost all warnings to catch bugs early.
# -Werror: Treats warnings as errors (forces to fix them to compile).
# -std=c11: Forces the compiler to use the C11 standard of the C language.
# -D_POSIX_C_SOURCE=200809L: Unlocks specific POSIX (Unix/Linux) functions.
CFLAGS = -Wall -Wextra -Werror -std=c11 -D_POSIX_C_SOURCE=200809L


# LDFLAGS are flags passed to the linker
LDFLAGS = 

# Finds every single file ending in .c in the current folder.
SRC = $(wildcard *.c)

# This takes the SRC list, and replaces the '.c' extension with '.o'. 
# If SRC is "main.c db.c", OBJ becomes "main.o db.o".
OBJ = $(SRC:.c=.o)

# The name of the final runnable program
EXEC = kvstore

# .PHONY tells Make that these are commands, not actual file names.
# This prevents errors if you accidentally create a file named "clean" or "all".
.PHONY: all release debug asan clean test_e2e 

# default target
all: release

# --- Build modes ---

# 1. RELEASE: Max optimization (-O3), no debug
release: CFLAGS += -O3
release: $(EXEC)

# 2. DEBUG: Zero optimization (-O0), debug flag (-g).
# -g: Includes debugging information to be able to use GDB later.
debug: CFLAGS += -g -O0
debug: $(EXEC)

# 3. ASAN: Debug with Address Sanitizer on.
asan: CFLAGS += -g -O0 -fsanitize=address -fno-omit-frame-pointer
asan: LDFLAGS += -fsanitize=address
asan: $(EXEC)
		mv $(EXEC) $(EXEC)_asan

# --- Core build rules ---

$(EXEC): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@


# --- Cleanup ---

clean: 
		rm -f $(OBJ) $(EXEC) $(EXEC)_asan



# --- Testing ---

# Integration test and benchmark
# N.B. ./kvstore must be already in execution 
test_e2e:
	@echo "=== Start Edge Cases ==="
	python3 test/test_edge_cases.py
	@echo "\n=== Start Benchmark CRUD ==="
	go run test/benchmark/benchmark_crud.go
	@echo "\n=== Start Benchmark Pipelining ==="
	go run test/benchmark/benchmark_pipelining.go
	@echo "\n=== Start Benchmark Ping-Pong ==="
	go run test/benchmark/benchmark_pingpong.go