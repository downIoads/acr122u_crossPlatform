# Compiler and flags
CC = clang
CFLAGS = -Wall -Wextra -std=c99
LDFLAGS = -framework PCSC

# Source files and output
SRC = main.c mifare-classic.c
OBJ = $(SRC:.c=.o)
TARGET = main

# Default rule
all: $(TARGET)

# Linking the executable
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Compiling object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean rule
clean:
	rm -f $(OBJ) $(TARGET)

# Phony targets
.PHONY: all clean
