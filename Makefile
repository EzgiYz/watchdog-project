# Compiler and compiler flags
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2 -Iinclude

# Executable names
TARGET = watchdog
WORKER = worker

# Watchdog source files
SOURCES = src/main.c \
          src/process_manager.c \
          src/signal_manager.c \
          src/logger.c

# Object files
OBJECTS = $(SOURCES:.c=.o)
DEPS = $(OBJECTS:.o=.d)

# Default target
all: $(TARGET) $(WORKER)

# Build watchdog executable
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $(TARGET)

# Compile source files into object files
src/%.o: src/%.c
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# Build worker executable
$(WORKER): workers/worker.c
	$(CC) $(CFLAGS) $< -o $@

# Run test scenarios
test: all
	bash tests/test_scenarios.sh

# Remove generated build files
clean:
	rm -f $(OBJECTS) $(DEPS) $(TARGET) $(WORKER)

# Include header dependencies
-include $(DEPS)

# Declare non-file targets
.PHONY: all clean test