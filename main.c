#include <winsock2.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#define SDL_MAIN_HANDLED
#include "definitions.h"
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

    if (argc > 1 && argv[1][0] == 's') {
        // SERVER MODE
        mainServerLoop(window, renderer);
    } else {  
        //CLIENT MODE
        mainClientLoop(window, renderer);
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    WSACleanup();
    SDL_Quit();
}

void mainClientLoop(SDL_Window* window, SDL_Renderer* renderer) {
    // set up client                            
    int client_initialized = 0, stop_flag = 0;
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
                switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        stop_flag = 1;
                        break;
                    case SDLK_w:
                        if (state.y != MAX_COORDINATE) {state.y +=1;}
                        break;
                    case SDLK_a:
                        if (state.x != MIN_COORDINATE) {state.x -=1;}
                        break;
                    case SDLK_s:
                        if (state.y != MIN_COORDINATE) {state.y -=1;}
                        break;
                    case SDLK_d:
                        if (state.x != MAX_COORDINATE) {state.x +=1;}
                        break;
                    default:
                        break;
                }
            }
        }

        SDL_SetRenderDrawColor(renderer,0,0,0,255);
        SDL_RenderClear(renderer);
        SDL_Rect player = {0.5 * WIDTH_WINDOW + (state.x * CELL_SIZE), 
                            0.5 * HEIGHT_WINDOW + (state.y * CELL_SIZE), PLAYER_SIZE, PLAYER_SIZE};
        SDL_SetRenderDrawColor(renderer, 255,0,0,255);
        SDL_RenderFillRect(renderer, &player);
        SDL_RenderPresent(renderer);
        frameTime = SDL_GetTicks() - frameStart;
        if (frameTime < FRAME_TIME) {
            SDL_Delay(FRAME_TIME - frameTime);
        }
    }
    
    TerminateThread(hThread, 100);
}

void mainServerLoop(SDL_Window* window, SDL_Renderer* renderer) {
    //setup & start server
    SOCKET connectors[MAX_CONNECTORS];
    int stop_flag = 0, server_initialized =0, connected = 0;
    world_state world;
    for (int i =0; i < MAX_CONNECTORS; i++) {
        world.clients[i].x = INVALID_COORDINATE;
        world.clients[i].y = INVALID_COORDINATE;
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
            if (world.clients[i].x == INVALID_COORDINATE) { 
                continue;
            }
            SDL_Rect player = {0.5 * WIDTH_WINDOW + (world.clients[i].x * CELL_SIZE),
                                0.5 * HEIGHT_WINDOW + (world.clients[i].y * CELL_SIZE), PLAYER_SIZE, PLAYER_SIZE};
            SDL_SetRenderDrawColor(renderer, (int) (i * 255 / MAX_CONNECTORS),0,0,255);
            SDL_RenderFillRect(renderer, &player);
        }
        SDL_RenderPresent(renderer);
        frameTime = SDL_GetTicks() - frameStart;
        if (frameTime < FRAME_TIME) {
            SDL_Delay(FRAME_TIME - frameTime);
        }
    }
    
    TerminateThread(hThread, 100);
}



void client(clientThreadParams* params) {
    printf("you started a client\n");
    SOCKET s = INVALID_SOCKET;
    client_state last_state = {(params->state->x), (params->state->y)};
    struct sockaddr_in service = {.sin_family = AF_INET, .sin_addr.s_addr = inet_addr("192.168.1.245"), .sin_port = htons(8008)};
    client_receiver_Params receive_params = {s, (params->world)};
    HANDLE hObject = CreateThread(NULL,0,client_receive_thread, &receive_params, 0, NULL);

    { // initialize and assert correct initialization
        ASSERT(!((s = socket(AF_INET , SOCK_STREAM, 0))  == INVALID_SOCKET), 
                "Could not create socket :%d", WSAGetLastError());
        ASSERT(!(connect(s, (SOCKADDR *) &service, sizeof(service))), 
                    "Could not connect to server : %d, close socket gave : %d", WSAGetLastError(), CLOSE_SOCK(s) );
        ASSERT(hObject, "Coulnt initialize Receiver : %d", GetLastError());  // NEED close socket ?? 
    } 
    *(params->initialized) = 1;
    do {
        if (last_state.x != params->state->x || last_state.y != params->state->y) {
            // if state changed since last time, send data to server
            last_state.x = params->state->x; // maybe paralellism issue
            last_state.y = params->state->y; // maybe paralellism issue
            char x[MAX_LENGTH_COORDINATE_CHARS + 1], y[MAX_LENGTH_COORDINATE_CHARS + 1];
            char tot[2 * MAX_LENGTH_COORDINATE_CHARS + 1];
            snprintf(x,sizeof(x),"%03d", last_state.x);
            snprintf(y,sizeof(y), "%03d", last_state.y);
            snprintf(tot, sizeof(tot), "%s%s", x, y);
            sendPackage(&s, "01", "0000000006", tot);           // LENGTH IS STILL MAGIC NUMBER
        } else { 
            // else send ping
            sendPackage(&s, "00", "0000000000", "0");
        }
        Sleep(MSG_DELAY_MILLIS);
    } while ( *(params->stop_flag) == 0);

    // clean up networking
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
        //start receiver for this client
        server_receiver_Params receive_params = {client, *(params->connected), params->world};
        parameters[*(params->connected)] = receive_params;
        HANDLE hObject = CreateThread(NULL,0,server_receive_thread, &parameters[*(params->connected)], 0, NULL);
        ASSERT(hObject, "Coulnt initialize Receiver : %d", GetLastError()); // need close socket ??   
        connector_threads[*(params->connected)]= hObject;

        *(params->connector_list + connectors) = client;
        connectors++;
        *(params->connected) = *(params->connected) + 1;
    }
    while (*params->stop_flag == 0) {
        continue;
    }
    // clean up for thread exit
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
                char x[MAX_LENGTH_COORDINATE_CHARS + 1], y[MAX_LENGTH_COORDINATE_CHARS + 1];
                strncpy(x, dest, MAX_LENGTH_COORDINATE_CHARS);
                strncpy(y, dest+MAX_LENGTH_COORDINATE_CHARS, MAX_LENGTH_COORDINATE_CHARS);
                y[MAX_LENGTH_COORDINATE_CHARS] = x[MAX_LENGTH_COORDINATE_CHARS] = '\0';
                printf("Cient %d, x = %d, y = %d\n", params->connector_id, atoi(x), atoi(y)); // x and y are correct
                
                params->world->clients[params->connector_id].x = atoi(x);
                params->world->clients[params->connector_id].y = atoi(y);
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
    char * package = (char*)malloc((MAX_MESSAGE_SIZE + 1 * sizeof(char)));
    strcpy(package, type);
    strcat(package, size);
    strcat(package, payload);
    send(*s,package,MAX_MESSAGE_SIZE,0);
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
