CC=gcc
LIB=../proto/base64/libb64-1.2/src/libb64.a
CFLAGS=-Ilibb64-1.2/include

all: node

node: node.o base64.o index.o
	$(CC) -o $@ $^ $(LIB) $(CFLAGS)

node.o: node.c
	$(CC) -c $< -o $@ $(CFLAGS) 

base64.o: ../proto/base64/base64.c
	$(CC) -c $< -o $@ $(CFLAGS) 

index.o: ../proto/index/index.c
	$(CC) -c $< -o $@ $(CFLAGS) 

clean:
	rm -f  node node.o base64.o index.o

