CC=gcc

all: server-test client-test

server-test: server.c ../index/index.c 
	$(CC) -o $@ $^

client-test: client.c
	$(CC) -o $@ $^

clean:
	rm -f server-test client-test
