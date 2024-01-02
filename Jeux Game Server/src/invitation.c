#include "jeux_globals.h"
#include "debug.h"
#include "client_registry.h"
#include <stdlib.h>
#include <pthread.h>
#include <game.h>
#include "invitation.h"

typedef struct invitation{
    INVITATION_STATE state;
    CLIENT* source;
    CLIENT* target;
    GAME_ROLE source_role;
    GAME_ROLE target_role;
    GAME* current_game;
    int ref_count;
    pthread_mutex_t lock;

} INVITATION;

INVITATION *inv_create(CLIENT *source, CLIENT *target, GAME_ROLE source_role, GAME_ROLE target_role){
     debug("Creating Invitation");
    if(source == target){ return NULL; }
    if(source_role == target_role) { return NULL;}

    INVITATION* invite = malloc(sizeof(INVITATION));
    if(invite == NULL) { return NULL;}

    invite->state = INV_OPEN_STATE;
    invite->current_game = NULL;
    invite->ref_count = 0;
    invite->source = source;
    client_ref(source, "Source of Invitation");
    client_ref(target, "Target of Invitation");
    invite->target = target;
    invite->source_role = source_role;
    invite->target_role = target_role;
    debug("TARGET ROLE: %d", target_role);

    invite = inv_ref(invite, "newly created invitation");
    debug("Invitation Created Successfully");
    pthread_mutex_init(&invite->lock, NULL);

    
    return invite;

}

INVITATION *inv_ref(INVITATION *inv, char *why){
    debug("Incrementing Reference Count on Invitation");

    pthread_mutex_lock(&inv->lock);
    //int before = inv->ref_count;
    inv->ref_count++;
    //debug("%lu: Reference Count increased on invitation from %d -> %d because %s.", pthread_self(), before, inv->ref_count, why);

    pthread_mutex_unlock(&inv->lock);

    debug("Invitation Reference Count Incremented");

    return inv;
}

void inv_unref(INVITATION *inv, char *why){
    debug("Decrementing Invitation");


    pthread_mutex_lock(&inv->lock);

   // int before = inv->ref_count;
    inv->ref_count--;
    //debug("%lu: Reference Count decreased on invitation from %d -> %d because %s.", pthread_self(), before, inv->ref_count, why);    
    
    if(inv->ref_count == 0){
        debug("Preparing to Free the Invitation.");
        pthread_mutex_destroy(&inv->lock); //destroy the mutex since no more ref?
        if(inv->current_game != NULL){
            game_unref(inv->current_game, "Invitation freed");

        }
        client_unref(inv->source, "Invitation being freed.");
        client_unref(inv->target, "Invitation being freed.");
        debug("Free Invitation");
        free(inv);  //free the pointer
        
    }
    pthread_mutex_unlock(&inv->lock);

    debug("Invitation Successfully Decremented");


}

CLIENT *inv_get_source(INVITATION *inv){
    debug("Retrieving Invitation Source");

    if(inv == NULL){
        return NULL;
    }

    CLIENT *source = inv->source;

    debug("Exiting get source");

    return source;

}

CLIENT *inv_get_target(INVITATION *inv){
    debug("Retrieving Invitation Target");

    if(inv == NULL){
        debug("null");
        return NULL;
    }

    CLIENT *target = inv->target;

    debug("Exiting get target");

    return target;


}

GAME_ROLE inv_get_source_role(INVITATION *inv){
    debug("Getting Invitation Source Role");

    // if(inv == NULL){
    //     return NULL;
    // }

    GAME_ROLE source_role = inv->source_role;

    return source_role;

}

GAME_ROLE inv_get_target_role(INVITATION *inv){
    debug("Getting Invitation Target Role");

    if(inv == NULL){
        debug("ITS NULL!");
    }
    GAME_ROLE target_role = inv->target_role;

    debug("exit target role");

    return target_role;

}


GAME *inv_get_game(INVITATION *inv){
    debug("Geting game");

    if(inv == NULL){
        return NULL;
    }
    if(inv->current_game == NULL){
        return NULL;
    }

    GAME *game = inv->current_game;

    return game;

}

int inv_accept(INVITATION *inv){
    debug("Accepting the invite");
    pthread_mutex_lock(&inv->lock);
    if(inv->state != INV_OPEN_STATE){
        debug("invite not in open state");
        pthread_mutex_unlock(&inv->lock);
        return -1;
    }

    inv->state = INV_ACCEPTED_STATE;
    debug("Invitation: %p accepted", inv);
    
    GAME* game = game_create(); //is this a proper way to create the game - is this all?
    if(game == NULL){
        pthread_mutex_unlock(&inv->lock);
        return -1;
    }
    inv->current_game = game;
    debug("Game created");
    pthread_mutex_unlock(&inv->lock);
    return 0;
}

/*
 * Close an INVITATION, changing it from either the OPEN state or the
 * ACCEPTED state to the CLOSED state.  If the INVITATION was not previously
 * in either the OPEN state or the ACCEPTED state, then it is an error.
 * If INVITATION that has a GAME in progress is closed, then the GAME
 * will be resigned by a specified player.
 *
 * @param inv  The INVITATION to be closed.
 * @param role  This parameter identifies the GAME_ROLE of the player that
 * should resign as a result of closing an INVITATION that has a game in
 * progress.  If NULL_ROLE is passed, then the invitation can only be
 * closed if there is no game in progress.
 * @return 0 if the INVITATION was successfully closed, otherwise -1.
 */
int inv_close(INVITATION *inv, GAME_ROLE role) {

    pthread_mutex_lock(&inv->lock);

    if (inv->state != INV_OPEN_STATE && inv->state != INV_ACCEPTED_STATE) {
        pthread_mutex_unlock(&inv->lock);
        return -1;
    }

    if(role == FIRST_PLAYER_ROLE || role == SECOND_PLAYER_ROLE) {

        int check = game_resign(inv->current_game, role);
        if (check == -1) { 
            debug("Resign Failed");
            pthread_mutex_unlock(&inv->lock);
            return -1;
        }

        else{
            
            debug("Resign successful");
            inv->state = INV_CLOSED_STATE;
            pthread_mutex_unlock(&inv->lock);
            return 0;

        }
    }

    //AT THIS POINT THE ROLE IS NULL ROLE
    inv->state = INV_CLOSED_STATE;
    pthread_mutex_unlock(&inv->lock);


    return 0;


    // debug("Closing Invitation");
    // pthread_mutex_lock(&inv->lock);
    
    // if(inv->state != INV_OPEN_STATE && inv->state != INV_ACCEPTED_STATE){
    //     debug("Invalid Invitation State");
    //     return -1;
    // }

    // if(role == NULL_ROLE){
    //     debug("Type NULL ROLE");
    //     if(inv->current_game == NULL){ 
    //         //no game in progress
    //         inv->state = INV_CLOSED_STATE;
    //         return 0;
    //     }
    //     else{
    //         return -1;
    //     }
    // }

    // if(inv->current_game != NULL){
    //     //resign the game 0.0
    //     int x = game_resign(inv->current_game, role);
    //     if(x == -1){
    //         return -1;
    //     }
    // }
    
    // inv->state = INV_CLOSED_STATE;
    // debug("Invitation is set to closed");

    // pthread_mutex_unlock(&inv->lock);

    // debug("exiting invite close");

    // return 0;

}
