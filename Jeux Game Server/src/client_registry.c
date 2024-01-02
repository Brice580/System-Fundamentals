#include <stdlib.h>
#include <stdio.h>
#include "string.h"
#include "client_registry.h"
#include <pthread.h>
#include <semaphore.h>
#include <debug.h>
#include <unistd.h>
#include "jeux_globals.h"


//Every function that modified the values of this struct should have a mutex lock
//Originally I thought to put a mutex lock in the struct but that would make things inefficient, since all the functions would have the same lock
typedef struct client_registry{
    //add more as I read the doc
    CLIENT *clients[MAX_CLIENTS]; //array of clients
    int size; //size of array
    pthread_mutex_t lock;
    sem_t semaphore;
    
} CLIENT_REGISTRY;



CLIENT_REGISTRY *creg_init(){
    debug("Initializing Registry");

    client_registry = calloc(1, sizeof(CLIENT_REGISTRY)); //malloc space for the reg
    

    if (client_registry == NULL) {
        debug("Malloc error");
        return NULL; //the malloc did not succeed so return NULL
    }
    

    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_registry->clients[i] = NULL; //Just clear all the values of the array so we dont have any garbage values
    }

    int ptexx = pthread_mutex_init(&client_registry->lock, NULL);
    if(ptexx != 0){return NULL;}

    sem_init(&client_registry->semaphore, 0, 0);

    client_registry->size = 0;

    return client_registry;
}

/*
 * Finalize a client registry, freeing all associated resources.
 * This method should not be called unless there are no currently
 * registered clients.
 *
 * @param cr  The client registry to be finalized, which must not
 * be referenced again.
 */
void creg_fini(CLIENT_REGISTRY *cr){ 

    debug("Finalizing Registry");
    if(cr->size == 0){

     
    pthread_mutex_destroy(&cr->lock);
    sem_destroy(&cr->semaphore);
    debug("Freed Resources.");

    //MAY HAVE TO FREE PLAYERS ARRAY AND OR THE CR REGISTRY BEFORE CALLING THIS LOL
   
    if(cr != NULL) {
        debug("Freeing the Registry");
        free(cr);
        cr = NULL; //set to null to prevent corruption

    }
    }
}

CLIENT *creg_register(CLIENT_REGISTRY *cr, int fd){

    debug("Registering the Client...");
    CLIENT* client = client_create(cr,fd);//malloc a new CLIENT
    if(client == NULL){return NULL;} 
    debug("Client created");
    /*REMEMBER : this function is used to keep inventory of all them CLIENT*/

    if(cr->size >= MAX_CLIENTS){return NULL;} //client cannot be registered

    pthread_mutex_lock(&cr->lock); // lock the registry for reasons... idk if this is right yet tbh

    // for(int i= 0; i < cr->size; i++){
    //     if(cr->clients[i] == NULL){
    //         cr->clients[i] = client;
    //         debug("CLIENT STORED AT INDEX: %d", cr->size);
    //         cr->size++;

    //         break;
    //     }
    // }

    cr->clients[cr->size] = client;
    cr->size++;

    //client_ref(client, "New client in registry");
    debug("Client Registered at FD %d", client_get_fd(client));

    pthread_mutex_unlock(&cr->lock);

    return client;
}

/*
 * Unregister a CLIENT, removing it from the registry.
 * The client reference count is decreased by one to account for the
 * pointer discarded by the client registry.  If the number of registered
 * clients is now zero, then any threads that are blocked in
 * creg_wait_for_empty() waiting for this situation to occur are allowed
 * to proceed.  It is an error if the CLIENT is not currently registered
 * when this function is called.
 *
 * @param cr  The client registry.
 * @param client  The CLIENT to be unregistered.
 * @return 0  if unregistration succeeds, otherwise -1.
 */
