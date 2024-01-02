#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "debug.h"
#include "protocol.h"
#include "server.h"
#include "client_registry.h"
#include "player_registry.h"
#include "jeux_globals.h"
#include "csapp.h"

#ifdef DEBUG
int _debug_packets_ = 1;
#endif

static void terminate(int status);
void sighup_handler(int sig, siginfo_t *info, void *ucontext);
void *thread(void *vargp);

/*
 * "Jeux" game server.
 *
 * Usage: jeux <port>
 */
int main(int argc, char* argv[]){

    int port_number = 0;

    if (argc != 3 || strcmp(argv[1], "-p") != 0 /*|| strcmp(argv[0], "bin/jeux") != 0*/) {
        fprintf(stderr, "Usage: jeux -p <port>\n");
        exit(1);//check this after
    }
    //now check thats its a valid port after all

    port_number = atoi(argv[2]); //Actually I think we check this when we try to make a connection, not now other than port 0?
    if(port_number == 0){exit(1);}

    //set up for SIGHUP Handlers -> Should just terminate if detected?
    struct sigaction sighup;
    sighup.sa_flags = SA_SIGINFO;
    sighup.sa_sigaction = &sighup_handler;
    sigemptyset(&sighup.sa_mask);  
    if (sigaction(SIGHUP, &sighup, NULL) == -1) { terminate(1); }


    // Perform required initializations of the client_registry and
    // player_registry.
    client_registry = creg_init();
    player_registry = preg_init();

    // TODO: Set up the server socket and enter a loop to accept connections
    // on this socket.  For each connection, a thread should be started to
    // run function jeux_client_service().  In addition, you should install
    // a SIGHUP handler, so that receipt of SIGHUP will perform a clean
    // shutdown of the server.

    int *fd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;
    int listenfd = Open_listenfd(port_number);

    while(1){

        clientlen= sizeof(struct sockaddr_storage);
        fd = malloc(sizeof(int));
        if(fd == NULL){ terminate(0);}
        *fd = accept(listenfd, (SA *) &clientaddr, &clientlen);
        if(*fd < 0){ terminate(0);}
        debug("thread about to be created");
        int c =pthread_create(&tid, NULL, jeux_client_service, fd);
        if(c != 0) { terminate(0);}
    }
    
}


void sighup_handler(int sig, siginfo_t *info, void *ucontext){

    terminate(EXIT_SUCCESS);

}


/*
 * Function called to cleanly shut down the server.
 */
void terminate(int status) {
    // Shutdown all client connections.
    // This will trigger the eventual termination of service threads.
    creg_shutdown_all(client_registry);
    
    debug("%ld: Waiting for service threads to terminate...", pthread_self());
    creg_wait_for_empty(client_registry);
    debug("%ld: All service threads terminated.", pthread_self());

    // Finalize modules.
    creg_fini(client_registry);
    preg_fini(player_registry);

    debug("%ld: Jeux server terminating", pthread_self());
    exit(status);
}
