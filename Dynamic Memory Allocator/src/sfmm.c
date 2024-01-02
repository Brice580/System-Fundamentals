/**
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "sfmm.h"
#include <errno.h> //added this for ENOMEM error

int empty_flag = 0; //flag for empty heap (malloc not called yet)
void set_up(); //for setting up the sentiel nodes and the first page of memory
int get_findex(size_t size); //for finding the appropriate size class for num free lists
int get_qindex(size_t size);
void write_header_split(sf_block* block, size_t size, int alloc, int prvalloc); //used for writing headers for split blocks
void write_footer(sf_block* remainder);
void add_free(sf_block* block, size_t size);
sf_block* coalesce(sf_block* block);
void set_header_size(sf_block* blockz, uint64_t new);
void write_header(sf_block* write, size_t size);
int check_free(void* pp);
void update_epilogue(sf_block* block);
void flush(sf_block* block, int index);
void update_prev_alloc(sf_block* block, int status);
int power2(size_t x);

void *sf_malloc(size_t size) {
    
    //initial check for size
    if(size == 0){
        return NULL;
    }
    /*Here's my initial plan to implement this function:
        1. Check size 
        2. Initialize the heap(set sentiel) if necessary & Calculate size of block
        3. Check quicklists to see if it contains block of that size
        4. If its not there, check the main free lists 
        5. If no block is found, sf_mem_grow is called to extend the heap by a PAGE
        */
    
    int block_sz = size + 8; //use this for the size of the block + header/footer/next/prev : size is the payload 

    if(block_sz % 8 != 0){
        int remainder = block_sz % 8; // 8 - remainder is the padding bytes
        block_sz = block_sz + (8-remainder); //this essentially rounds up to the closest multiple of 8
    }
    /*Now we have to check for whether or not the block size is big enough for links + footer when freed (bigger than 32)*/
    if(block_sz < 32){
        block_sz = 32; //easiest way to deal with this case is to just set it 32 which is the absolute minimum. MIGHT NEED TO RECHECK THIS
    }

    /*Now its time to check if the heap is empty using the flag, might need a helper method!*/

    if(!empty_flag){
        /*Sentiel Nodes need to be initialized and sf_mem_grow needs to be called and prologue/epilogue*/
        set_up();
        if(sf_errno == ENOMEM)
            return NULL;

        empty_flag=1; //toggle the empty flag to show the first malloc was triggered already

    }

    /*Now we move onto the fun stuff*/

    /*CHECK THE QUICKLISTS TO SEE IF BLOCK OF THAT SIZE IS CONTAINED
      NUM_QUICK_LISTS =20
      QUICK_LIST_MAX = 5
      [32, 40, 48, 56, 64, 72, 80, 88, 96, 104, 112, 120, 128, 136, 144, 152, 160, 168, 176, 184] <-- Size classes*/

      if(block_sz <= 184){ //initial scan if it'll fit in one of the QUICK LISTS

        int qk_index = get_qindex(block_sz); //index of the quicklist

        if(sf_quick_lists[qk_index].length > 0){ //there is a block of the size
            
            sf_block* temp = sf_quick_lists[qk_index].first;//to modify the header and pointer to return
            sf_quick_lists[qk_index].first = sf_quick_lists[qk_index].first->body.links.next; //move head forward in list
            temp->header &= ~0x4; //clears 3rd lsb(qklist)

            sf_quick_lists[qk_index].length--; //update size

            return temp->body.payload; //is this right??, do we set payload in a diff function?
        }


      }

      /*QUICKLISTS DID NOT SUFFICE*/

      /*CHECK THE FREE LISTS - the hard part
        NUM_FREE_LISTS 10
        sf_block sf_free_list_heads[NUM_FREE_LISTS];*/

        int fr_index = get_findex(block_sz); //get the index for sf_free_lists

        for(int i=fr_index; i < NUM_FREE_LISTS; i++){
            //iterate through lists, starting at fr_index, might need a while loop to go through each list whole
            //we are using first fit
            //if block found, split it (if you can = splinter will not be less than 32 bytes)
                //insert the remainder (higher address) into free lists
            sf_block* sentiel = &sf_free_list_heads[i]; //sets the head for the while loop
            sf_block* cursor = (sf_block*) sf_free_list_heads[i].body.links.next; //cursor to the next

                //now start searching the list
                while(cursor != sentiel){
                    //look for a size of at least block_sz
                    int cursor_size = cursor->header & 0xFFFFFFF8; //gets the size of the block from header
                    //printf("%d", cursor_size);
                    
                    //now we check if the size >= blocksize, hence its a valid block to get mem from

                    if(cursor_size >= block_sz){
                        //valid, split and alloc
                        /*If splitting a block to be allocated would result in a splinter, then the block should
                        not be split; rather, the block should be used as-is to satisfy the allocation request*/
                        if(cursor_size - block_sz >= 32){
                            //the lower value addresses are returned as allocs, so the header down
                            //the higher value addresses are returned to mem list, so the footer up

                                //first off we should remove the block from the freelist.. no need to keep it in the free list at this point?
                                cursor->body.links.prev->body.links.next = cursor->body.links.next; //this sets the previous nodes next to the next node
                                cursor->body.links.next->body.links.prev = cursor->body.links.prev; //sets next's previous to previous
                                cursor->body.links.prev = NULL;
                                cursor->body.links.next = NULL;
                                
                                /*At this point, the block is removed from the free list and all links gone, now its time to split*/

                                write_header_split(cursor, block_sz, 1, 0);
                                
                                sf_block* remainder = (sf_block*) ((void*)cursor + block_sz); //new block after previous block?

                                write_header_split(remainder, cursor_size-block_sz, 0, 1); //write header for new block

                                write_footer(remainder);
                                // it need to be coalesced before adding...
                                //sf_show_heap();

                                remainder = coalesce(remainder);

                                add_free(remainder, remainder->header & 0xFFFFFFF8); 

                                update_epilogue(remainder);
                                update_prev_alloc(remainder, 0);
                                
                                //sf_show_heap();

                                return cursor->body.payload;


                                /*I have no idea if any of the above is working correnctly yet*/
                                //sf_show_heap();


                        }
                        else{ //split is not valid so just return the block whole, with the extra padding n all

                            cursor->body.links.prev->body.links.next = cursor->body.links.next; //this sets the previous nodes next to the next node
                            cursor->body.links.next->body.links.prev = cursor->body.links.prev; //sets next's previous to previous
                            cursor->body.links.prev = NULL;
                            cursor->body.links.next = NULL;

                            cursor->header |= 0x1; //sets the alloc bit to 1

                            update_prev_alloc(cursor, 1);

                            return cursor->body.payload; //returns payload


                        }
                        
                    }
                    
                    
                    cursor = cursor->body.links.next; //if not valid size, keep it moving

                }
        }

        //If it reached here, that means that there was no block was found, so must call sf_mem_grow()
        /*
        After coalescing this page with any free block that immediately precedes it, you should
        attempt to use the resulting block of memory to satisfy the allocation request;
        splitting it if it is too large and no splinter would result.  If the block of
        memory is still not large enough, another call to sf_mem_grow should be made;
        continuing to grow the heap until either a large enough block is obtained or the return
        value from sf_mem_grow indicates that there is no more memory.*/

    while(1){ //infinite loop
        //void* prev_end = sf_mem_end(); 
        //sf_show_heap();
        void* mem = sf_mem_grow();
        if(mem == NULL){
            break;
        }
        //mem is valid, so coalesce with previous free block, if any
        sf_block* epilogue = (sf_block*)((char*)mem - 8);
        sf_footer* test = (sf_footer*)((char*)mem - 16);
        if(((epilogue->header >> 1) & 1 ) == 0){ //free block exists immediately before, since prev alloc of epilogue is 0!

        //coalesce
            size_t last_size = *test & 0xFFFFFFF8; //sets size
            sf_block* previous_free = ((sf_block*)((char*)mem - last_size - 8)); //start of new mem - size of block - 8 for epilogue

            //if(PAGE_SZ+last_size-8 % 8 !=0){  -> MIGHT COME BACK TO DO A LATER CHECK

            //}
            size_t block_size = PAGE_SZ + last_size;
            write_header(previous_free, block_size);
       

        //remove old block
            previous_free->body.links.prev->body.links.next = previous_free->body.links.next; //this sets the previous nodes next to the next node
            previous_free->body.links.next->body.links.prev = previous_free->body.links.prev; //sets next's previous to previous
            previous_free->body.links.prev = NULL;
            previous_free->body.links.next = NULL;
        //set new footer
            sf_footer* last_footer = (sf_footer*)(sf_mem_end()-16);
            *last_footer = previous_free->header;
            
        //set old footer to null?
            test = NULL;
        //set a new epilogue
            sf_block* epi = (sf_block*)(sf_mem_end() - 8);
            epi->header = (epi->header & ~0x7) | 0;
            epi->header |= 0x1; //sets the alloc bit to 1
            epi->header &= 0xFFFFFFFD; //clears 2 lsb (prev_alloc)
            epi->header &= ~0x4; //clears 3 lsb(in qklst)
            if(epilogue != NULL)
                epilogue = NULL;
        //now try to alloc by checking if it satisfies the size, otherwise need to sf_grow again.
            //sf_show_heap();
            //ize_t new_size = previous_free->header & 0xFFFFFFF8; //new size of complete block

            if(block_sz <= block_size){ //if sufficiently large
                //now check split
                if(block_size - block_sz >=32){

                    //remove from free list is done already
                    write_header_split(previous_free, block_sz, 1, 0);
                    sf_block* remainder1 = (sf_block*) ((void*)previous_free + block_sz); //new block after previous block?

                    write_header_split(remainder1, block_size-block_sz, 0, 1); //write header for new block

                    write_footer(remainder1);
                                // it need to be coalesced before adding...                        sf_show_heap();

                    remainder1 = coalesce(remainder1);

                    add_free(remainder1, remainder1->header & 0xFFFFFFF8); 
                    update_epilogue(remainder1);
                                
                    update_prev_alloc(remainder1, 0);
                    //sf_show_heap();

                    return previous_free->body.payload;


                }
                else{//no split
                    previous_free->header |= 0x1; //sets the alloc bit to 1
                    update_epilogue(previous_free);
                    return previous_free->body.payload; //returns payload
                }
            }
            else{
                add_free(previous_free, block_size);
            }


            //if its not big enough for block size then you just run the loop bakc again
        }
        else{
            //no previous free block, just make a new block of mem and try to alloc?
            sf_block* new_blk = (sf_block*)((char*)mem + PAGE_SZ-8); //-8 for the epilogue
            new_blk->header = (new_blk->header & ~0x7) | (PAGE_SZ-8);
            new_blk->header |= 0x1; //sets the alloc bit to 1
            new_blk->header |= 0x2; //sets 2 lsb (prev_alloc) to 1
            new_blk->header &= ~0x4; //clears this bit too

            sf_footer* foot_new = (sf_footer*)((char*)new_blk+PAGE_SZ-16);
            *foot_new = new_blk->header;

            sf_block* epi = (sf_block*)(sf_mem_end() - 8);
            epi->header = (epi->header & ~0x7) | 0;
            epi->header |= 0x1; //sets the alloc bit to 1
            epi->header &= 0xFFFFFFFD; //clears 2 lsb (prev_alloc)
            epi->header &= ~0x4; //clears 3 lsb(in qklst)
            if(epilogue != NULL)
                epilogue = NULL;

            //try to alloc
            
            size_t new_siz3 = new_blk->header & 0xFFFFFFF8; //new size of complete block

            if(block_sz <= new_siz3){ //if sufficiently large
                //now check split
                if(new_siz3 - block_sz >=32){

                    //remove from free list is done already
                    write_header_split(new_blk, block_sz, 1, 0);

                    sf_block* remainder2 = (sf_block*) ((void*)new_blk + block_sz); //new block after previous block?

                    write_header_split(remainder2, new_siz3 - block_sz, 0, 1); //write header for new block

                    write_footer(remainder2);
                                // it need to be coalesced before adding...                        sf_show_heap();

                    remainder2 = coalesce(remainder2);

                    add_free(remainder2, remainder2->header & 0xFFFFFFF8); 
                                
                    update_epilogue(remainder2);
                    update_prev_alloc(remainder2, 0);


                    return remainder2->body.payload;


                }
                else{//no split

                    new_blk->header |= 0x1; //sets the alloc bit to 1
                    update_epilogue(new_blk);
                    update_prev_alloc(new_blk,1);
                    return new_blk->body.payload; //returns payload
                }
            }
            else{
                add_free(new_blk, new_siz3);
            }
            

            
        }


    }

    sf_errno = ENOMEM; //no mem left to allocate, cannot process request
    return NULL;
}

