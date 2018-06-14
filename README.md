# IPCAssignment
Implements inter-process communication between a 'server' and a 'client'. Server listens for incoming connections. Client establishes 
connection to a pipe, if one exists, and the requests for meta data. This data contains the types and functions avaiable on the server. For 
ease of communication, json notation is used on both client and server. Data is transferred to and fro as bytes(ubjson). 

The following type and its functions are registered on the client.  
type : point  
       -get_x - (Gets x component of point) Synchronous  
       -get_y - (Gets y component of point) Synchronous  
       -move - (Moves point by given x and y values) Synchronous  
       -slow_add - (Adds both components and returns single value) Aysnchronous (For demonstrations purposes, just calls Sleep(2000) underneath)  
       

Client is allowed to pick 5 options  
1. Get available types - prints available types (only point) (No communication with server)
2. Get existing objects - prints names of existing objects (No communication with server)
3. Create new object - Asks for name, and then creates new object on the server which is then returned as bytes and stored in a buffer (Communication with server)  
4. Operate on existing object - First asks for type of object to operate on, then asks name of object to operate on. Finally the function name is asked, after which the user is prompted for input (as necessary). Information is sent to the server, where the function is called on the object. If return type is the object itself, the source object is modifed, else it is printed out to stdout(only trivial types used here) (Communication with server)  
5. Close connection - Closes the connection.  

Only 1 or 2 can be picked when Asynchronous operation is under way.

# Requirements
C++ 14 or later  
Windows


# Dependencies
Both projects use header only json library(nlohmann/json)

