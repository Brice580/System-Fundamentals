#include "server.h"
#include <stdlib.h>
#include <debug.h>
#include <time.h>
#include "player_registry.h"
#include <pthread.h>
#include <string.h>
#include "jeux_globals.h"
#include <unistd.h>

/*
 * Thread function for the thread that handles a particular client.
 *
 * @param  Pointer to a variable that holds the file descriptor for
 * the client connection.  This pointer must be freed once the file
 * descriptor has been retrieved.
 * @return  NULL
 *
 * This function executes a "service loop" that receives packets from
 * the client and dispatches to appropriate functions to carry out
 * the client's requests.  It also maintains information about whether
 * the client has logged in or not.  Until the client has logged in,
 * only LOGIN packets will be honored.  Once a client has logged in,
 * LOGIN packets will no longer be honored, but other packets will be.
 * The service loop ends when the network connection shuts down and
 * EOF is seen.  This could occur either as a result of the client
 * explicitly closing the connection, a timeout in the network causing
 * the connection to be closed, or the main thread of the server shutting
 * down the connection as part of graceful termination.
 */
void *jeux_client_service(void *arg){

    int fd = *((int*) arg); //arg holds the file descriptor
    free(arg); //free the previous allocated space in arg
    pthread_detach(pthread_self()); //detach the thread from the pool of threads for self termination
    debug("Thread has been Detached!");
    //========================================================================================================
    CLIENT* client = creg_register(client_registry, fd);
    debug("[%d] Client Registered", client_get_fd(client));
    //========================================================================================================Maybe set some error handling here...

    int logged_in = 0; // Will be set to 1, when a client has logged in <-Stored on Thread Stack so OK
    
    debug("Service Loop Time");
    JEUX_PACKET_HEADER* header = calloc(1, sizeof(JEUX_PACKET_HEADER));
    void* potential_payload = NULL;
    //Enter service loop
    //========================================================================================================
    while(1){
        debug("Inside the Loop");
        int x = proto_recv_packet(fd, header, &potential_payload);
        if(x == - 1){
            debug("EOF on fd: %d", fd);
            if(logged_in){
                player_unref(client_get_player(client), "server discarded"); //=========================come back to this too
                client_logout(client);
                logged_in = 0;
                debug("Client Logged Out.");
            }
            debug("Unregistering the Client.");
            int y = creg_unregister(client_registry, client);
            close(fd); //added this because i realize fds are never used again once started.
            if(y == -1){
                free(header);
                return NULL;
                }
            debug("Client Unregistered!");

            free(header);
            return NULL;

        }

    // ok now we can start doing things with the packets we get
    //========================================================================================================
        if(header->type == JEUX_LOGIN_PKT){
            debug("[%d] LOGIN RECIEVED",client_get_fd(client));
            //should I check if there are tow
            if(logged_in == 1){
                debug("User Already Logged in");
                //Send a NACK back to the client
                client_send_nack(client); //sends a NACK

            }
            else{
                //Client is not logged in
                debug("Client is not logged in!");
                PLAYER* player = preg_register(player_registry, (char*)potential_payload);
                if(player == NULL){
                    debug("Player error");
                    client_send_nack(client);
                }
                
                int z = client_login(client, player);
                if(z == -1){
                    debug("Could not create client");
                    client_send_nack(client);
                }

                debug("ACK sent");
                client_send_ack(client, NULL, 0);
                logged_in = 1;

            }

        }
        else if(header->type == JEUX_USERS_PKT){
            debug("[%d] USERS PACKET RECIEVED", client_get_fd(client));
            if(logged_in == 0){
                debug("Must log in first!");
                client_send_nack(client);
            }
            else{

                PLAYER** arr = creg_all_players(client_registry);
                debug("PLAYER ARRAY RETRIEVED");
                char* payload = NULL;
                int payload_size = 0;

                for (int i = 0; arr[i] != NULL; i++) {
                    PLAYER *player = arr[i];
                    int rating = player_get_rating(player);
                    char str[20];
                    sprintf(str, "%d", rating); //convert int into a string length
                    char* p = player_get_name(player);
                    int name_len = strlen(p);
                    int rating_len = strlen(str);
                    int line_len = name_len + 1 + rating_len + 2; // account for tab character and newline character
                    char line[line_len];
                    snprintf(line, line_len, "%s\t%d\n", p, rating);
                    payload = realloc(payload, payload_size + line_len);
                    memcpy(payload + payload_size, line, line_len);
                    payload_size += line_len;
                    player_unref(player, "Removed from players list in sevrer.c");
                }

                client_send_ack(client, payload, payload_size);
                free(payload);
                free(arr);
                                    
                }
            }
            else if(header->type == JEUX_INVITE_PKT){
            debug("[%d] INVITE PACKET RECIEVED", client_get_fd(client));
                if(logged_in == 0){
                debug("Must log in first!");
                client_send_nack(client);
                }
                else{
                    //potential payload has the name of the invited user
                    debug("Look Up name");
                    CLIENT* invited = creg_lookup(client_registry, (char*)potential_payload);
                    if(invited != NULL){ 

                        debug("Client Found");

                        debug("Make an invitation");
                        if(header->role == 2){
                            int check = client_make_invitation(client, invited, FIRST_PLAYER_ROLE, SECOND_PLAYER_ROLE);
                            if(check == -1){
                                client_send_nack(client);
                                //return NULL;
                            }
                            debug("INVITATION MADE");

                            JEUX_PACKET_HEADER* source_ack = calloc(1, sizeof(JEUX_PACKET_HEADER));
                            source_ack->type = JEUX_ACK_PKT;
                            struct timespec clock;
                            clock_gettime(CLOCK_MONOTONIC, &clock);
                            source_ack->timestamp_sec = htonl(clock.tv_sec); 
                            source_ack->timestamp_nsec = htonl(clock.tv_nsec);
                            source_ack->id = check;
                            source_ack->role = header->role;
                            client_unref(invited, "after invitation attempt");
                            int k = client_send_packet(client, source_ack, NULL);
                            if(k == -1){
                                client_send_nack(client); // <- IDK ABOUT THIS..?
                            }
                            free(source_ack);
                            //client_unref(client, "after invitation attempt");


                        }
                        else if(header->role == 1){
                            int check = client_make_invitation(client, invited, SECOND_PLAYER_ROLE, FIRST_PLAYER_ROLE);
                            if(check == -1){
                                  client_send_nack(client);
                                //return NULL;
                            }
                            debug("INVITATION MADE");
                            
                            JEUX_PACKET_HEADER* source_ack = calloc(1, sizeof(JEUX_PACKET_HEADER));
                            source_ack->type = JEUX_ACK_PKT;
                            struct timespec clock;
                            clock_gettime(CLOCK_MONOTONIC, &clock);
                            source_ack->timestamp_sec = htonl(clock.tv_sec); 
                            source_ack->timestamp_nsec = htonl(clock.tv_nsec);
                            source_ack->id = check;
                            source_ack->role = header->role;

                            client_unref(invited, "after invitation attempt");
                            int k = client_send_packet(client, source_ack, NULL);
                            if(k == -1){
                                client_send_nack(client); // <- IDK ABOUT THIS..?
                            }
                            free(source_ack);
                            //client_unref(client, "after invitation attempt");

                                            
                        }
                    }
                    else{
                        client_send_nack(client);
                    }

                }


            }
            else if(header->type == JEUX_REVOKE_PKT){
            debug("[%d] REVOKE PACKET RECIEVED", client_get_fd(client));
                if(logged_in == 0){
                debug("Must log in first!");
                client_send_nack(client);
                }
                else{
                    int revoke = client_revoke_invitation(client, header->id);

                    if(revoke == -1){
                        debug("Revoke failed");
                        client_send_nack(client);
                    }
                    else{
                        client_send_ack(client, NULL, 0);
                    }
                }
                
            }
            else if(header->type == JEUX_DECLINE_PKT){
                debug("DECLINE DETECTED");
                if(logged_in == 0){
                debug("Must log in first!");
                client_send_nack(client);
                }
                else{
                    int decline = client_decline_invitation(client, header->id);

                    if(decline == -1){
                        debug("decline failed");
                        client_send_nack(client);
                    }
                    else{
                        client_send_ack(client, NULL, 0);
                    }
                }

            }
            else if(header->type==JEUX_ACCEPT_PKT){

            debug("[%d] ACCEPT PACKET RECIEVED", client_get_fd(client));
                if(logged_in == 0){
                debug("Must log in first!");
                client_send_nack(client);
                } //============================================================ Make the case for if the inv is in accepted or closed state
                else{  

                    char* arg_ptr = NULL; //<- might need to switch back
                    int accept = client_accept_invitation(client, header->id, &arg_ptr);
                    debug("returned from client accept inv");
                    if(accept == -1){
                        debug("accept failed");
                        client_send_nack(client);
                        if(arg_ptr != NULL){free(arg_ptr);}
                    }
                    else{
                        debug("send an ack");
                        if(arg_ptr != NULL){
                            debug("payload: %s", arg_ptr);
                            char *u = arg_ptr;
                            client_send_ack(client, u, strlen(u));
                            free(arg_ptr);
                        }
                        else{ client_send_ack(client, NULL, 0); }
                    }
                }

            }
            else if(header->type == JEUX_RESIGN_PKT){
            debug("[%d] RESIGN PACKET RECIEVED", client_get_fd(client));
                if(logged_in == 0){
                    debug("Client not logged in, so how would you resign.");
                    client_send_nack(client);
                }
                else{
                    //should check if its a valid id or not
                    debug("Calling Client Resign");
                    int resign =  client_resign_game(client, header->id);
                    debug("Exited Client Resign with exit code: %d", resign);

                    if(resign == -1){
                        debug("Resign Failed");
                        client_send_nack(client);
                    }
                    else{
                        client_send_ack(client, NULL, 0);
                    }
                }

            }
            else if(header->type == JEUX_MOVE_PKT){
            debug("[%d] MOVE PACKET RECIEVED", client_get_fd(client));
                if(logged_in == 0){
                debug("Must log in first!");
                client_send_nack(client);
                }
                else{
                    int move =  client_make_move(client, header->id, potential_payload);

                    if(move == -1){
                        debug("make move failed");
                        client_send_nack(client);
                    }
                    else{
                        client_send_ack(client, NULL, 0);
                    }
                }

            }
            else{
                client_send_nack(client);
            }
           
        }
         return NULL;

    }
       

