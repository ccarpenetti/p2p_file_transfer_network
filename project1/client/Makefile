CC=gcc
LIB=../proto/base64/libb64-1.2/src/libb64.a
CFLAGS=-Ilibb64-1.2/include

all: client

client: client.o base64.o
	$(CC) -o $@ $^ $(LIB) $(CFLAGS)

client.o: client.c
	$(CC) -c $< -o $@ $(CFLAGS)

base64.o: ../proto/base64/base64.c
	$(CC) -c $< -o $@ $(CFLAGS)

clean:
	rm -f client client.o base64.o