int creg_unregister(CLIENT_REGISTRY *cr, CLIENT *client){

    debug("[%d] Unregistering Client", client_get_fd(client));
    if(client == NULL){ return -1; }
    int index = -1;

    pthread_mutex_lock(&cr->lock);

    for(int i =0 ; i < cr->size; i++){
        //debug("Pointer to client : %p. In client array: %p. Size: %d. i = %d", client, cr->clients[i], cr->size, i);
        if(cr->clients[i] == client){
            
            debug("Client Retrieved in search of registry");
            index = i;
            break;
        }
    }

    /*check if index was found (client was registered or not)*/
    if(index == -1){
        debug("Error in finding index to unregister given client.");
        return -1;
    }



    for(int i = index; i < cr->size - 1; i++){

        cr->clients[i] = cr->clients[i+1];

    }
    cr->clients[cr->size-1] = NULL;
    cr->size--;
    //closing fd and freeing clinet should be done in server.c ...?
    //close(client_get_fd(client));
    //free(client);


    client_unref(client, "Unregistered"); // <------------- MIGHT HAVE TO PUT THIS IN THE LOCK...?

    //player_unref(client_get_player(client), "Client is being freed");

    //come back here to deal with whatever the creg wait for empty is..

    if(cr->size == 0){
        debug("The semaphore is incremented...");
        sem_post(&cr->semaphore);
    }
    
    pthread_mutex_unlock(&cr->lock);

    debug("Client successfully unregistered!");

    return 0;


}
/*
 * Given a username, return the CLIENT that is logged in under that
 * username.  The reference count of the returned CLIENT is
 * incremented by one to account for the reference returned.
 *
 * @param cr  The registry in which the lookup is to be performed.
 * @param user  The username that is to be looked up.
 * @return the CLIENT currently registered under the specified
 * username, if there is one, otherwise NULL.
 */
CLIENT *creg_lookup(CLIENT_REGISTRY *cr, char *user){
    debug("creg_look_up() entry");
    if(user == NULL){ return NULL; }
    if(cr == NULL){ return NULL; }

    CLIENT* o = NULL;
    
    pthread_mutex_lock(&cr->lock);

    for(int i = 0; i < cr->size; i++){
        PLAYER* ptr = client_get_player(cr->clients[i]);
        if(ptr != NULL){

            char *x = player_get_name(ptr);
            if(strcmp(x, user) == 0){
                debug("CLIENT FOUND");
                o = cr->clients[i];
                client_ref(o, "Client Looked Up");
            }
        }
        

    }

    pthread_mutex_unlock(&cr->lock);

    if(o != NULL){
        return o;
    }
    else {
        debug("Client logged in under user name not found...");
        return NULL;
    }


}

/*
 * Return a list of all currently logged in players.  The result is
 * returned as a malloc'ed array of PLAYER pointers, with a NULL
 * pointer marking the end of the array.  It is the caller's
 * responsibility to decrement the reference count of each of the
 * entries and to free the array when it is no longer needed.
 *
 * @param cr  The registry for which the set of usernames is to be
 * obtained.
 * @return the list of players as a NULL-terminated array of pointers.
 */
PLAYER **creg_all_players(CLIENT_REGISTRY *cr){ 
    debug("Preparing creg_all_players()");

    PLAYER** players = (PLAYER**) malloc((MAX_CLIENTS+1) * sizeof(PLAYER*)); // should malloc an array correctly I believe? the plus 1 is for NULL

    if(players == NULL) { return NULL; } //Malloc failed

    pthread_mutex_lock(&cr->lock);

    PLAYER* returned = NULL;
    int player_index = 0;

    for(int i = 0 ; i < cr->size; i++){
        
        if(cr->clients[i] == NULL){ break; }

        returned = client_get_player(cr->clients[i]);

        if(returned != NULL){
            players[player_index] = returned;
            returned = player_ref(returned, "added to player's list to be returned to creg all"); //Might have to relook this but should be valid.
            player_index++;
        }

    }

    players[player_index] = NULL;


    
    pthread_mutex_unlock(&cr->lock);

    return players;

}

/*
 * A thread calling this function will block in the call until
 * the number of registered clients has reached zero, at which
 * point the function will return.  Note that this function may be
 * called concurrently by an arbitrary number of threads.
 *
 * @param cr  The client registry.
 */
void creg_wait_for_empty(CLIENT_REGISTRY *cr){
    debug("creg_wait_for_empty() called");
    if(cr->size != 0){
        sem_wait(&cr->semaphore);
    }
    
}

/*
 * Shut down (using shutdown(2)) all the sockets for connections
 * to currently registered clients.  The clients are not unregistered
 * by this function.  It is intended that the clients will be
 * unregistered by the threads servicing their connections, once
 * those server threads have recognized the EOF on the connection
 * that has resulted from the socket shutdown.
 *
 * @param cr  The client registry.
 */
void creg_shutdown_all(CLIENT_REGISTRY *cr){

    debug("Shutting Services Down");
    pthread_mutex_lock(&cr->lock); 

    for (int i = 0; i < cr->size; i++) {
        
        CLIENT *client = cr->clients[i];

        if (client != NULL) {
            int fd = client_get_fd(client);
            debug("Ending it all");
            shutdown(fd, SHUT_RD);
        }
        
    }

    pthread_mutex_unlock(&cr->lock); 


}




