#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include "debug.h"
#include "jeux_globals.h"

typedef struct player_registry{

    pthread_mutex_t lock;
    int length;
    int num_players;
    PLAYER** players;

} PLAYER_REGISTRY;

PLAYER_REGISTRY *preg_init(void){
    debug("Initialize the Player Registry");
    
    player_registry = calloc(1, sizeof(PLAYER_REGISTRY));
    if(player_registry == NULL){return NULL;} // calloc failed

    player_registry->num_players = 0; //no players yet

    player_registry->length = 200; //abritary value for now

    player_registry->players = calloc(player_registry->length, sizeof(PLAYER*));
    if(player_registry->players == NULL){return NULL;}

    int ptex = pthread_mutex_init(&player_registry->lock, NULL);
    if(ptex != 0 ){ return NULL;}

    return player_registry;


}
void preg_fini(PLAYER_REGISTRY *preg){

    debug("Finalizing the player registry!");

    
    for(int i =0; i < preg->num_players; i++){

        PLAYER* x = preg->players[i];
        player_unref(x, "finalizing the player registry.");

    }
    free(preg->players);
    free(preg);
    debug("PREG_FINI: destroying the mutex");
    //pthread_mutex_destroy(&preg->lock);

}

PLAYER *preg_register(PLAYER_REGISTRY *preg, char *name){

    pthread_mutex_lock(&preg->lock);

    //first check the registry for a user with name already
    debug("Attemping to register into the Player Registry");
    for(int i =0; i < preg->num_players; i++){
        
        if(preg->players[i] != NULL){
            PLAYER* ref = preg->players[i];
            //debug("IN THE ARRAY NAME: %s, name: %s", player_get_name(ref), name);
            if(strcmp(player_get_name(ref),name) == 0){
                pthread_mutex_unlock(&preg->lock);
                debug("IN THE ARRAY NAME: %s, name: %s", player_get_name(ref), name);
                player_ref(ref, "player with queried name already exists, so reference returned");
                return ref;
            }

            
        }
    }

    if (preg->num_players == preg->length-1) { //adding one more would be invalud
        debug("Array is full, resizing...");
        preg->length *= 2;
        preg->players = realloc(preg->players, preg->length * sizeof(PLAYER*));
        if (preg->players == NULL) {
            
            pthread_mutex_unlock(&preg->lock);

            return NULL;
        }
        debug("Array Resized");
    }

    PLAYER* player = player_create(name);
    debug("[%lu] Player Created", pthread_self());

    preg->players[preg->num_players] = player; //adds.. since its persistent
    preg->num_players++;
    debug("[%lu] Player added into registry at index (%d)", pthread_self(), preg->num_players - 1);

    player_ref(player, "retained by registry");

    pthread_mutex_unlock(&preg->lock);

    return player;

}