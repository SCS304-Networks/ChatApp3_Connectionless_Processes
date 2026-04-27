CC = gcc
CFLAGS = -Wall -Wextra

# Server source files
SERVER_SRC = server/server.c server/auth.c server/chat.c server/utils.c server/network_server.c
SERVER_OUT = server/server

# Client source files
CLIENT_SRC = client/client.c client/ui.c client/network_client.c
CLIENT_OUT = client/client

.PHONY: all clean server client

all: server client

server:
	$(CC) $(CFLAGS) -o $(SERVER_OUT) $(SERVER_SRC)
	@echo "[+] Server compiled successfully: $(SERVER_OUT)"

client:
	$(CC) $(CFLAGS) -o $(CLIENT_OUT) $(CLIENT_SRC)
	@echo "[+] Client compiled successfully: $(CLIENT_OUT)"

clean:
	rm -f $(SERVER_OUT) $(CLIENT_OUT)
	@echo "[+] Cleaned build artifacts"
