This is a project I completed as a part of my Computer Networks course at Notre Dame!

I built a fully functional P2P file-transfering system in C. Each node in the system serves as both a server for clients and a client to other nodes, handling file indexing, search, and transfer functionalities with Base64 encoding. All nodes in the network communicate with a centralized tracker via UDP, allowing nodes to register with the tracker and retrieve network membership data. Each client can enter the following, HTTP-like commands:





GETR: GETR adapts the GET command from Part 2 to instruct the server to GET content from a remote node.  GETR will have two parameters, the identifier of the node to retrieve the file from and the number of the file to retrieve from that node.  From the perspective of the client, it will still receive a Base64 encoded file that will be saved in the specified download directory.  
FINDR: FINDR is a similar modification to the FIND command from Part 2 with the addition of the number of the remote node.  FINDR will have two parameters, the identifier of the node from which to do the find and the pattern on which to search.  From the perspective of the client, it will receive a similar semi-colon delimited list.
REFRESHR: The REFRESHR is a new command that requests the node to refresh the list of known nodes from the tracker.  A list of nodes will be provided as part of the response to the client.  
