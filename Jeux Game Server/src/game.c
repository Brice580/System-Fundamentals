#include "game.h"
#include <stdlib.h>
#include <stdio.h>
#include "debug.h"
#include <pthread.h>


typedef struct game_move{

    GAME_ROLE current;
    int index;

} GAME_MOVE;

typedef struct game{

    GAME_MOVE current_move;
    GAME_ROLE current_player;
    GAME_ROLE winner;
    int ref_count;
    pthread_mutex_t lock;
    char* state;
    int finished;

} GAME;


GAME *game_create(void){

    GAME* game = calloc(1, sizeof(GAME));
    if(game == NULL){return NULL;}

    int ptex = pthread_mutex_init(&game->lock, NULL);
    if(ptex != 0 ){ 
        free(game);
        return NULL;
        }

    game->state =  calloc(1, 9*sizeof(char));
    if (game->state == NULL) {
        pthread_mutex_destroy(&game->lock);
        return NULL;
    }

    // Initialize the initial state
    
    for (int j = 0; j < 9; j++) {
        game->state[j] = ' ';
    }
    

    game->current_player = FIRST_PLAYER_ROLE;
    game->winner = NULL_ROLE;
    game = game_ref(game, "being initialized");
    game->finished = 0;

    pthread_mutex_init(&game->lock, NULL);
    
    return game;
}

GAME *game_ref(GAME *game, char *why){

    pthread_mutex_lock(&game->lock);

    //int before = game->ref_count;
    game->ref_count++;
    //debug("%lu: Reference Count increased from %d -> %d %s.", pthread_self(), before, game->ref_count, why);

    pthread_mutex_unlock(&game->lock);

    return game;

}

void game_unref(GAME *game, char *why){

    debug("Decrementing Game");


    pthread_mutex_lock(&game->lock);

    //int before = game->ref_count;
    game->ref_count--;
    //debug("%lu: Reference Count decreased on game from %d -> %d %s.", pthread_self(), before, game->ref_count, why);    
    

    if(game->ref_count == 0){
        debug("Preparing to free player");
        pthread_mutex_unlock(&game->lock);

        pthread_mutex_destroy(&game->lock); //destroy the mutex since no more ref?

        debug("Free Player");
        //free(player->username);
        free(game->state);
        free(game);  //free the pointer

    }
    pthread_mutex_unlock(&game->lock);

}

/*
 * Apply a GAME_MOVE to a GAME.
 * If the move is illegal in the current GAME state, then it is an error.
 *
 * @param game  The GAME to which the move is to be applied.
 * @param move  The GAME_MOVE to be applied to the game.
 * @return 0 if application of the move was successful, otherwise -1.
 */
int game_apply_move(GAME *game, GAME_MOVE *move){

    pthread_mutex_lock(&game->lock);

    if (game == NULL || move == NULL) {
        pthread_mutex_unlock(&game->lock);
        return -1;
    }

    if(game->state[move->index -1] != ' '){
        pthread_mutex_unlock(&game->lock);
        return -1;

    }
    
    else if(game->current_player == SECOND_PLAYER_ROLE){
        game->state[move->index - 1] = 'O'; //minus 1 since its stored in an array ;)
        game->current_player = FIRST_PLAYER_ROLE;
    }
    else if(game->current_player == FIRST_PLAYER_ROLE){
        game->state[move->index - 1] = 'X';
        game->current_player = SECOND_PLAYER_ROLE;
    }

    //At this point the game move is already executed
    //Now must check for winner, if any

    for (int i = 0; i < 9; i+=3) { //check the rows
        if (game->state[i] == game->state[i+1] && game->state[i+1] == game->state[i+2]) {
            if (game->state[i] == 'X') {
                game->winner = FIRST_PLAYER_ROLE;
                game->finished =1;
                break;
            
            } else if (game->state[i] == 'O') {
                game->winner = SECOND_PLAYER_ROLE;
                game->finished =1;
                break;
            }
        }
    }
    debug("ROWS CHECKED");
    for (int i = 0; i < 3; i++) { //check the columns
        if (game->state[i] == game->state[i+3] && game->state[i+3] == game->state[i+6]) {
            debug("i: %d i+3: %d i+6: %d",game->state[i], game->state[i+3], game->state[i+6]);
            if (game->state[i] == 'X') {
                game->winner = FIRST_PLAYER_ROLE;
                game->finished = 1;
                break;


            } else if (game->state[i] == 'O') {
                game->winner = SECOND_PLAYER_ROLE;
                game->finished = 1;
                break;
            }
        }
    }
    
    //one diagonal
    if (game->state[0] == game->state[4] && game->state[4] == game->state[8]) {
        if (game->state[0] == 'X') {
            game->winner = FIRST_PLAYER_ROLE;
            game->finished = 1;


        } else if (game->state[0] == 'O') {
            game->winner = SECOND_PLAYER_ROLE;
            game->finished = 1;


        }
    }
    //another diagonal
    if (game->state[2] == game->state[4] && game->state[4] == game->state[6]) {
        if (game->state[2] == 'X') {
            game->winner = FIRST_PLAYER_ROLE;
            game->finished = 1;

        } else if (game->state[2] == 'O') {
            game->winner = SECOND_PLAYER_ROLE;
            game->finished = 1;

        }
    }
    if(game->winner == NULL_ROLE){ //no winner yet
        int taken = 0;
        for (int i = 0; i < 9; i++) {
        if(game->state[i] != ' '){
            taken++;
        }
        }

        if (taken == 9) { //meaning board is filled up
            debug("DRAW");
            game->winner = NULL_ROLE; //??
            game->finished = 1;
        }
    }

    pthread_mutex_unlock(&game->lock);


    return 0;

}