void sf_free(void *pp) {

    //First lets start by doing the proper checks - NOTE: POINTER IS POINTING TO THE PAYLOAD
    int y = check_free(pp);
    if(y == 1){
        abort(); //<- MIGHT NEED TO RE LOOK THIS AND MAKE SURE ITS USED CORRECTLY
    }

    sf_block* block_to_free = (sf_block*)(pp-8); //creates a pointer to block
    size_t size_free = block_to_free->header & 0xFFFFFFF8;

    if(size_free <= 184){ //initial scan if it'll fit in one of the QUICK LISTS

        int qk_index = get_qindex(size_free); //index of the quicklist

        if(sf_quick_lists[qk_index].length != QUICK_LIST_MAX){ //there is a block of the size
            
            //sf_block* temp = sf_quick_lists[qk_index].first;//to modify the header and pointer to return
            block_to_free->body.links.next = sf_quick_lists[qk_index].first; //put block behind first
            sf_quick_lists[qk_index].first = block_to_free;//set first to new block
            block_to_free->header |= 0x4; //sets 3rd lsb(qklist)
            
          
            sf_quick_lists[qk_index].length++; //update size
            //update_prev_alloc(block_to_free, 0);
        }
        else{

            flush(sf_quick_lists[qk_index].first, qk_index); //send the first node to flush, should be connected to all of them by .next
            //add into quick list after flush
            block_to_free->body.links.next = sf_quick_lists[qk_index].first; //put block behind first
            sf_quick_lists[qk_index].first = block_to_free;//set first to new block
            block_to_free->header |= 0x4; //sets 3rd lsb(qklist)
            //update_prev_alloc(block_to_free, 0);

            


        }

      }
      else{ 

        //bits set properly
        block_to_free->header = block_to_free->header & ~0x4; //clears the qklist bit
        block_to_free->header = block_to_free->header & ~0x1;
        //Added to the front of the appropriate free list
        

        //SET A FOOTER IDIOT

        sf_footer* footer4free = (sf_footer*)((char*)block_to_free+ size_free-8);
        *footer4free = block_to_free->header;

        block_to_free = coalesce(block_to_free);
        add_free(block_to_free, block_to_free->header & 0xFFFFFFF8);
        update_prev_alloc(block_to_free, 0);
        

      }

     



}

