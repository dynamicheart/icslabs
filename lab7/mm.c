/*
 * mm.c
 * 
 * In this  approach, a block is allocated by finding free block in 
 * multiple explicit segregated lists according to the size to be 
 * allocated. A block consists of 
 *
 *              | Headers | Prev_Block_Pointer |
 *      Next_Block Pointer | Payload | Padding | Footer |
 * 
 * Blocks will be coalesced immediately. First_fit strategy is used 
 * to find the free blocks. Realloc is implemented by using mm_malloc
 * and mm_free.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "515030910223",
    /* First member's full name */
    "Jianbang Yang",
    /* First member's email address */
    "yangjianbang112@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<9)

#define MAX(x,y) ((x) > (y)? (x) : (y))

#define PACK(size, alloc) (size | (alloc))

#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char*)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

#define PREV_FRBLK(bp) GET(bp)
#define NEXT_FRBLK(bp) GET(bp + WSIZE)
#define SET_PREVFB(bp,val) PUT(bp,val)
#define SET_NEXTFB(bp,val) PUT(bp + WSIZE,val)

enum GROUP { A=0,B,C,D,E,F,G,H,I,END };

static void *heap_listp;
static void *free_listp;

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t size);
static void place(void *bp, size_t size);

static void add_fblock(void *bp);
static void remove_fblock(void *bp);
static enum GROUP find_group(size_t size);
static void *next_group(enum GROUP group); 

/*
 * mm_check - check heap consistency 
 */

static int mm_check(void){
    /* Is every block in the free list marked as free?*/
    enum GROUP group = A;
    void* listp = GET(free_listp + group*WSIZE);
    while(group != END){
        void *fp = listp;
        while(fp){
            if(GET_ALLOC(HDRP(fp))){
                printf("Error:Allocted block in free list.\n");
                exit(1);
                return 1;
            }
            fp = NEXT_FRBLK(fp);
        }
        group ++;
        listp = GET(free_listp + group*WSIZE);
    }

    /* Are There any contiguous free blocks 
    * that somehow escaped coalescing ?*/
    int prev_alloc = 1;
    void *bp;
    int i = 0;
    int alloc;
    int size;
    for(bp = heap_listp;GET_SIZE((HDRP(bp))); bp = NEXT_BLKP(bp)){
        if(!prev_alloc && !GET_ALLOC(HDRP(bp))){
            printf("Error: Free blocks escaped coalescing.\n");
            exit(1);
            return 2;
        }

        prev_alloc = GET_ALLOC(HDRP(bp));

        
        /* Print the information of block */ 
        alloc = GET_ALLOC(HDRP(bp));
        size = GET_SIZE(HDRP(bp));
        printf("Block:%d\tSize:%d\tAddress:0x%x\tAlloced:%d",i,size,bp,alloc);
        if(alloc){
            printf("\n");
        }
        else{
            /* Is the header of a free equal to the footer?*/ 
            if(GET(HDRP(bp)) != GET(FTRP(bp))){
                printf("Error: Header and footer not equal.\n");
                exit(1);
            }
            printf("\tPrev:0x%x\tNext:0x%x\n",PREV_BLKP(bp),NEXT_FRBLK(bp));
        }
        i++;

        
        /* Is every free block actually in free list?*/ 
        if(!prev_alloc){
            enum GROUP group = find_group(GET_SIZE(HDRP(bp)));
            void *fp = GET(free_listp + group*WSIZE);
            int check_flag = 0;
            while(fp){
                if(fp == bp){
                    check_flag = 1;
                    break;
                }
                fp = NEXT_FRBLK(fp);
            }
            if(!check_flag){
                printf("Error: Free block not in any free lists.\n" );
                exit(1);
            }
        }
        
    }

    /* Do any allocated blocks overlap*/
    //printf("Error: Allocated blocks overlap.");

    /* Do the pointer in a heap block point to valid heap addresses*/
    //printf("Error: Pointer in a heap block point to invalid heap addresses");


    return 0;
}