int game_resign(GAME *game, GAME_ROLE role){

    if (game == NULL) {
        return -1;
    }

    pthread_mutex_lock(&game->lock);


    if(game->finished == 1){
        pthread_mutex_unlock(&game->lock);
        return -1;
    }

    if (role == FIRST_PLAYER_ROLE) {
        game->winner = SECOND_PLAYER_ROLE;
    } else {
        game->winner = FIRST_PLAYER_ROLE;
    }

    game->finished = 1;
    debug("finish");
    pthread_mutex_unlock(&game->lock);

    return 0;
}


/*
 * Get the GAME_ROLE of the player who has won the game.
 *
 * @param game  The GAME for which the winner is to be obtained.
 * @return  The GAME_ROLE of the winning player, if there is one.
 * If the game is not over, or there is no winner because the game
 * is drawn, then NULL_PLAYER is returned.
 */
GAME_ROLE game_get_winner(GAME *game){

    if(game == NULL) {
        return NULL_ROLE;
    }

    if(game_is_over(game)){
        return game->winner;
    }

    return NULL_ROLE;

}

int game_is_over(GAME *game){


    if(game->finished == 1){
        return 1;

    }

    return 0;
}


char *game_unparse_state(GAME *game){
    //not actually modifying the values of anything
    pthread_mutex_lock(&game->lock);
    char* buf = calloc(1, sizeof(char) * 60);
    char* board = game->state; //acts like a getter
    int index = 0;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            buf[index++] = board[i * 3 + j];
            if (j < 2) {
                buf[index++] = '|';
            }
        }
            if (i < 2) {
                buf[index++] = '\n';
                buf[index++] = '-';
                buf[index++] = '-';
                buf[index++] = '-';
                buf[index++] = '-';
                buf[index++] = '-';
                buf[index++] = '\n';
            } else {
                buf[index++] = '\n';
            }
    }

    if (game->current_player == FIRST_PLAYER_ROLE) {
        snprintf(buf + index, 11, "X to move\n");
    } else if (game->current_player == SECOND_PLAYER_ROLE) {
        snprintf(buf + index, 11, "O to move\n");
    }

    pthread_mutex_unlock(&game->lock);

    return buf;


}

/*
 * Attempt to interpret a string as a move in the specified GAME.
 * If successful, a GAME_MOVE object representing the move is returned,
 * otherwise NULL is returned.  The caller is responsible for freeing
 * the returned GAME_MOVE when it is no longer needed.
 * Refer to the assignment handout for the syntax that should be used
 * to specify a move.
 *
 * @param game  The GAME for which the move is to be parsed.
 * @param role  The GAME_ROLE of the player making the move.
 * If this is not NULL_ROLE, then it must agree with the role that is
 * currently on the move in the game.
 * @param str  The string that is to be interpreted as a move.
 * @return  A GAME_MOVE described by the given string, if the string can
 * in fact be interpreted as a move, otherwise NULL.
 */

GAME_MOVE *game_parse_move(GAME *game, GAME_ROLE role, char *str){

    if (role != NULL_ROLE && game->current_player != role) {
        return NULL;
    }

    GAME_MOVE *move = calloc(1, sizeof(GAME_MOVE));

    int idx = atoi(str);

    if (idx < 1 || idx > 9) {

        return NULL;

    }

    move->current = role;

    move->index = idx;

    return move;


}

char *game_unparse_move(GAME_MOVE *move){

    char player;
    if (move->current == FIRST_PLAYER_ROLE) {
        player = 'X';
    } else {
        player = 'O';
    }

    int index = move->index;

    size_t size = snprintf(NULL, 0, "%d<-%c", index, player) + 1; 

    char *buf = calloc(size, sizeof(char));  

    snprintf(buf, size, "%d<-%c", index, player); 


    return buf;

}