CC = gcc
CFLAGS = -Wall -Iinclude -Isrc/common

BUILD_DIR = build

SERVER_OBJ = $(BUILD_DIR)/server/main.o \
             $(BUILD_DIR)/server/stato.o \
             $(BUILD_DIR)/server/handlers.o \
             $(BUILD_DIR)/server/rete.o

CLIENT_OBJ = $(BUILD_DIR)/client/main.o \
             $(BUILD_DIR)/client/stato.o \
             $(BUILD_DIR)/client/handlers.o \
             $(BUILD_DIR)/client/rete.o

.PHONY: all clean

all: lavagna utente

lavagna: $(SERVER_OBJ)
	$(CC) $(CFLAGS) -o $@ $^

utente: $(CLIENT_OBJ)
	$(CC) $(CFLAGS) -pthread -o $@ $^

$(BUILD_DIR)/server/%.o: src/server/%.c | $(BUILD_DIR)/server
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/server/rete.o: src/common/rete.c | $(BUILD_DIR)/server
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/client/%.o: src/client/%.c | $(BUILD_DIR)/client
	$(CC) $(CFLAGS) -pthread -c -o $@ $<

$(BUILD_DIR)/client/rete.o: src/common/rete.c | $(BUILD_DIR)/client
	$(CC) $(CFLAGS) -pthread -c -o $@ $<

$(BUILD_DIR)/server $(BUILD_DIR)/client:
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR) lavagna utente