void flush(sf_block* head, int index){

    sf_block* ptr_head = head;
    sf_block* next = NULL;
    
    while(ptr_head != NULL){

        next = ptr_head->body.links.next;

        ptr_head->header &= ~0x4; //clear the bit for in qklist
        ptr_head->header &= ~0x1;

        //set a footer..?
        size_t siz = ptr_head->header & 0xFFFFFFF8;
        sf_footer* footer4free = (sf_footer*)((char*)ptr_head+siz-8);
        *footer4free = ptr_head->header;
        
        ptr_head = coalesce(ptr_head); //coalesce the block if possible

        sf_quick_lists[index].length--;
        sf_quick_lists[index].first = sf_quick_lists[index].first->body.links.next;

        //NEED TO ADD IT IN AFTERRRR REMOVING THE BLOCK FROM THE QK LIST ... BUT HOW??
        update_prev_alloc(ptr_head, 0);
        update_epilogue(ptr_head);
        add_free(ptr_head, ptr_head->header & 0xFFFFFFF8);

        ptr_head = next; //keep going to next block in list
        

        //sf_show_heap();
    }


}

void *sf_realloc(void *pp, size_t rsize) {

    
   
   int x = check_free(pp);

   if(x == 1){
    sf_errno = EINVAL;
    abort();
   }

   sf_block* ralloc = (sf_block*)(pp-8);
   size_t ralloc_size = ralloc->header & 0xFFFFFFF8;

   if(rsize == ralloc_size ){
    return ralloc->body.payload;
   }


   if(rsize == 0){
    sf_free(ralloc);
    return NULL;
   }

    int rsizeM = rsize + 8; //use this for the size of the block + header/footer/next/prev : size is the payload 

    if(rsizeM % 8 != 0){ //align
        int remainder = rsizeM % 8; // 8 - remainder is the padding bytes
        rsizeM = rsizeM + (8-remainder); //this essentially rounds up to the closest multiple of 8
    }
    if(rsizeM < 32)
        rsizeM = 32;



   //NOW TWO CASES FOR REALLOC:
   if(ralloc_size < rsizeM){

    void* init = sf_malloc(rsize); //POINTER TO THE PAYLOAD
    if(init == NULL){
        return NULL;
    }
    sf_block* block_init = (sf_block*)(init - 8);
    size_t mal_size = block_init->header & 0xFFFFFFF8;
    memcpy(block_init->body.payload, ralloc->body.payload, (mal_size-8)); //CHECK THIS TOO
    sf_free(pp);

    return block_init->body.payload;


   }
   else if(ralloc_size > rsizeM){
        
        //this time we not mallocing, just using the existing block
        //ralloc is block we are using , already defined 
        if(ralloc_size - rsizeM >= 32){
            //Split will NOT result in splinter so, split it
        
            
            /*At this point, the block is removed from the free list and all links gone, now its time to split*/

            write_header_split(ralloc, rsizeM, 1, 0);
            
            sf_block* remainder = (sf_block*) ((void*)ralloc + rsizeM); //new block after previous block?

            write_header_split(remainder, ralloc_size-rsizeM, 0, 1); //write header for new block

            write_footer(remainder); 
            // it need to be coalesced before adding...
            //sf_show_heap();

            remainder = coalesce(remainder);

            add_free(remainder, remainder->header & 0xFFFFFFF8); 

            update_epilogue(remainder);
            update_prev_alloc(remainder, 0);

            return ralloc->body.payload;
             
        }
        else{
            //NO splitting, just return the block normally? idk really cuz that doesnt really do anything 

            return ralloc->body.payload;

        } 
        

   }

    return  NULL;
}

