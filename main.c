#include <winsock2.h>
#include <stdio.h>
#include <windows.h>
#include <time.h>
#include <string.h>
#define SDL_MAIN_HANDLED
#include "SDL2/SDL.h"
// gcc -o myprogram.exe main.c -lSDL2main -lSDL2 -lws2_32
#pragma comment(lib, "Ws2_32.lib")


#define ASSERT(_e, ...) if (!(_e)) { fprintf(stderr, __VA_ARGS__); exit(1); }
#define CLOSE_SOCK(s) ((closesocket(s) == 0) ? 0 : WSAGetLastError())
#define MAX_CONNECTORS 10
#define MAX_MESSAGE_SIZE 512
#define MSG_DELAY_MILLIS 1000
/*
                TODO:
        max msg size
        server should send message:
            x,y p1 ; x,y p2 ; x,y p3
        client should parse that and update positions of all clients in their game
        client should send message:
            x,y
        server should parse that and update the position it knows from that client
        CLient displays
        Server should have a thread that accepts connections instead of it accepting itself.
            OR thread that periodically sends message to client about world state.



*/

void client(int* stop_flag, int* initialized);
void server(SOCKET *connector_list, int* stop_flag, int* initialized, int* connectedAmount);
DWORD WINAPI connection_handler(LPVOID lpParam);
DWORD WINAPI receive_message_thread(LPVOID lpParam);
int sendPackage(SOCKET* s, char* type, char* size, char* payload);

typedef struct {
    int mode; // 0 for run Client, 1 for run Server
    SOCKET* connector_list; // ignored for Client
    int* stop_flag; 
    int* initialized; //1 if server/client is done with initializing
    int* connected; //ignored for client
} connectionThreadParams;
typedef struct {
    int mode; // 0 for Client receiver, 1 for Server recevier
    SOCKET s;
    int connector_id; // ignored when used for client_receiver

} receive_message_Params;
typedef struct {
    int x, y;
    // More might come
} client_state;
/* Package protocol
    512 bytes total; 0-1 = type
    type 0 package : ping, no payload
    type 1 package : Client-Server, payload = Client State Info
        payload format: att1:val,att2:val,att3:val, ... ,attn:val
    type 2 package : Server-Client, payload = World State info (containing all clients)
        payload format: Client1(att1:val,att2:val, ... ,attn:val)Client2( ... )ClientN( ... )World( ... )
    type 3 package : UNUSED
    2-11 = size
    12-511 = payload
*/

int main(int argc, char *argv[]) {   
    WSADATA wsaData;
    ASSERT(!SDL_Init( SDL_INIT_EVERYTHING ), "Could not initialize SDL : %s", SDL_GetError());
    printf("\nInitialising Winsock...\n");
    ASSERT(!WSAStartup(MAKEWORD(2,2), &wsaData), "Failed. Error Code : %d",WSAGetLastError());
    if (argc > 1 && argv[1][0] == 's') {    // SERVER MODE
        SOCKET connectors[MAX_CONNECTORS];
        int stop_flag = 0;
        int server_initialized = 0;
        int connected = 0;
        connectionThreadParams params = {1, &connectors[0], &stop_flag, &server_initialized, &connected};
        HANDLE hThread = CreateThread(
            NULL, 0,
            connection_handler,
            &params,
            0, NULL
        );
        ASSERT(hThread, "Coulnt initialize Server : %d", GetLastError());
        char input[100];
        while(server_initialized == 0) {;}
        while (stop_flag == 0) {
            printf("Enter q to stop server, s to send messages\n");
            if (fgets(input, sizeof(input), stdin) != NULL) {
                if (input[0] == 'q' && input[1] == '\n') {
                    stop_flag = 1;
                    break;
                } else if (input[0] == 's' && input[1] == '\n') {
                    // BROADCAST A MESSAGE TO ALL SOCKETS HERE
                    printf("%d\n", connected);
                    char msg[] = "HELLO FELLOW SOCKET";
                    for (int i =0; i<connected; i++) {
                        sendto(connectors[i], &msg[0], sizeof(msg) / sizeof(msg[0]), 0, NULL, 0);
                    }
                }
            }
        }
        TerminateThread(hThread, 100);
        //server.accept() blocks the thread...
        //WaitForSingleObject(hThread, INFINITE);
    } else {                                //CLIENT MODE
        int client_initialized = 0;
        int stop_flag = 0;
        connectionThreadParams params = {0,NULL, &stop_flag, &client_initialized, NULL};
        HANDLE hThread = CreateThread(
            NULL, 0,
            connection_handler,
            &params,
            0, NULL
        );
        ASSERT(hThread, "Coulnt initialize Client : %d", GetLastError());
        char input[100];
        while (client_initialized ==0) {;}
        while (stop_flag == 0) {
            // printf("Enter q to stop client\n");
            // if (fgets(input, sizeof(input), stdin) != NULL) {
            //     if (input[0] == 'q' && input[1] == '\n') {
            //         stop_flag = 1;
            //     }
            // }
        }
        
        WaitForSingleObject(hThread, INFINITE);
    }
    
    WSACleanup();
    SDL_Quit();
}

