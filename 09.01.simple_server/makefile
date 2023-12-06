SERVER_FLAGS = -lwayland-server
CLIENT_FLAGS = -lwayland-client

all: server client

server: 
	gcc server.c -o server $(SERVER_FLAGS)

client:
	gcc connect.c -o client $(CLIENT_FLAGS)

clean:
	rm server client