void *sf_memalign(size_t size, size_t align) {

    if(size == 0){
        return NULL;
    }

    if(align < 8){

        sf_errno = EINVAL;
        return NULL;

    }

    if(power2(align) == 1){

        sf_errno = EINVAL;
        return NULL;

    }
     int block_idk = size + 8; //use this for the size of the block + header/footer/next/prev : size is the payload 

    if(block_idk % 8 != 0){
        int remainder = block_idk % 8; // 8 - remainder is the padding bytes
        block_idk = block_idk + (8-remainder); //this essentially rounds up to the closest multiple of 8
    }
    /*Now we have to check for whether or not the block size is big enough for links + footer when freed (bigger than 32)*/
    if(block_idk < 32){
        block_idk = 32; //easiest way to deal with this case is to just set it 32 which is the absolute minimum. MIGHT NEED TO RECHECK THIS
    }

    /*allocate a block whose size is at least the requested size, plus the alignment size, plus the minimum
    block size, plus the size required for a block header and footer*/
    void* big_malloc = sf_malloc(size + align + 32 + 8);

    sf_show_heap();
    //sf_show_heap();

    if(big_malloc == NULL){
        //errno already set 
        return NULL;
    }

    //I wanna get the header for this block so i can get the address of the end of the payload
    sf_block* bam = (sf_block*)(big_malloc - 8); 
    size_t size_bam = bam->header & 0xFFFFFFF8;

    //big_malloc is a pointer to the payload, which already has a header, which is where we should start ..

    //look for alligned value, starting at the payload ?

    //size_t counter = 0;

   
    if((uintptr_t)big_malloc % align != 0){ //NOT ALLIGNED WITH THE REQUESTED ALLIGNMENT, so.. traverse? <===== COME BACK TO CHECK THIS IDK IF IT

    //NOT ALIGNED, add 32 just so we can find address after the min block size
        big_malloc += 32; //garuntees there is space?
    
    //1.find new align address
        size_t mask = align - 1;
        void* aligned_block = (void*)(((uintptr_t)big_malloc + mask) & ~(mask)); //calculates next address FOR PAYLOAD

        big_malloc -=32;

    //2.make new block from new address of (size)

        size_t prior_size = (aligned_block - big_malloc); //size of the prior block BEHIND THE ALIGNED ADDRESS

        sf_block* align_block = (sf_block*)((char*)aligned_block - 8); //new block header 8 bytes before the aligned address
        write_header_split(align_block, size_bam - prior_size, 1, 0); 
        //allocated !

    //3. free the block at old address of size "prior_size"
            
        //check if this is 32?

        write_header_split(bam,prior_size, 0, 0); //write header for new block
        sf_show_heap();

        write_footer(bam); 
        sf_show_heap();

        bam = coalesce(bam);
        sf_show_heap();


        add_free(bam,bam->header & 0xFFFFFFF8); 

        sf_show_heap();
        update_prev_alloc(bam, 0);

        //first block before aligned block is free
        size_t updates_align = size_bam - prior_size;

        //header for new block is set, now check the size and see if its too big ..
        // ((char*)(align_block + block_idk)) < ((char*)big_malloc + size_bam - 8)
        if(updates_align > block_idk){ //aligned block with previous block freed is still too large, split lower half
            //check if there is more space past block size ending

            if(updates_align - block_idk >= 32){
                
                write_header_split(align_block, block_idk , 1, 0);

                sf_block* remainder3 = (sf_block*) ((void*)align_block + block_idk); //new block after previous "align" block?

                write_header_split(remainder3, updates_align - block_idk , 0, 1); //write header for new block

                write_footer(remainder3); 
                // it need to be coalesced before adding...
                //sf_show_heap();

                remainder3 = coalesce(remainder3);
                sf_show_heap();

                add_free(remainder3, remainder3->header & 0xFFFFFFF8); 
                sf_show_heap();

                update_epilogue(remainder3);
                update_prev_alloc(remainder3, 0);
                
                //sf_show_heap();

                return align_block->body.payload;


            }
            //else return payload?
            else{
                update_prev_alloc(align_block, 1);
                return align_block->body.payload;

            }

        }
        else{
            update_prev_alloc(align_block, 1);
            return align_block->body.payload;
        }
        
        /*
        and in addition is sufficiently far from the beginning of the block that the initial
        portion of the block can itself be split off into a block of at least minimum size
        and freed:*/

    }
    else{ 
        //It IS ALREADY ALLIGNED.. somehow.. so just split the block if its too big, like we normally do.
        

        if(size_bam > block_idk){ //bigger than needed block
            //check if there is more space past block size ending

            if(size_bam - block_idk >= 32){ //no splinters arise after subtracting needed size from the big blockb

                write_header_split(bam, block_idk , 1, 0);

                sf_block* remainder3 = (sf_block*) ((void*)bam + block_idk); //new block after previous block?

                write_header_split(remainder3, size_bam - block_idk, 0, 1); //write header for new block

                write_footer(remainder3); 
                // it need to be coalesced before adding...
                //sf_show_heap();

                remainder3 = coalesce(remainder3);

                add_free(remainder3,remainder3->header & 0xFFFFFFF8); 

                update_epilogue(remainder3);
                update_prev_alloc(remainder3, 0);
                
                //sf_show_heap();

                return bam->body.payload;


            }
            //else return payload?

        }
        else{
            update_prev_alloc(bam, 1);
            return bam->body.payload;
        }
        
    

    }
    
        //make sure it has enough space after for the payload

    //if space before alligned value is at least min block size, free it

    //if block still too big, split the block normally

   return NULL;
}