void client(int* stop_flag, int* initialized) {
    SOCKET s = INVALID_SOCKET;
    printf("you started a client\n");
    ASSERT(!((s = socket(AF_INET , SOCK_STREAM, 0))  == INVALID_SOCKET), 
            "Could not create socket :%d", WSAGetLastError());

    struct sockaddr_in service;
    service.sin_family = AF_INET;
    service.sin_addr.s_addr = inet_addr("192.168.1.9");
    service.sin_port = htons(8008);

    ASSERT(!(connect(s, (SOCKADDR *) &service, sizeof(service))), 
                "Could not connect to server : %d, close socket gave : %d", WSAGetLastError(), CLOSE_SOCK(s) );
    
    *initialized = 1;
    // start receive thread here
    receive_message_Params params = {0,s,0};
    HANDLE hObject = CreateThread(NULL,0,receive_message_thread, &params, 0, NULL);
    ASSERT(hObject, "Coulnt initialize Receiver : %d", GetLastError());  // NEED close socket ??  
    printf("Write and press enter to send message to Server");
    do {
        // PERIODICALLY SEND MESSAGE CONTAINING CLIENT'S stated
        // For now sends ping each 10ms
        sendPackage(&s, "00", "0000000000", "0");
        Sleep(1000);
    } while ( *stop_flag == 0);

    printf("close socket gave: %d", CLOSE_SOCK(s));
}

void server(SOCKET *connector_list, int* stop_flag, int* server_initialized, int* connectedAmount) {
    SOCKET s = INVALID_SOCKET;
    int connectors = 0;
    HANDLE connector_threads[MAX_CONNECTORS];
    receive_message_Params parameters[MAX_CONNECTORS];
    printf("you started a server\n");
    ASSERT(!((s = socket(AF_INET , SOCK_STREAM, 0))  == INVALID_SOCKET), 
            "Could not create socket :%d", WSAGetLastError());
    struct sockaddr_in service;
    service.sin_family = AF_INET;
    service.sin_addr.s_addr = inet_addr("192.168.1.245");
    service.sin_port = htons(8008);
    // BIND SOCKET AND SET TO LISTEN MODE
    ASSERT(!(bind(s, (SOCKADDR *) &service, sizeof(service))), 
            "Could not bind socket : %d, close socket gave : %d", WSAGetLastError(), CLOSE_SOCK(s));
    ASSERT(!(listen(s, SOMAXCONN)), 
            "Could not listen to incoming connections : %d, close socket gave : %d", WSAGetLastError(), CLOSE_SOCK(s));
    *server_initialized = 1;
    while (connectors < MAX_CONNECTORS && *stop_flag == 0 ) {
        SOCKET client = INVALID_SOCKET;
        ASSERT((client = accept(s, NULL, NULL)) != INVALID_SOCKET, 
                "Could not accept connection : %d, close socket gave : %d", WSAGetLastError(), CLOSE_SOCK(s));
        
        printf("Client Connected");
        *(connector_list + connectors) = client;
        connectors++;
        *connectedAmount = *connectedAmount + 1;
        //start receiver for this client
        receive_message_Params params = {1,client, *connectedAmount};
        parameters[*connectedAmount-1] = params;
        HANDLE hObject = CreateThread(NULL,0,receive_message_thread, &parameters[*connectedAmount-1], 0, NULL);
        ASSERT(hObject, "Coulnt initialize Receiver : %d", GetLastError()); // need close socket ??   
        connector_threads[*connectedAmount-1] = hObject;
        char msg[] = "HELLO FROM SERVER";
        send(client, &msg[0], sizeof(msg) / sizeof(msg[0]), 0);
    }
    while (*stop_flag == 0) {
        continue;
    }
    printf("close socket gave: %d", CLOSE_SOCK(s));
}

DWORD WINAPI connection_handler(LPVOID lpParam) {
    connectionThreadParams* params = (connectionThreadParams*)lpParam;
    if (params->mode == 1) {
        //SERVER MODE
        server(params->connector_list, params->stop_flag, params->initialized, params->connected);
    } else {
        //CLIENT MODE
        client(params->stop_flag, params->initialized);
    }
}

DWORD WINAPI receive_message_thread(LPVOID lpParam) {

    receive_message_Params* params = (receive_message_Params*)lpParam;
    char recvbuf[MAX_MESSAGE_SIZE];
    int res;
    do {
        res = recvfrom(params->s, recvbuf, MAX_MESSAGE_SIZE,0, NULL, NULL);
        if (res > 0) {
            if (params->mode == 0 ){
                printf("Received : %.*s\n",res, recvbuf);
            } else {
                if (recvbuf[0] == '0' && recvbuf[1] == '0')
                printf("received ping from client: %d\n", params->connector_id);
            }
        }
    } while (1);
}



int sendPackage(SOCKET* s, char* type, char* size, char* payload) {
    char * package = (char*)malloc((512 + 1 * sizeof(char)));
    strcpy(package, type);
    strcat(package, size);
    strcat(package, payload);
    send(*s,package,512,0);
    free(package);
}