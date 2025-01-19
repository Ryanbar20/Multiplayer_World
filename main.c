#include <winsock2.h>
#include <stdio.h>
#include <windows.h>
#include <time.h>
// gcc .\implementation.c -o .\implementation -lws2_32
#pragma comment(lib, "Ws2_32.lib")

#define MAX_CONNECTORS 10


void client(int* stop_flag, int* initialized);
void server(SOCKET *connector_list, int* stop_flag, int* initialized, int* connectedAmount);
DWORD WINAPI connection_handler(LPVOID lpParam);
DWORD WINAPI receive_message_thread(LPVOID lpParam);

/////////////////////////////////////////// MULTIPLE CLIENTS DOESNT WORK ANYMORE ///////////////////////////////////
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

int main(int argc, char *argv[]) {   
    WSADATA wsaData;
    printf("\nInitialising Winsock...\n");
    if (WSAStartup(MAKEWORD(2,2),&wsaData) != 0) {
        printf("Failed. Error Code : %d",WSAGetLastError());
        return 1;
    }
    if (argc > 1 && argv[1][0] == 's') {
        //SERVER MODE
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
        //server.accept() blocks the thread...
        //WaitForSingleObject(hThread, INFINITE);
    } else {
        //CLIENT MODE
        int client_initialized = 0;
        int stop_flag = 0;
        connectionThreadParams params = {0,NULL, &stop_flag, &client_initialized, NULL};
        HANDLE hThread = CreateThread(
            NULL, 0,
            connection_handler,
            &params,
            0, NULL
        );
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
}

void client(int* stop_flag, int* initialized) {
    SOCKET s = INVALID_SOCKET;
    printf("you started a client\n");
    if ((s = socket(AF_INET , SOCK_STREAM, 0)) == INVALID_SOCKET) {
        printf("Could not create socket : %d", WSAGetLastError());
        return;
    }

    struct sockaddr_in service;
    service.sin_family = AF_INET;
    service.sin_addr.s_addr = inet_addr("192.168.1.245");
    service.sin_port = htons(8008);

    if (connect(s, (SOCKADDR *) &service, sizeof(service)) == SOCKET_ERROR) {
        printf("Could not connect to server : %d", WSAGetLastError());
        return;
    }
    *initialized = 1;
    // start receive thread here
    receive_message_Params params = {0,s,0};
    HANDLE hObject = CreateThread(NULL,0,receive_message_thread, &params, 0, NULL);
    printf("Write and press enter to send message to Server");
    do {
        char buf[100];
        if (fgets(buf, 100, stdin) != NULL) {
            printf("sending...");
            send(s,&buf[0],100,0);
        }
    } while ( *stop_flag == 0);

    if (closesocket(s) == SOCKET_ERROR) {
        wprintf(L"closesocket function failed with error %d\n", WSAGetLastError());
        WSACleanup();
        return;
    }
}
void server(SOCKET *connector_list, int* stop_flag, int* server_initialized, int* connectedAmount) {
    SOCKET s = INVALID_SOCKET;
    int connectors = 0;
    printf("you started a server\n");
    if ((s = socket(AF_INET , SOCK_STREAM, 0)) == INVALID_SOCKET) {
        printf("Could not create socket : %d", WSAGetLastError());
        return;
    }
    struct sockaddr_in service;
    service.sin_family = AF_INET;
    service.sin_addr.s_addr = inet_addr("192.168.1.245");
    service.sin_port = htons(8008);
    if (bind(s , (SOCKADDR *) &service , sizeof(service) ) != 0) {
        printf("Could not bind socket : %d", WSAGetLastError());
        closesocket(s);
        return;
    } else {
        printf("Bind success!!!\n");
    }

    if (listen(s, SOMAXCONN) != 0) {
        printf("Could not listen to incoming connections : %d", WSAGetLastError());
        return;
    }
    *server_initialized = 1;
    while (connectors < MAX_CONNECTORS && *stop_flag == 0 ) {
        SOCKET client = INVALID_SOCKET;
        client = accept(s, NULL, NULL);
        if (client == INVALID_SOCKET) {
            printf("Couldnt accept connection : %d", WSAGetLastError());
            closesocket(s);
            return;
        } else {
            printf("Client Connected");
            *(connector_list + connectors) = client;
            connectors++;
            *connectedAmount = *connectedAmount + 1;
            //start receiver for this client
            receive_message_Params params = {1,client, *connectedAmount};
            HANDLE hObject = CreateThread(NULL,0,receive_message_thread, &params, 0, NULL);
            //should store the handle ... 
        }
        char msg[] = "HELLO FELLOW SOCKET";
        send(client, &msg[0], sizeof(msg) / sizeof(msg[0]), 0);
    }
    while (*stop_flag == 0) {
        continue;
    }
    if (closesocket(s) == SOCKET_ERROR) {
        wprintf(L"closesocket function failed with error %d\n", WSAGetLastError());
        WSACleanup();
        return;
    }
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
    char recvbuf[512];
    int res;
    do {
        res = recv(params->s, recvbuf, 512,0);
        if (res > 0) {
            if (params->mode == 0 ){
                printf("Received : %.*s\n",res, recvbuf);
            } else {
                printf("Received from client %d \t\t : %.*s\n",params->connector_id, res, recvbuf);
            }
        }
    } while (1);
}