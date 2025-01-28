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
        client_state state = {0, 0};
        clientThreadParams params = {&stop_flag, &client_initialized, &state};
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
            printf("Enter q to stop client\n");
            if (fgets(input, sizeof(input), stdin) != NULL) {
                if (input[0] == 'q' && input[1] == '\n') {
                    stop_flag = 1;
                    break;
                // movement inbetween -10 <= x <= 10 and -10 <= y <= 10
                } else if (input[0] == 'w' && input[1] == '\n' && state.y != 10) {
                    state.y += 1;
                } else if (input[0] == 'a' && input[1] == '\n' && state.x != -10) {
                    state.x -= 1;
                } else if (input[0] == 's' && input[1] == '\n' && state.y != -10) {
                    state.y -= 1;
                } else if (input[0] == 'd' && input[1] == '\n' && state.x != 10) {
                    state.x += 1;
                } else {
                    continue;
                }
                printf("%d, %d = current state", state.x, state.y);
            }
        }
        
        WaitForSingleObject(hThread, INFINITE);
    }
    
    WSACleanup();
    SDL_Quit();
}

void client(clientThreadParams* params) {
    SOCKET s = INVALID_SOCKET;
    printf("you started a client\n");
    client_state last_state = {(params->state->x), (params->state->y)};
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
    client_receiver_Params receive_params = {s};
    HANDLE hObject = CreateThread(NULL,0,client_receive_thread, &receive_params, 0, NULL);
    ASSERT(hObject, "Coulnt initialize Receiver : %d", GetLastError());  // NEED close socket ??  
    printf("Write and press enter to send message to Server");
    do {
        if (last_state.x != params->state->x || last_state.y != params->state->y) {
            // send data to server
            last_state.x = params->state->x; // maybe paralellism issue
            last_state.y = params->state->y; // maybe paralellism issue
            char x[4] = "000";
            char y[4] = "000";
            char tot[7] = "000000";
            snprintf(x,sizeof(x),"%03d", last_state.x);
            snprintf(y,sizeof(y), "%03d", last_state.y);
            snprintf(tot, sizeof(tot), "%s%s", x, y);
            sendPackage(&s, "01", "0000000006", tot);
        } else { // send ping
            sendPackage(&s, "00", "0000000000", "0");
        }
        Sleep(1000);
    } while ( *(params->stop_flag) == 0);

    printf("close socket gave: %d", CLOSE_SOCK(s));
}

void server(serverThreadParams* params) {
    SOCKET s = INVALID_SOCKET;
    int connectors = 0;
    HANDLE connector_threads[MAX_CONNECTORS];
    server_receiver_Params parameters[MAX_CONNECTORS];
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
        server_receiver_Params receive_params = {client, *(params->connected)};
        parameters[*(params->connected)-1] = receive_params;
        HANDLE hObject = CreateThread(NULL,0,server_receive_thread, &parameters[*(params->connected)-1], 0, NULL);
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

DWORD WINAPI server_receive_thread(LPVOID lpParam) {
    server_receiver_Params* params = (server_receiver_Params*)lpParam;
    char recvbuf[MAX_MESSAGE_SIZE];
    int res;
    do {
        res = recvfrom(params->s, recvbuf, MAX_MESSAGE_SIZE,0, NULL, NULL);
        if (res > 0 && res <=MAX_MESSAGE_SIZE) {
            if (recvbuf[0] == '0' && recvbuf[1] == '0') {
                        printf("received ping from client: %d\n", params->connector_id);
                    } else if (recvbuf[0] == '0' && recvbuf[1] == '1') {
                        printf("%s", recvbuf);
                        //TODO ...................................................................................
                    }
        } else if (res == 0) {
            // handle connection close
        } else {
            // handle error in connection
        }
    } while (1);
}
DWORD WINAPI client_receive_thread(LPVOID lpParam) {
    client_receiver_Params* params = (client_receiver_Params*)lpParam;
    char recvbuf[MAX_MESSAGE_SIZE];
    int res;
    do {
        res = recvfrom(params->s, recvbuf, MAX_MESSAGE_SIZE,0,NULL,NULL);
        if (res > 0 && res <= MAX_MESSAGE_SIZE) {
            printf("Received : %.*s", res, recvbuf);
        } else if (res == 0) {
            // handle connection close
        } else {
            // handle error in receiver
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