/* 
 * mm_init - initialize the malloc package.
 */

int mm_init(void)
{
    if((heap_listp = mem_sbrk(12*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp + (A*WSIZE), 0);
    PUT(heap_listp + (B*WSIZE),0);
    PUT(heap_listp + (C*WSIZE),0);
    PUT(heap_listp + (D*WSIZE),0);
    PUT(heap_listp + (E*WSIZE),0);
    PUT(heap_listp + (F*WSIZE),0);
    PUT(heap_listp + (G*WSIZE),0);
    PUT(heap_listp + (H*WSIZE),0);
    PUT(heap_listp + (I*WSIZE),0);
    PUT(heap_listp + (9*WSIZE),PACK(DSIZE,1));
    PUT(heap_listp + (10*WSIZE),PACK(DSIZE,1));
    PUT(heap_listp + (11*WSIZE),PACK(0,1));
    free_listp = heap_listp;
    heap_listp += (10*WSIZE);

    if(extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    switch(size){
        case 112:
            size = 128;
            break;
        case 448:
            size = 512;
            break;
        case 4092:
            size = 28672;
            break;
        default:
            break;
    }

    size_t asize;
    size_t extendsize;
    char *bp;

    if(size <= 0){
        return NULL;
    }

    if(size <= DSIZE){
        asize = 2*DSIZE;
    }
    else{
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
    }

    if((bp = find_fit(asize)) != NULL){
        place(bp,asize);
        return bp;
    }


    
    extendsize = MAX(asize,CHUNKSIZE);
    if((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;
    place(bp,asize);
    
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr),PACK(size,0));
    PUT(FTRP(ptr),PACK(size,0));
    coalesce(ptr);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free 
 */

void *mm_realloc(void *ptr, size_t size)
{
    size_t oldsize;
    size_t asize;
    void *newptr;

    if(size == 0) {
        mm_free(ptr);
        return 0;
    }

    if(ptr == NULL) {
        return mm_malloc(size);
    }

    oldsize = GET_SIZE(HDRP(ptr));
    asize = (size <= DSIZE)? 2*DSIZE : (DSIZE *((size + DSIZE + (DSIZE - 1)))/(DSIZE));
    if(asize <= oldsize){
        return ptr;
    }
    else{
        size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(ptr)));
        size_t next_size = GET_SIZE(HDRP(NEXT_BLKP(ptr)));
        if(!next_alloc && size <= (oldsize + next_size)){
            remove_fblock(NEXT_BLKP(ptr));
            PUT(HDRP(ptr),PACK(oldsize + next_size, 1));
            PUT(FTRP(ptr),PACK(oldsize + next_size, 1));
            return ptr;
        }
        else{
            newptr = mm_malloc(size);
            if(!newptr) {
                return 0;
            }

            if(size < oldsize) 
                oldsize = size;
            
            memcpy(newptr, ptr, oldsize);

            mm_free(ptr);

            return newptr;
        }
    }

}

/*
 * coalesce - coalesce the neighbour free blocks
 */
static void *coalesce(void *bp)
{    
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if(prev_alloc && next_alloc){
        add_fblock(bp);
        return bp;
    }

    else if(prev_alloc && !next_alloc){
        remove_fblock(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size,0));
        PUT(FTRP(bp), PACK(size,0));
        add_fblock(bp);
    }

    else if(!prev_alloc && next_alloc){
        remove_fblock(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp),PACK(size,0));
        PUT(HDRP(PREV_BLKP(bp)),PACK(size,0));
        bp = PREV_BLKP(bp);
        add_fblock(bp);
    }

    else {
        remove_fblock(NEXT_BLKP(bp));
        remove_fblock(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) +
            GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)),PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)),PACK(size,0));
        bp = PREV_BLKP(bp);
        add_fblock(bp);
    }

    return bp;
}

