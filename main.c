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
DONE    max msg size
        server should send message:
            x,y p1 ; x,y p2 ; x,y p3
        client should parse that and update positions of all clients in their game
DONE    client should send message:
            x,y
        server should parse that and update the position it knows from that client
        CLient displays



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
    printf("initializing winsock and SDL");
    ASSERT(!SDL_Init( SDL_INIT_EVERYTHING ), "Could not initialize SDL : %s", SDL_GetError());
    SDL_Window* window = SDL_CreateWindow("SDL Window",
                                            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 
                                            WIDTH_WINDOW, HEIGHT_WINDOW, SDL_WINDOW_SHOWN
                                        );
    ASSERT(window != NULL, "Could not create window: %s", SDL_GetError());


    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == NULL) {
        printf("SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    ASSERT(!WSAStartup(MAKEWORD(2,2), &wsaData), "Failed. Error Code : %d",WSAGetLastError());



    if (argc > 1 && argv[1][0] == 's') {    // SERVER MODE

        //setup & start server
        SOCKET connectors[MAX_CONNECTORS];
        int stop_flag = 0, server_initialized =0, connected = 0;
        world_state world;
        for (int i =0; i < MAX_CONNECTORS; i++) {
            world.clients[i].x = -11;
            world.clients[i].y = -11;
        }

        serverThreadParams params = {&connectors[0], &stop_flag, &server_initialized, &connected, &world};
        HANDLE hThread = CreateThread(
            NULL, 0,
            server_connection_handler,
            &params,
            0, NULL
        );
        ASSERT(hThread, "Coulnt initialize Server : %d", GetLastError());
        // wait for server initialization
        while(server_initialized == 0) {;}

        // main thread server loop
        Uint32 frameStart, frameTime;
        SDL_Event e;
        while (stop_flag == 0) {
            frameStart = SDL_GetTicks();
            while (SDL_PollEvent(&e) != 0) {
                if (e.type == SDL_QUIT) {
                    stop_flag = 1;
                }
            }

            SDL_SetRenderDrawColor(renderer,0,0,0,255);
            SDL_RenderClear(renderer);
            for (int i =0; i<MAX_CONNECTORS; i++) {
                SDL_Rect player = {0.5 * WIDTH_WINDOW + (world.clients[i].x * 50), 0.5 * HEIGHT_WINDOW + (world.clients[i].y * 50), 10, 10};
                SDL_SetRenderDrawColor(renderer, (int) i * 255 / MAX_CONNECTORS,0,(int) (MAX_CONNECTORS - i) * 255 / MAX_CONNECTORS,255);
                SDL_RenderFillRect(renderer, &player);
            }
            SDL_RenderPresent(renderer);
            frameTime = SDL_GetTicks() - frameStart;

            // Delay to cap the FPS
            if (frameTime < FRAME_TIME) {
                SDL_Delay(FRAME_TIME - frameTime); // Delay the remaining time to cap FPS
            }
        }
        
        TerminateThread(hThread, 100);
        //server.accept() blocks the thread...
        //WaitForSingleObject(hThread, INFINITE);
    } else {                                //CLIENT MODE
        // set up client                            
        int client_initialized = 0;
        int stop_flag = 0;
        client_state state = {0, 0};
        world_state world = {0};
        clientThreadParams params = {&stop_flag, &client_initialized, &state, &world};
        HANDLE hThread = CreateThread(
            NULL, 0,
            client_connection_handler,
            &params,
            0, NULL
        );
        //wait for initialization
        ASSERT(hThread, "Coulnt initialize Client : %d", GetLastError());
        while (client_initialized ==0) {;}

        // main Client loop
        Uint32 frameStart, frameTime;
        SDL_Event e;
        while (stop_flag == 0) {
            frameStart = SDL_GetTicks();
            while (SDL_PollEvent(&e) != 0) {
                if (e.type == SDL_QUIT) {
                    stop_flag = 1;
                }
                else if (e.type == SDL_KEYDOWN) {
                    switch (e.key.keysym.sym)
                    {
                    case SDLK_ESCAPE:
                        stop_flag = 1;
                        break;
                    case SDLK_w:
                        if (state.y != 10) {state.y +=1;}
                        break;
                    case SDLK_a:
                        if (state.x != -10) {state.x -=1;}
                        break;
                    case SDLK_s:
                        if (state.y != -10) {state.y -=1;}
                        break;
                    case SDLK_d:
                        if (state.x != 10) {state.x +=1;}
                        break;
                    default:
                        break;
                    }
                    printf("%d, %d = current state\n", state.x, state.y);
                }
            }

            SDL_SetRenderDrawColor(renderer,0,0,0,255);
            SDL_RenderClear(renderer);
            SDL_Rect player = {0.5 * WIDTH_WINDOW + (state.x * 50), 0.5 * HEIGHT_WINDOW + (state.y * 50), 10, 10};
            printf("player: %d %d \n", player.x, player.y);
            SDL_SetRenderDrawColor(renderer, 255,0,0,255);
            SDL_RenderFillRect(renderer, &player);
            SDL_RenderPresent(renderer);
            frameTime = SDL_GetTicks() - frameStart;

            // Delay to cap the FPS
            if (frameTime < FRAME_TIME) {
                SDL_Delay(FRAME_TIME - frameTime); // Delay the remaining time to cap FPS
            }
        }
        
        TerminateThread(hThread, 100);
    }


    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
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
    client_receiver_Params receive_params = {s, (params->world)};
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
    HANDLE connector_threads[MAX_CONNECTORS]; server_receiver_Params parameters[MAX_CONNECTORS];
    struct sockaddr_in service = { .sin_family = AF_INET, .sin_addr.s_addr = inet_addr("192.168.1.245"), .sin_port = htons(8008)};
    { // initialization of networking
        ASSERT(!((s = socket(AF_INET , SOCK_STREAM, 0))  == INVALID_SOCKET), 
                "Could not create socket :%d", WSAGetLastError());
        ASSERT(!(bind(s, (SOCKADDR *) &service, sizeof(service))), 
                "Could not bind socket : %d, close socket gave : %d", WSAGetLastError(), CLOSE_SOCK(s));
        ASSERT(!(listen(s, SOMAXCONN)), 
                "Could not listen to incoming connections : %d, close socket gave : %d", WSAGetLastError(), CLOSE_SOCK(s));

    }
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
        server_receiver_Params receive_params = {client, *(params->connected), params->world};
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

DWORD WINAPI server_receive_thread(LPVOID lpParam) {
    server_receiver_Params* params = (server_receiver_Params*)lpParam;
    char receive_buffer[MAX_MESSAGE_SIZE];
    int res;
    do {
        res = recvfrom(params->s, receive_buffer, MAX_MESSAGE_SIZE,0, NULL, NULL);
        if (res > 0 && res <=MAX_MESSAGE_SIZE) {

            if (receive_buffer[0] == '0' && receive_buffer[1] == '0') {
                printf("received ping from client: %d\n", params->connector_id);
                continue;
            }

            if (receive_buffer[0] == '0' && receive_buffer[1] == '1') {
                char dest[MAX_MESSAGE_SIZE];
                strncpy(dest, receive_buffer + TYPE_FIELD_CHARS, LENGTH_FIELD_CHARS);
                dest[LENGTH_FIELD_CHARS] = '\0';
                int payload_size = atoi(dest); // got the size of the payload
                // memset(dest, '\0',MAX_MESSAGE_SIZE);
                strncpy(dest, receive_buffer + TYPE_FIELD_CHARS + LENGTH_FIELD_CHARS, payload_size);
                dest[payload_size] = '\0'; // payload
                char x[4], y[4];
                strncpy(x, dest, 3);
                strncpy(y, dest+3, 3);
                y[3] = x[3] = '\0';
                printf("Cient %d, x = %d, y = %d\n", params->connector_id, atoi(x), atoi(y)); // x and y are correct
                
                params->world->clients[params->connector_id -1].x = atoi(x);
                params->world->clients[params->connector_id -1].y = atoi(y);
                continue;
            }  
            continue;     
        }
        if (res == 0) {
            printf("Client %d stopped\n", params->connector_id);
            return 0;
            // handle connection close
        }
        // handle error in connection
        printf("Error while reading from client %d\n", params->connector_id);
        return 1;
    } while (1);
}
DWORD WINAPI client_receive_thread(LPVOID lpParam) {
    client_receiver_Params* params = (client_receiver_Params*)lpParam;
    char receive_buffer[MAX_MESSAGE_SIZE]; int res;
    do {
        res = recvfrom(params->s, receive_buffer, MAX_MESSAGE_SIZE,0,NULL,NULL);
        if (res > 0 && res <= MAX_MESSAGE_SIZE) {
            printf("Received : %.*s", res, receive_buffer);
            continue;
        } 
        if (res == 0) {
            printf("Server closed\n");
            return 0 ;
            //handle conn close
        }
        // handle error in receiver
        printf("Error while reading\n");
        return 1;
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
DWORD WINAPI server_connection_handler(LPVOID lpParam) {
    serverThreadParams* params = (serverThreadParams*)lpParam;
    server(params);
    
}
DWORD WINAPI client_connection_handler(LPVOID lpParam) {
    clientThreadParams* params = (clientThreadParams*)lpParam;
    client(params);
}
