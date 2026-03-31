# --- Variables Setup ---
CC = gcc # Sets the compiler to use (GNU C Compiler)

# CFLAGS are the flags passed to the compiler:
# -Wall -Wextra: Turn on almost all warnings to catch bugs early.
# -Werror: Treats warnings as errors (forces to fix them to compile).
# -g: Includes debugging information to be able to use GDB later.
# -std=c11: Forces the compiler to use the C11 standard of the C language.
# -D_POSIX_C_SOURCE=200809L: Unlocks specific POSIX (Unix/Linux) functions.
CFLAGS = -Wall -Wextra -Werror -g -std=c11 -D_POSIX_C_SOURCE=200809L

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
.PHONY: all clean asan

# --- Build Rules ---

# 'all' is the default rule that runs when you just type 'make'.
# It depends on $(EXEC), meaning it will trigger the $(EXEC) rule below.
all: $(EXEC)

# This rule builds the final executable. It requires all the .o files to exist first.
$(EXEC): $(OBJ)
# $@ stands for the target name (kvstore).
# $^ stands for all the dependencies (the .o files).
# So this translates to: gcc <flags> -o kvstore main.o db.o <linker_flags>
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# This is a generic rule to build a .o file from a .c file.
%.o: %.c
# $< stands for the first dependency (the .c file).
# This translates to: gcc <flags> -c main.c -o main.o
	$(CC) $(CFLAGS) -c $< -o $@


# --- Special Targets ---

# The 'asan' target builds your program with AddressSanitizer.
asan: CFLAGS += -fsanitize=address -fno-omit-frame-pointer # Adds ASan compiler flags
asan: LDFLAGS += -fsanitize=address # Adds ASan linker flags
asan: clean $(EXEC) # Cleans old files, then builds
# Renames the output to 'kvstore_asan'
	mv $(EXEC) $(EXEC)_asan

# The 'clean' target deletes all generated files.
clean:
	rm -f $(OBJ) $(EXEC) $(EXEC)_asan

# --- Testing ---

TEST_EXEC = run_tests

TEST_SRC = tests/test_hash_table.c hash_table.c 

test:
		$(CC) $(CFLAGS) $(TEST_SRC) -o $(TEST_EXEC) 
		./$(TEST_EXEC)

clean_tests:
		rm -f $(TEST_EXEC)