int power2(size_t x){ //implement this

    if(x == 0){
        return 1;
    }
    else if((x & (x-1)) == 0){ //x is only one bit and (x-1) is all 1 except for the one bit in x, so should equal 0 
        return 0;
    }
    return 1;

}

void set_header_size(sf_block* blockz, uint64_t new) {
    // save the original value of the long

    blockz->header = (blockz->header & ~0x7) | (PAGE_SZ + new - 8);

}

void update_epilogue(sf_block* block){

    //this method is just to update the epilgoue prev alloc bit

    size_t size1 = block->header & 0xFFFFFFF8;

    sf_block* epi_pointer = (sf_block*)(sf_mem_end() - 8);
    
    if((sf_block*)(sf_mem_end()- size1-8) == block){ //block is the last block

        if((block->header & 1) == 1){ //alloc bit is 1
            epi_pointer->header |= 0x2; //sets prev alloc to 1
        }
        else{
            epi_pointer->header &= ~0x2;
        }

    }
}


void set_up(){

    /*1. Mem Grow 
      2. Initialize Heads
      3. Separate prologue, epilogue*/

    void* mem_ptr = sf_mem_grow(); //new PAGE_SZ (4096 BYTES of mem)

    if(mem_ptr == NULL){ //checks if the memory failed, therefore return

        sf_errno = ENOMEM;
        //return NULL;
    }

    /*Now set up the heads of the MAIN free lists 
    struct sf_block sf_free_list_heads[NUM_FREE_LISTS]; <-- FOR REFERENCE
    */

   for(int i =0; i < NUM_FREE_LISTS; i++){

    sf_free_list_heads[i].body.links.prev = &sf_free_list_heads[i]; //sets prev to itself
    sf_free_list_heads[i].body.links.next = &sf_free_list_heads[i]; //sets next to itself
    //printf("Address of sentiel: %p\n", (void *)&sf_free_list_heads[i]);


   }
    /*The main free lists are set up and are ready for memory!! */
    /*Split page into prologue and epilogue*/

        //sf_mem_start and sf_mem_end will return the 2 start/end addresses of the page...
    
    sf_block* pro = (sf_block*) sf_mem_start(); //points to the starting address
    sf_block* ep = (sf_block*) (sf_mem_end() - 8); //points to the last address with space for a header(8 bytes only)

    /*The first block of the heap is the "prologue", which is an allocated block of minimum size with an unused payload area.*/

    pro->header = (pro->header & ~0x7) | 32;//sets the bits and perserves the last 3
    pro->header |= 0x1; //sets the alloc bit to 1
    pro->header &= 0xFFFFFFFD; //clears 2 lsb (prev_alloc)
    pro->header &= ~0x4; //clears 3 lsb(in qklst)

    ep->header = (ep->header & ~0x7) | 0;
    ep->header |= 0x1; //sets the alloc bit to 1
    ep->header &= 0xFFFFFFFD; //clears 2 lsb (prev_alloc)
    ep->header &= ~0x4; //clears 3 lsb(in qklst)

    //first block in the heap should be the prologue- which it is now of size 32 bytes
    //epilogue is at the end as it should be

    //sf_show_heap();
    /*Now to add the rest of memory into the freelist. */

    int insert = get_findex(PAGE_SZ - 40); //this SHOULD be the memory left behind since Prologue(32) + Epilogue(8) = 40?
    //index found at this point for main free lists
    
    sf_block* init_block = (sf_block*) (sf_mem_start() + 32); //points the new block starting at address after it
    init_block->header = (init_block->header & 0x7) | ((PAGE_SZ-40) & ~(uint64_t)0x7); // does this work?
    init_block->header &= ~0x1;
    init_block->header |= 0x2;
    init_block->header &= ~0x4;
    //printf("Address of header: %p\n", &init_block->header);

    //printf("Header value: %lx\n", init_block->header);
    size_t block_size = init_block->header & ~0x7;
    sf_footer* footer = (sf_footer*) ((char*) init_block + block_size - 8);
    *footer = init_block->header; // Set the footer value  
    

    

    //ALL there is left to do is add it to the main free list

    sf_block* ptr = (sf_block*) init_block; //pointer to init block since it doesnt let me use non pointer types
    
    // printf("Before insertion:\n");
    // printf("ptr->body.links.next = %p\n", ptr->body.links.next);
    // printf("ptr->body.links.prev = %p\n", ptr->body.links.prev);
    // printf("sf_free_list_heads[%d].body.links.prev = %p\n", insert, sf_free_list_heads[insert].body.links.prev);
    // printf("sf_free_list_heads[%d].body.links.next = %p\n", insert, sf_free_list_heads[insert].body.links.next);

    ptr->body.links.next = &sf_free_list_heads[insert];
    ptr->body.links.prev = sf_free_list_heads[insert].body.links.prev;
    sf_free_list_heads[insert].body.links.prev->body.links.next = ptr;
    sf_free_list_heads[insert].body.links.prev = ptr;

    // printf("\nAfter insertion:\n");
    // printf("ptr->body.links.next = %p\n", ptr->body.links.next);
    // printf("ptr->body.links.prev = %p\n", ptr->body.links.prev);
    // printf("sf_free_list_heads[%d].body.links.prev = %p\n", insert, sf_free_list_heads[insert].body.links.prev);
    // printf("sf_free_list_heads[%d].body.links.next = %p\n", insert, sf_free_list_heads[insert].body.links.next);


    //^idk if this right but it looks right
        //sf_show_heap();

    return; //return back - set up is done.
    
}

int get_findex(size_t size){

    int min_size = 32; //32 bytes is min block size and also index 1

    if(size == min_size){
        return 0; //0th index is M which equals 32 bytes
    }

    /*(M,2M] (2M, 4M] (4M, 8M]...*/
    int index =1;
    int i = 1;
    int j = 2;
    //use min size as the range
    while(index < NUM_FREE_LISTS-1){

        if(size > min_size*i && size <= min_size*j){
            return index;
        }
        else{
            i *= 2;
            j *= 2;
            index++;
        }

        
    }
    return index; //will return 9
}

int get_qindex(size_t size){

    int start = 32;
    int counter = 0;

    while(counter < NUM_QUICK_LISTS){

        if(size == start){
            return counter;
        }
        else{
            start += 8;
            counter++;
        }
    }

    return -1; //error?

}

void write_header(sf_block* write, size_t size){

    uint64_t first_lsb = write->header & 1; // extracts the first LSB (bit 0)
    uint64_t second_lsb = (write->header >> 1) & 1; // extracts the second LSB (bit 1)
    uint64_t third_lsb = (write->header >> 2) & 1; // extracts the third LSB (bit 2)

    write->header = (write->header & 0x7) | ((size & ~(uint64_t)0x7));
    
    if(first_lsb == 1){
        write->header |= 0x1;
    }
    else
        write->header &= ~0x1;

    if(second_lsb == 1)
        write->header |= 0x2;
    else
        write->header &= ~0x2;
    
    if(third_lsb ==1)
        write->header |= 0x4;
    else   
        write->header &= ~0x4;


}

