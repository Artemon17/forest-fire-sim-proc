CC       = gcc
CFLAGS   = -std=c11 -Wall -Wextra -Wpedantic -O2 -g -Iinclude \
           -D_POSIX_C_SOURCE=200809L -pthread
LDFLAGS  = -pthread -lrt

BUILD_DIR = build
SRC_DIR   = src
TEST_DIR  = tests

TEST_SHM = $(BUILD_DIR)/test_shm
TEST_SHM_SRCS = $(TEST_DIR)/test_shm.c $(SRC_DIR)/shm.c

all: $(TEST_SHM)

$(TEST_SHM): $(TEST_SHM_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean