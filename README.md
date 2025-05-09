This is a project I completed as a part of my Computer Networks course at Notre Dame!


I built a fully functional P2P file-transfering system in C. Each node in the system serves as both a server for clients and a client to other nodes, handling file indexing, search, and transfer functionalities with Base64 encoding. All nodes in the network communicate with a centralized tracker via UDP, allowing nodes to register with the tracker and retrieve network membership data. Each client can enter the following, HTTP-like commands:


HELO - Request metadata about files from the server. 

REFRESHR - Refresh the list of known nodes from the tracker. A list of nodes will be returned to the client. 

FIND xxx - Search and identify any indexed file whose name matches the specified pattern (xxx). 

FINDR n xxx - Search and identify any indexed file from a node (n) whose filename matches the specified pattern (xxx). 

GET n - Fetch and download the file noted with the specific index number (n).

GETR m n - Fetch and download the file noted with the specific index number (n) from a specific node (m).

END - A mechanism for the client to explicitly end the connection with the server.