void write_header_split(sf_block* block, size_t size, int alloc, int prvalloc){
    //from my understanding, you should just keep the prev alloc bit as is, since only the header is changing, but address stays the same
    //qk list is just set to 0
    block->header = (block->header & 0x7) | ((uint64_t)size & ~0x7);//should set the header to the new size?
    if(alloc == 1){
        block->header |= 0x1; //sets the bit to 1
    }
    else{
        block->header &= ~0x1;
    }

    if(prvalloc == 1){
        block->header |= 0x2; //sets the prv alloc bit to 1
    }



}

void write_footer(sf_block* remainder){
    
    size_t block_size = remainder->header & ~0x7;
    sf_footer* footer = (sf_footer*) ((char*) remainder + block_size - 8);
    *footer = remainder->header;


    
}

void add_free(sf_block* block, size_t size){

    int x = get_findex(size);

    block->body.links.prev = &sf_free_list_heads[x];
    block->body.links.next = sf_free_list_heads[x].body.links.next;
    sf_free_list_heads[x].body.links.next->body.links.prev = block;
    sf_free_list_heads[x].body.links.next = block;

}

sf_block* coalesce(sf_block* block){
    //Four cases
    /*
    Case 1: Both previous and next block in heap is alloc
    Case 2: Previous is free and next is alloc
    Case 3: Next is free and previous is alloc
    Case 4: Prev and next are free*/

    //First check the previous alloc bit
    size_t prev_alloc  = (block->header & 0x2) >> 1; //sets the prev alloc to either 1 or 0
    size_t size_current = block->header & 0xFFFFFFF8; //size of the current block
    size_t next_alloc = ((sf_block*)((char*)block + size_current))->header & 0x1; //gets the ALLOC bit of the next block

    //printf("%ld",next_alloc);
    //printf("%ld", prev_alloc);
    if(prev_alloc && next_alloc){ //case 1, both alloced so no coalesce allowed, just return the block

        return block;

    }
    else if(!prev_alloc && next_alloc){ //case 2, prev is free and next is alloc, so coalesce with the previous

        //calculate the size of the previous block
        sf_footer* previous_footer = ((sf_footer*)((char*)block - 8)); //pointer to the previous block
        size_t previous_size = *previous_footer & 0xFFFFFFF8; //sets the previous size?
        sf_block* previous = ((sf_block*)((char*)block - previous_size)); //block minus the size stored in the footer of the previous block
        

        //remove the previous block from the free list
        previous->body.links.prev->body.links.next = previous->body.links.next; //this sets the previous nodes next to the next node
        previous->body.links.next->body.links.prev = previous->body.links.prev; //sets next's previous to previous
        previous->body.links.prev = NULL;
        previous->body.links.next = NULL;

        //update the previous block header

        previous->header =  (previous->header & 0x7) | (((size_current+previous_size) & ~(uint64_t)0x7));

        //edit footer at the previous footer location and set that equal to the header

        sf_footer* current_footer = (sf_footer*)((char*)block+(size_current-8));
        *current_footer = previous->header;

        //set the old footer and block to null?

        block = NULL;
        previous_footer = NULL;

        return previous;
        

    }
    else if(prev_alloc && !next_alloc){

        //calculate size of the next block
        sf_block* next_b = (sf_block*)((char*) block + size_current);
        size_t next_sz = next_b->header & 0xFFFFFFF8;
        sf_footer* next_foot = (sf_footer*)((char*)next_b +(next_sz-8));

        //remove next block from free list

        next_b->body.links.prev->body.links.next = next_b->body.links.next; //this sets the previous nodes next to the next node
        next_b->body.links.next->body.links.prev = next_b->body.links.prev; //sets next's previous to previous
        next_b->body.links.prev = NULL;
        next_b->body.links.next = NULL;
        //update current header
        block->header =  (block->header & 0x7) | (((size_current+next_sz) & ~(uint64_t)0x7));
        //edit footer of next block
        *next_foot = block->header;
        //set old footer and block to null, in this case, the next block
        sf_footer* current_foot = (sf_footer*)((char*)block+(size_current-8));
        next_b= (sf_block*)*current_foot;
        current_foot = NULL;

        next_b = NULL;
        
        return block;
    }
    else if(!prev_alloc && !next_alloc){
        //calculate sizes of both previous and next blocks
        sf_footer* previous_footer = ((sf_footer*)((char*)block - 8)); //pointer to the previous block
        size_t previous_size = *previous_footer & 0xFFFFFFF8; //sets the previous size?
        sf_block* previous = ((sf_block*)((char*)block - previous_size)); //block minus the size stored in the footer of the previous block

        sf_block* next_b = (sf_block*)((char*) block + size_current);
        size_t next_sz = next_b->header & 0xFFFFFFF8;
        sf_footer* next_foot = (sf_footer*)((char*)next_b +(next_sz-8));

        //remove BOTH blocks from free lists
        previous->body.links.prev->body.links.next = previous->body.links.next; //this sets the previous nodes next to the next node
        previous->body.links.next->body.links.prev = previous->body.links.prev; //sets next's previous to previous
        previous->body.links.prev = NULL;
        previous->body.links.next = NULL;

        next_b->body.links.prev->body.links.next = next_b->body.links.next; //this sets the previous nodes next to the next node
        next_b->body.links.next->body.links.prev = next_b->body.links.prev; //sets next's previous to previous
        next_b->body.links.prev = NULL;
        next_b->body.links.next = NULL;


        //update previous header
        previous->header =   (previous->header & 0x7) | (((size_current+previous_size+next_sz) & ~(uint64_t)0x7)); //updates the header 

        //update next footer
        *next_foot = previous->header;

        //set old footer and header to null, as well as current and next 
        sf_footer* current_foot = (sf_footer*)((char*)block+(size_current-8));
        *previous_footer = *current_foot; //just to satisfy lint
        current_foot = NULL;

        block = NULL;
        previous_footer = NULL;
        next_b = NULL;

        return previous;

    }


    return NULL;

}

