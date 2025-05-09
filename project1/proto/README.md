The three subdirectories in proto include:

1. project1/proto/index/index.c - Traverses a given directory (and its subdirectories) to create an index of all filenames
2. project1/proto/base64/base64_encode.c - Given a file name, encodes the contents of the file into base64 encoding and prints the output to stdout and to a file
3. project1/proto/nettest/client.c and server.c - Code for a simple client and server in C.

Explanation of Master Makefile:

My master makefile includes three main targets: base64, index, and nettest. These are **not** the actual executables for executing the C programs named above. These are just targets to make those executables. By typing "make" while in project1/proto, you will create index/index-test, base64/base64-test, nettest/client-test, and nettest/server-test. Likewise, to clean up all executables while in project1/proto, type "make clean."
