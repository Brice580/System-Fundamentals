#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "protocol.h"
#include "player.h"
#include "debug.h"
#include "csapp.h"


int proto_send_packet(int fd, JEUX_PACKET_HEADER *hdr, void *data){

    if(hdr == NULL){return -1;} //Invalid header 

    if(fd == -1){return -1;} //Invalid fd

    int converted = ntohs(hdr->size);

    struct timespec clock;
    clock_gettime(CLOCK_MONOTONIC, &clock);
    hdr->timestamp_sec = (uint32_t)clock.tv_sec; //are we supposed set the time stamp values or are they set elsewhere..? <-nvm doc says its when they are sent
    hdr->timestamp_nsec = (uint32_t)clock.tv_nsec;
    void* hdr_pointer = hdr; //for iteration

    int bufferset = write(fd, hdr, sizeof(JEUX_PACKET_HEADER)); //writes from the header to the fd
    //debug("bytes read: %d", bufferset);

    if(bufferset == -1 || bufferset == 0){return 1;}
    else if(bufferset < sizeof(JEUX_PACKET_HEADER)){
        //debug("short count 1");
        int missing_bytes = sizeof(JEUX_PACKET_HEADER) - bufferset;

        while (bufferset < sizeof(JEUX_PACKET_HEADER)) {
            //debug("gotcha");
            int bytes_written = write(fd, hdr, missing_bytes);
            if (bytes_written == -1 || bytes_written == 0) {
                return -1;
            }
            bufferset += bytes_written;
            missing_bytes -= bytes_written;
            hdr_pointer += bytes_written;

        }
    }
    //debug("payload size: %d", hdr->size);
    if(hdr->size > 0){ //size of data payload
        if(data != NULL){
            
            //void* data_ptr = data;
            int add_write = write(fd, data, converted);
            //debug("bytes payload written in send %d", add_write);

            if(add_write == -1 || add_write == 0){return -1;}
            else if(add_write < converted){
            //handle the short count situation, i will make this a separate function 
            int missing_add = converted - add_write;

            while(add_write < converted) {
                
                int written = write(fd, data, missing_add);
                if (written == -1 || written == 0) {
                    return -1;
                }
                add_write += written;
                missing_add -= written;
                //data_ptr += written;
            }
        }
        }

    }

    debug("Send is Finished");
    debug("%d.%d =>  size: %d type: %d role: %d",hdr->timestamp_sec, hdr->timestamp_nsec, hdr->size, hdr->type, hdr->role);

    return 0;

 

}

/*
 * Receive a packet, blocking until one is available.
 *
 * @param fd  The file descriptor from which the packet is to be received.
 * @param hdr  Pointer to caller-supplied storage for the fixed-size
 *   packet header.
 * @param payloadp  Pointer to a variable into which to store a pointer to any
 *   payload received.
 * @return  0 in case of successful reception, -1 otherwise.  In the
 *   latter case, errno is set to indicate the error.
 *
 * The returned packet has all multi-byte fields in network byte order.
 * If the returned payload pointer is non-NULL, then the caller has the
 * responsibility of freeing that storage.
 */
int proto_recv_packet(int fd, JEUX_PACKET_HEADER *hdr, void **payloadp){

    if(hdr == NULL){return -1;} //Invalid header 

    if(fd == -1){return -1;} //Invalid fd

    void* hdr_pointer = hdr;
    debug("Recieving");
    int bytes_read = read(fd, hdr, sizeof(JEUX_PACKET_HEADER));
    if(bytes_read == 0) {return -1; }
    //debug("bytes read in recv: %d", bytes_read);
    if(bytes_read == -1){return -1;}
    else if(bytes_read !=  sizeof(JEUX_PACKET_HEADER)){
        //debug("bytes read in short count: : %d", bytes_read);
        //handle the short count situation, i will make this a separate function 
        int missing_read = sizeof(JEUX_PACKET_HEADER) - bytes_read;
        //debug("missing read: %d", missing_read);
        while(bytes_read < sizeof(JEUX_PACKET_HEADER)) {
            //debug("stuck here?");
            int readd = read(fd, hdr, missing_read);
            
            if (readd == -1 || readd == 0) {
                return -1;
            }
            bytes_read += readd;
            missing_read -= readd;
            hdr_pointer += readd;
        }
    }

    //read is complete, now convert the multi byte fields because we are reading from a packet

    int y = ntohs(hdr->size);
    // hdr->timestamp_sec = ntohl(hdr->timestamp_sec);
    // hdr->timestamp_nsec = ntohl(hdr->timestamp_nsec);

    //he proto_recv_packet() uses malloc() to allocate memory for the payload (if any) <- Malloc should be called in the if for hdrsize >0 (since thats payload size)
    if(hdr->size > 0){
        debug("Payload present");
     
        if((*payloadp = (void*) calloc(1, y)) == NULL) {return -1;} //Returns if Malloc did not work 

        int read_more = read(fd, *payloadp, y);
    
        //debug("readmore: %d", read_more);
        //debug("size: %d", y);
        //char *p = (char*) *payloadp;
        //debug("payload: %s", p);
        if(read_more == -1) {return -1;}

        else if(read_more !=  y){
        //debug("short count 4");
        //handle the short count situation, i will make this a separate function 
        int missing_read1 = y - read_more;
        //debug("missing read: %d", missing_read1);
        while(read_more < y) {
            //debug("sike?");
            int readd1 = read(fd, *payloadp, missing_read1);
            if (readd1 == -1 || readd1 == 0) {
                return -1;
            }
            read_more += readd1;
            missing_read1 -= readd1;
            *payloadp += readd1;
        }


    }
    


    }
    else{*payloadp = NULL;}

    return 0;

}