int check_free(void* pp){

    sf_block* check = (sf_block*)(pp-8);

    size_t sizee = check->header & 0xFFFFFFF8;

    if(check == NULL){
        return 1;
    }
    else if(((uintptr_t)pp & 0x7) != 0){
        return 1;
    }
    else if(sizee < 32){
        return 1;
    }
    else if(sizee % 8 != 0){
        return 1;
    }
    /*The header of the block is before the start of the first block of the heap,
    or the footer of the block is after the end of the last block in the heap.
    ^Does this just mean prologue and epilogue*/
    else if(check < (sf_block*)sf_mem_start()){ //before prologue
        return 1; 
    }
    else if(((sf_footer*)((char*)check+sizee-8)) > (sf_footer*)sf_mem_end()){ //footer after epilogue <check these
        return 1;
    }
    /*
    The allocated bit in the header is 0.
    The in quick list bit in the header is 1
    */
   else if((check->header & 1) == 0){
        return 1;
   }
   else if(((check->header >>2) & 1) == 1){
        return 1;
   }
   //still need one case
   /*For the point in question, if the current block's prev_alloc bit is 0, it would be enough to just check that the footer of the previous block 
   has its alloc bit also set to 0. If this is the case, then all is well, and if not, then you should treat it as an invalid pointer.*/
    else if(((check->header >> 1) & 1) == 0){
         
         sf_footer* previ_footer = ((sf_footer*)((char*)check - 8)); //previous footer
        if(((*previ_footer >> 2) & 1)!=0){

            return 1;
        }

    }


    return 0;
}

void update_prev_alloc(sf_block* block, int status){

    //this method is just to update the prev alloc bit of the next block

    //HAVE TO CHECK IF THE BLOCK IS FREE SINCE YOUD HAVE TO UPDATE THE FOOTER TOOO
    size_t size1 = block->header & 0xFFFFFFF8;

    sf_block* pointer = (sf_block*)((char*)block + size1);
    size_t first_bit = pointer->header & 1; //use this to check whether block is alloc or not
    if(first_bit == 1){
        if(status == 0){

            pointer->header = pointer->header & ~0x2;

        }
        else{

            pointer->header |= 0x2;

        }
    }
    else if(first_bit == 0){
        
        size_t size_new = pointer->header & 0xFFFFFFF8;
        sf_footer* foota = (sf_footer*)((char*)pointer+ size_new-8);


        if(status == 0){

            pointer->header = pointer->header & ~0x2;
            *foota = pointer->header;

        }
        else{

            pointer->header |= 0x2;
            *foota = pointer->header;
        }

    }
}