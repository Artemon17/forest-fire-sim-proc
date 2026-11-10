CC       = gcc
CFLAGS   = -std=c11 -Wall -Wextra -Wpedantic -O2 -g -Iinclude \
           -D_POSIX_C_SOURCE=200809L -pthread
LDFLAGS  = -pthread -lrt

BUILD_DIR = build
SRC_DIR   = src
TEST_DIR  = tests

COMMON_SRCS = $(SRC_DIR)/shm.c \
              $(SRC_DIR)/sync.c \
              $(SRC_DIR)/sync_mutex.c \
              $(SRC_DIR)/sync_barrier.c \
              $(SRC_DIR)/process.c

MODEL_SRCS = $(SRC_DIR)/log_shared.c \
             $(SRC_DIR)/fire.c \
             $(SRC_DIR)/sensors.c \
             $(SRC_DIR)/messages.c \
             $(SRC_DIR)/center.c \
             $(SRC_DIR)/teams.c \
             $(SRC_DIR)/render.c \
             $(SRC_DIR)/term.c \
             $(SRC_DIR)/config.c \
             $(SRC_DIR)/map.c \
             $(SRC_DIR)/main_common.c

all: \
    $(BUILD_DIR)/fire-fork-named \
    $(BUILD_DIR)/fire-fork-unnamed \
    $(BUILD_DIR)/fire-main \
    $(BUILD_DIR)/fire-sensors \
    $(BUILD_DIR)/fire-teams \
    $(BUILD_DIR)/fire-spread \
    $(BUILD_DIR)/test_shm \
    $(BUILD_DIR)/test_sync-named      $(BUILD_DIR)/test_sync-unnamed \
    $(BUILD_DIR)/test_barrier-named   $(BUILD_DIR)/test_barrier-unnamed \
    $(BUILD_DIR)/test_mutex-named     $(BUILD_DIR)/test_mutex-unnamed

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/fire-fork-named: $(SRC_DIR)/main_fork_named.c $(COMMON_SRCS) $(MODEL_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/fire-fork-unnamed: $(SRC_DIR)/main_fork_named.c $(COMMON_SRCS) $(MODEL_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DUSE_UNNAMED -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/fire-main: $(SRC_DIR)/fire_main.c $(COMMON_SRCS) $(MODEL_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/fire-sensors: $(SRC_DIR)/fire_sensors.c $(COMMON_SRCS) $(MODEL_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/fire-teams: $(SRC_DIR)/fire_teams.c $(COMMON_SRCS) $(MODEL_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/fire-spread: $(SRC_DIR)/fire_spread.c $(COMMON_SRCS) $(MODEL_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/test_shm: $(TEST_DIR)/test_shm.c $(COMMON_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/test_sync-named: $(TEST_DIR)/test_sync.c $(COMMON_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/test_sync-unnamed: $(TEST_DIR)/test_sync.c $(COMMON_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DUSE_UNNAMED -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/test_barrier-named: $(TEST_DIR)/test_barrier.c $(COMMON_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/test_barrier-unnamed: $(TEST_DIR)/test_barrier.c $(COMMON_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DUSE_UNNAMED -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/test_mutex-named: $(TEST_DIR)/test_mutex.c $(COMMON_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/test_mutex-unnamed: $(TEST_DIR)/test_mutex.c $(COMMON_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -DUSE_UNNAMED -o $@ $^ $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean