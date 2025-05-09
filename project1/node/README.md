You can simply run "make" to create the node. This node Makefile also compiles the base64.c program and index.c program, since they are both used in the node code.

Type: ./node [personal port] [dir to index] [tracker IP] [tracker port]

Note: I do not use any sort of staging directory in the case where one node retrieves information from another, neighboring node. Hence my executable only requires 4 additional arguments (excluding the executable). Moreover, since I only account for the posibility that the user enters 4 arguments (excluding the executable), if you enter two arguments (as was the case in part 2), you will receive an error.
