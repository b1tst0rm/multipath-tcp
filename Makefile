CC = gcc

all: clean server client

server:
	gcc -o server mptcp-server.c

client:
	gcc -o client mptcp-client.c

clean:
	$(RM) server client
