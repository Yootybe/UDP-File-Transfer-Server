CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

OPENSSL_CFLAGS  = $(shell pkg-config --cflags openssl)
OPENSSL_LDFLAGS = $(shell pkg-config --libs openssl)

INCLUDES = -Iinclude -Isrc/server -Isrc/client $(OPENSSL_CFLAGS)
LDFLAGS = $(OPENSSL_LDFLAGS)

BUILD_DIR = build

SERVER_SRC = src/help_funcs.cpp src/server/main.cpp
CLIENT_SRC = src/help_funcs.cpp src/client/main.cpp

SERVER_OUT = $(BUILD_DIR)/server
CLIENT_OUT = $(BUILD_DIR)/client

FILE1 = testfile_2mb.bin
FILE2 = testfile_1mb.bin

.PHONY: all clean dirs run prepare

all: dirs $(SERVER_OUT) $(CLIENT_OUT)

dirs:
	mkdir -p $(BUILD_DIR)

$(SERVER_OUT): $(SERVER_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS)

$(CLIENT_OUT): $(CLIENT_SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf $(BUILD_DIR) $(FILE1) $(FILE2)

prepare:
	dd if=/dev/urandom of=$(FILE1) bs=1M count=2
	dd if=/dev/urandom of=$(FILE2) bs=1M count=1

run: all
	@echo "Creating test files..."
	@dd if=/dev/urandom of=testfile_2mb.bin bs=1M count=2 status=none
	@dd if=/dev/urandom of=testfile_1mb.bin bs=1M count=1 status=none
	@echo "Starting server..."
	@$(SERVER_OUT) & \
	SERVER_PID=$$!; \
	sleep 1; \
	echo "Starting clients..."; \
	$(CLIENT_OUT) testfile_2mb.bin; \
	$(CLIENT_OUT) testfile_1mb.bin; \
	echo "All clients done. Stopping server (PID $$SERVER_PID)..."; \
	kill $$SERVER_PID; \
	wait $$SERVER_PID 2>/dev/null || true
