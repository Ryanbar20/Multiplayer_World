#ifndef HEADER_H
#define HEADER_H

#include <winsock2.h>
#define ASSERT(_e, ...) if (!(_e)) { fprintf(stderr, __VA_ARGS__); exit(1); }
#define CLOSE_SOCK(s) ((closesocket(s) == 0) ? 0 : WSAGetLastError())
#define MAX_CONNECTORS 10
#define MAX_MESSAGE_SIZE 512
#define MSG_DELAY_MILLIS 1000


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
} clientThreadParams;

typedef struct {
    int mode; // 0 for Client receiver, 1 for Server recevier
    SOCKET s;
    int connector_id; // ignored when used for client_receiver

} receive_message_Params;

// game typedefs
typedef struct {
    int x, y;
    // More might come
} client_state;



//function declarations
DWORD WINAPI server_connection_handler(LPVOID lpParam);
DWORD WINAPI client_connection_handler(LPVOID lpParam);
void client(clientThreadParams* params);
void server(serverThreadParams* params);
DWORD WINAPI receive_message_thread(LPVOID lpParam);
int sendPackage(SOCKET* s, char* type, char* size, char* payload);

#endif