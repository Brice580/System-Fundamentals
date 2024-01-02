#include "player.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "debug.h"
#include <math.h>

typedef struct player{
    char* username;
    int rating;
    int ref_count;
    pthread_mutex_t lock;

} PLAYER;


PLAYER *player_create(char *name){

    debug("Creating Player");



    PLAYER* player = calloc(1, sizeof(PLAYER));
    if(player == NULL){return NULL;} //Calloc failed

    int ptex = pthread_mutex_init(&player->lock, NULL);
    if(ptex != 0 ){ return NULL;}


    player->username = name;
    player->ref_count = 0;
    player->rating = PLAYER_INITIAL_RATING;

    player_ref(player, "because player was initalized");
    debug("Player %s created successfully", name);
    return player;

}

PLAYER *player_ref(PLAYER *player, char *why){
    debug("Incrementing Player");

    pthread_mutex_lock(&player->lock);
    //int before = player->ref_count;
    player->ref_count++;
    //debug("%lu: Reference Count increased from %d -> %d %s.", pthread_self(), before, player->ref_count, why);

    pthread_mutex_unlock(&player->lock);

    return player;
}


void player_unref(PLAYER *player, char *why){
    debug("Decrementing Player");


    pthread_mutex_lock(&player->lock);

    //int before = player->ref_count;
    player->ref_count--;
    //debug("%lu: Reference Count decreased on player from %d -> %d %s.", pthread_self(), before, player->ref_count, why);    
    

    if(player->ref_count == 0){
        debug("Preparing to free player");
        pthread_mutex_destroy(&player->lock); //destroy the mutex since no more ref?
        
        debug("Free Player");
        //free(player->username);
        free(player);  //free the pointer

    }
     pthread_mutex_unlock(&player->lock);



}

char *player_get_name(PLAYER *player){
    
    debug("Getting player name");

    if(player == NULL){
        return NULL;
    }

    char *source = player->username;

    return source;

}

int player_get_rating(PLAYER *player){
    debug("Getting player rating");

    if(player == NULL){
        return 0;
    }

    int rate = player->rating;

    return rate;

}

 
void player_post_result(PLAYER *player1, PLAYER *player2, int result){

    debug("Post Results of Game");

//==========================================================================COME BACK HERE FOR MUTEX LOCKS=====================================
    if(result == 0 || result == 1 || result == 2){
        double S1;
        double S2;
        if (result == 0) {
            S1 = 0.5;
            S2 = 0.5;
        } else if (result == 1) {
            S1 = 1;
            S2 = 0;
        } else if (result == 2) {
            S1 = 0;
            S2 = 1;
        } 

        int R1 = player1->rating;
        int R2 = player2->rating;

        double E1 = 1.0 / (1.0 + pow(10.0, (S2 - S1) / 400.0));
        double E2 = 1.0 / (1.0 + pow(10.0, (S1 - S2) / 400.0));

        int newR1 = round(R1 + (32.0 * (S1 - E1)));
        int newR2 = round(R2 + (32.0 * (S2 - E2)));

        player1->rating = newR1;
        player2->rating = newR2;
    }

}