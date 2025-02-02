#ifndef HEADER_H
#define HEADER_H

#include <winsock2.h>
#define ASSERT(_e, ...) if (!(_e)) { fprintf(stderr, __VA_ARGS__); exit(1); }
#define CLOSE_SOCK(s) ((closesocket(s) == 0) ? 0 : WSAGetLastError())
#define MAX_CONNECTORS 10
#define MAX_MESSAGE_SIZE 512
#define MSG_DELAY_MILLIS 1000
#define TYPE_FIELD_CHARS 2
#define LENGTH_FIELD_CHARS 10
#define PAYLOAD_CHARS 500
#define HEIGHT_WINDOW 1000
#define WIDTH_WINDOW 1000
#define FPS_CAP 60
#define FRAME_TIME (1000 / FPS_CAP)


// game typedefs
typedef struct {
    int x; // More might come
    int y;
} client_state;

typedef struct world_state {
    client_state clients[MAX_CONNECTORS];
    // maybe some params for world state
} world_state;
// networking typedefs
typedef struct {
    SOCKET* connector_list; // list of all connected sockets
    int* stop_flag; // 1 if stop
    int* initialized; // 1 if done initializing
    int*connected; // amount of connected sockets
    world_state* world;
} serverThreadParams;

typedef struct clientThreadparams {
    int* stop_flag; // 1 if stop
    int* initialized; // 1 if initialized
    client_state* state;
    world_state* world;
} clientThreadParams;

typedef struct server_receiver_Params {
    SOCKET s;
    int connector_id; // id of client that is listened to
    world_state* world;
} server_receiver_Params;

typedef struct {
    SOCKET s;
    world_state* world;
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