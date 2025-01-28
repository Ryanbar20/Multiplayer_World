#ifndef HEADER_H
#define HEADER_H

#include <winsock2.h>
#define ASSERT(_e, ...) if (!(_e)) { fprintf(stderr, __VA_ARGS__); exit(1); }
#define CLOSE_SOCK(s) ((closesocket(s) == 0) ? 0 : WSAGetLastError())
#define MAX_CONNECTORS 10
#define MAX_MESSAGE_SIZE 512
#define MSG_DELAY_MILLIS 1000


// game typedefs
typedef struct {
    int x; // More might come
    int y;
} client_state;

// networking typedefs
typedef struct {
    SOCKET* connector_list; // list of all connected sockets
    int* stop_flag; // 1 if stop
    int* initialized; // 1 if done initializing
    int*connected; // amount of connected sockets
} serverThreadParams;

typedef struct {
    int* stop_flag; // 1 if stop
    int* initialized; // 1 if initialized
    client_state* state;
} clientThreadParams;

typedef struct {
    SOCKET s;
    int connector_id; // id of client that is listened to
} server_receiver_Params;

typedef struct {
    SOCKET s;
} client_receiver_Params;


//function declarations
DWORD WINAPI server_connection_handler(LPVOID lpParam);
DWORD WINAPI client_connection_handler(LPVOID lpParam);
void client(clientThreadParams* params);
void server(serverThreadParams* params);
int sendPackage(SOCKET* s, char* type, char* size, char* payload);
DWORD WINAPI server_receive_thread(LPVOID lpParam);
DWORD WINAPI client_receive_thread(LPVOID lpParam);
#endif