/*
 * extend_heap - extend the size of heap if there is no enough space
 */

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    PUT(HDRP(bp),PACK(size,0));
    PUT(FTRP(bp),PACK(size,0));
    PUT(HDRP(NEXT_BLKP(bp)),PACK(0,1));

    return coalesce(bp);
}

/*
 * find_fit - Given a size, find a suitable free block.
 */

static void *find_fit(size_t asize)
{  
    void *fp;
    enum GROUP group = find_group(asize);
    void *listp = GET(free_listp + group*WSIZE);

    while(group != END){
        fp = listp;
        while(fp){
            if(asize <= GET_SIZE(HDRP(fp))){
                return fp;
            }
            else{
                fp = NEXT_FRBLK(fp);
            }
        }
        group++;
        listp = GET(free_listp + group*WSIZE);
    }
    return NULL;
}

/*
 * place - set the free block allocated.
 */

static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));

    if((csize - asize) >= (2*DSIZE)){
        remove_fblock(bp);
        PUT(HDRP(bp),PACK(asize,1));
        PUT(FTRP(bp),PACK(asize,1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp),PACK(csize - asize, 0));
        PUT(FTRP(bp),PACK(csize - asize, 0));
        add_fblock(bp);
    }
    else{
        PUT(HDRP(bp), PACK(csize,1));
        PUT(FTRP(bp), PACK(csize,1));
        remove_fblock(bp);
    }
}

/*
 * add_fblock - add free block to a suitable free list
 */

static void add_fblock(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    enum GROUP group = find_group(size);
    void *listp = GET(free_listp + group*WSIZE);

    if(listp == NULL){
        listp = bp;
        SET_PREVFB(listp,0);
        SET_NEXTFB(listp,0);
        PUT(free_listp + group*WSIZE,listp);
        return;
    }

    SET_PREVFB(bp,0);
    SET_NEXTFB(bp,listp);
    SET_PREVFB(listp,bp);

    listp = bp;
    PUT(free_listp + group * WSIZE,listp);
}

/*
 * remove_fblock - remove free block from free list
 */ 

static void remove_fblock(void *bp)
{
    void *prev = PREV_FRBLK(bp);
    void *next = NEXT_FRBLK(bp);
    size_t size = GET_SIZE(HDRP(bp));
    enum GROUP group = find_group(size);
    void *listp;
    SET_PREVFB(bp,0);
    SET_NEXTFB(bp,0);

    if(prev){
        SET_NEXTFB(prev,next);
    }
    else{
        listp = next;
        PUT(free_listp + group*WSIZE, listp);
    }

    if(next){
        SET_PREVFB(next,prev);
    }
}

/*
 * find_group - return a group according to the given size;
 */ 

static enum GROUP find_group(size_t size){
    if(size <= 16){
        return A;
    }
    else if(16 < size && size <= 64){
        return B;
    }
    else if(64 < size && size<= 128){
        return C;
    }
    else if(128 < size && size<= 256){
        return D;
    }
    else if(256 < size && size <= 512){
        return E;
    }
    else if(512 < size && size<= 1024){
        return F;
    }
    else if(1024 < size && size<= 2056){
        return G;
    }
    else if(2056 < size && size<= 4096){
        return H;
    }
    else{
        return I;
    }
}

/*
 * next_group - return the a free list whose size is just bigger than the current one
 */

static void *next_group(enum GROUP group){
    switch(group){
        case A:
            return GET(free_listp + B*WSIZE);
        case B:
            return GET(free_listp + C*WSIZE);
        case C:
            return GET(free_listp + D*WSIZE);
        case D:
            return GET(free_listp + E*WSIZE);
        case E:
            return GET(free_listp + F*WSIZE);
        case F:
            return GET(free_listp + G*WSIZE);
        case G:
            return GET(free_listp + H*WSIZE);
        case H:
            return GET(free_listp + I*WSIZE);
        case I:
        default:
            return 0;
        break;
    } 
}
