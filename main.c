#include <winsock2.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#define SDL_MAIN_HANDLED
#include "SDL2/SDL.h"
#include "H.h"
// gcc -o myprogram.exe main.c -lSDL2main -lSDL2 -lws2_32
#pragma comment(lib, "Ws2_32.lib")
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
        serverThreadParams params = {&connectors[0], &stop_flag, &server_initialized, &connected};
        HANDLE hThread = CreateThread(
            NULL, 0,
            server_connection_handler,
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
        clientThreadParams params = {&stop_flag, &client_initialized};
        HANDLE hThread = CreateThread(
            NULL, 0,
            client_connection_handler,
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

void client(clientThreadParams* params) {
    SOCKET s = INVALID_SOCKET;
    printf("you started a client\n");
    ASSERT(!((s = socket(AF_INET , SOCK_STREAM, 0))  == INVALID_SOCKET), 
            "Could not create socket :%d", WSAGetLastError());

    struct sockaddr_in service;
    service.sin_family = AF_INET;
    service.sin_addr.s_addr = inet_addr("192.168.1.245");
    service.sin_port = htons(8008);

    ASSERT(!(connect(s, (SOCKADDR *) &service, sizeof(service))), 
                "Could not connect to server : %d, close socket gave : %d", WSAGetLastError(), CLOSE_SOCK(s) );
    
    *(params->initialized) = 1;
    // start receive thread here
    receive_message_Params receive_params = {0,s,0};
    HANDLE hObject = CreateThread(NULL,0,receive_message_thread, &receive_params, 0, NULL);
    ASSERT(hObject, "Coulnt initialize Receiver : %d", GetLastError());  // NEED close socket ??  
    printf("Write and press enter to send message to Server");
    do {
        // PERIODICALLY SEND MESSAGE CONTAINING CLIENT'S stated
        // For now sends ping each 10ms
        sendPackage(&s, "00", "0000000000", "0");
        Sleep(1000);
    } while ( *(params->stop_flag) == 0);

    printf("close socket gave: %d", CLOSE_SOCK(s));
}

void server(serverThreadParams* params) {
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
    *(params->initialized) = 1;
    while (connectors < MAX_CONNECTORS && *(params->stop_flag) == 0 ) {
        SOCKET client = INVALID_SOCKET;
        ASSERT((client = accept(s, NULL, NULL)) != INVALID_SOCKET, 
                "Could not accept connection : %d, close socket gave : %d", WSAGetLastError(), CLOSE_SOCK(s));
        
        printf("Client Connected");
        *(params->connector_list + connectors) = client;
        connectors++;
        *(params->connected) = *(params->connected) + 1;
        //start receiver for this client
        receive_message_Params receive_params = {1,client, *(params->connected)};
        parameters[*(params->connected)-1] = receive_params;
        HANDLE hObject = CreateThread(NULL,0,receive_message_thread, &parameters[*(params->connected)-1], 0, NULL);
        ASSERT(hObject, "Coulnt initialize Receiver : %d", GetLastError()); // need close socket ??   
        connector_threads[*(params->connected)-1] = hObject;
        char msg[] = "HELLO FROM SERVER";
        send(client, &msg[0], sizeof(msg) / sizeof(msg[0]), 0);
    }
    while (*params->stop_flag == 0) {
        continue;
    }
    printf("close socket gave: %d", CLOSE_SOCK(s));
}

DWORD WINAPI server_connection_handler(LPVOID lpParam) {
    serverThreadParams* params = (serverThreadParams*)lpParam;
    server(params);
    
}

DWORD WINAPI client_connection_handler(LPVOID lpParam) {
    clientThreadParams* params = (clientThreadParams*)lpParam;
    client(params);
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