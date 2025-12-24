# Simple Server Client example

## Instrucitons to run
1. Set Server node IP address in "Server.cpp" and "Client.cpp"
2. Build the server and client moduldes
```
 gcc -o server Server.cpp -lrdmacm -libverbs
 gcc -o client Client.cpp -lrdmacm -libverbs
```
3. Run the server on first node
```
./server
```

4. Run the Client on the second node
```
./client
```