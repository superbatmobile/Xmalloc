/* 
 * Simple, 32-bit and 64-bit clean allocator based on implicit free
 * lists, first-fit placement, and boundary tag coalescing, as described
 * in the CS:APP3e text. Blocks must be aligned to doubleword (8 byte) 
 * boundaries. Minimum block size is 16 bytes. 
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "mm.h"
#include "memlib.h"



/* Function prototypes for internal helper routines */
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void printblock(void *bp); 
static void checkheap(int verbose);
static void checkblock(void *bp);

team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Simon",
    /* First member's email address */
    "bovik@cs.cool.mail",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* 
 * mm_init - Initialize the memory manager 
 */
/* $begin mminit */
int mm_init(void) 
{
    /* Create the initial empty heap */
    int i;
    if ((heap_listp = mem_sbrk((SEG_LEN+3)*WSIZE)) == (void *)-1)
        return -1;
    for (i = 0; i < SEG_LEN; i++){
        PUT((heap_listp + (i*WSIZE)), NULL);
    }
    seg_list_head = heap_listp;

    PUT(heap_listp + ((i + 0) * WSIZE), PACK(DSIZE, 1)); /* Prologue header */ 
    PUT(heap_listp + ((i + 1) * WSIZE), PACK(DSIZE, 1)); /* Prologue footer */ 
    PUT(heap_listp + ((i + 2) * WSIZE), PACK(0, 1));     /* Epilogue header */
    heap_listp += (i+1) * WSIZE;                     //line:vm:mm:endinit  
    /* $end mminit */

#ifdef NEXT_FIT
    rover = heap_listp;
#endif
    /* $begin mminit */

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) 
        return -1;
    return 0;
}
/* $end mminit */

/* 
 * mm_malloc - Allocate a block with at least size bytes of payload 
 */
/* $begin mmmalloc */
void *mm_malloc(size_t size) 
{
    size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;      

    /* $end mmmalloc */
    if (heap_listp == 0){
        mm_init();
    }
    /* $begin mmmalloc */
    /* Ignore spurious requests */
    if (size == 0)
        return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE)                                          //line:vm:mm:sizeadjust1
        asize = 2*DSIZE;                                        //line:vm:mm:sizeadjust2
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE); //line:vm:mm:sizeadjust3

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {  //line:vm:mm:findfitcall
        place(bp, asize);                  //line:vm:mm:findfitplace
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize,CHUNKSIZE);                 //line:vm:mm:growheap1
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)  
        return NULL;                                  //line:vm:mm:growheap2
    place(bp, asize);                                 //line:vm:mm:growheap3
    return bp;
} 
/* $end mmmalloc */

/* 
 * mm_free - Free a block 
 */
/* $begin mmfree */
void mm_free(void *bp)
{
    /* $end mmfree */
    if (bp == 0) 
        return;

    /* $begin mmfree */
    size_t size = GET_SIZE(HDRP(bp));
    /* $end mmfree */
    if (heap_listp == 0){
        mm_init();
    }
    /* $begin mmfree */

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

/* $end mmfree */
/*
 * coalesce - Boundary tag coalescing. Return ptr to coalesced block
 */
/* $begin mmfree */
static void *coalesce(void *bp) 
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {            /* Case 1 */
        insert_node(bp);
        return bp;
    }

    else if (prev_alloc && !next_alloc) {      /* Case 2 */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        delete_node(NEXT_BLKP(bp));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(PREV_NODE(bp), NULL);
        PUT(NEXT_NODE(bp), NULL);
    }

    else if (!prev_alloc && next_alloc) {      /* Case 3 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        delete_node(PREV_BLKP(bp));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        PUT(PREV_NODE(bp), NULL);
        PUT(NEXT_NODE(bp), NULL);
    }

    else {                                     /* Case 4 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        delete_node(PREV_BLKP(bp));   
        delete_node(NEXT_BLKP(bp));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
        PUT(PREV_NODE(bp), NULL);
        PUT(NEXT_NODE(bp), NULL);
    }
    insert_node(bp);
    /* $end mmfree */
#ifdef NEXT_FIT
    /* Make sure the rover isn't pointing into the free block */
    /* that we just coalesced */
    if ((rover > (char *)bp) && (rover < NEXT_BLKP(bp))) 
        rover = bp;
#endif
    /* $begin mmfree */
    return bp;
}
/* $end mmfree */

/*
 * mm_realloc - Naive implementation of realloc
 */
void *mm_realloc(void *ptr, size_t size)
{
    size_t oldsize;
    void *newptr;

    /* If size == 0 then this is just free, and we return NULL. */
    if(size == 0) {
        mm_free(ptr);
        return 0;
    }

    /* If oldptr is NULL, then this is just malloc. */
    if(ptr == NULL) {
        return mm_malloc(size);
    }

    newptr = mm_malloc(size);

    /* If realloc() fails the original block is left untouched  */
    if(!newptr) {
        return 0;
    }

    /* Copy the old data. */
    oldsize = GET_SIZE(HDRP(ptr));
    if(size < oldsize) oldsize = size;
    memcpy(newptr, ptr, oldsize);

    /* Free the old block. */
    mm_free(ptr);

    return newptr;
}

/* 
 * The remaining routines are internal helper routines 
 */

/* 
 * extend_heap - Extend heap with free block and return its block pointer
 */
/* $begin mmextendheap */
static void *extend_heap(size_t words) 
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE; //line:vm:mm:beginextend
    if ((long)(bp = mem_sbrk(size)) == -1)  
        return NULL;                                        //line:vm:mm:endextend

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         /* Free block header */   //line:vm:mm:freeblockhdr
    PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */   //line:vm:mm:freeblockftr
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */ //line:vm:mm:newepihdr

    /* Coalesce if the previous block was free */
    return coalesce(bp);                                          //line:vm:mm:returnblock
}
/* $end mmextendheap */

/* 
 * place - Place block of asize bytes at start of free block bp 
 *         and split if remainder would be at least minimum block size
 */
/* $begin mmplace */
/* $begin mmplace-proto */
static void place(void *bp, size_t asize)
/* $end mmplace-proto */
{   
    delete_node(bp);
    size_t csize = GET_SIZE(HDRP(bp));   

    if ((csize - asize) >= (2*DSIZE)) { 
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
        insert_node(bp);
    }
    else { 
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
    return bp;
}
/* $end mmplace */

/* 
 * find_fit - Find a fit for a block with asize bytes 
 */
/* $begin mmfirstfit */
/* $begin mmfirstfit-proto */
static void *find_fit(size_t asize)
/* $end mmfirstfit-proto */
{
    int seg_index, size;
    char *node, *bp;
    seg_index = 0;
    size = asize;


    while (size > 1 && (seg_index < (SEG_LEN - 1) )){
        seg_index++;
        size >>= 1;
    }

    seg_index -= 4;
    seg_index = (seg_index > 0) ? seg_index : 0;
    seg_index = (seg_index < SEG_LEN) ? seg_index : SEG_LEN-1;

    while(seg_index < SEG_LEN){
        node= (char *)GET(NEXT_NODE(seg_list_head + seg_index*WSIZE));
        while (node){
            if (GET_SIZE(HDRP(node)) >= asize)
                return node;
            node = (char *)(GET(NEXT_NODE(node)));
        }
        seg_index ++;
    }
    return NULL;
}
/* $end mmfirstfit */

static void insert_node(char *bp)
{
    int seg_index, size;
    char *node;
    size = GET_SIZE(HDRP(bp));
    seg_index = 0;
    while (size > 1 && (seg_index < (SEG_LEN - 1) )){
        seg_index++;
        size >>= 1;
    }

    seg_index -= 4;
    seg_index = (seg_index > 0) ? seg_index : 0;
    seg_index = (seg_index < SEG_LEN) ? seg_index : SEG_LEN-1;

    node = seg_list_head + seg_index * WSIZE;
    while (GET(NEXT_NODE(node))){
        node = (char *)GET(NEXT_NODE(node));
        if ((unsigned int)node > (unsigned int)bp){
            char *next = node;
            node = (char *)GET(PREV_NODE(node));
            PUT(NEXT_NODE(node), bp);
            PUT(PREV_NODE(bp), node);
            PUT(NEXT_NODE(bp), next);
            PUT(PREV_NODE(next), bp);
            return;
        }
    }
    PUT(NEXT_NODE(node), bp);
    PUT(PREV_NODE(bp), node);
    PUT(NEXT_NODE(bp), NULL);    
}

static void delete_node(char *bp)
{
    if (GET(PREV_NODE(bp)) && GET(NEXT_NODE(bp))){
        PUT(NEXT_NODE(GET(PREV_NODE(bp))), GET(NEXT_NODE(bp)));
        PUT(PREV_NODE(GET(NEXT_NODE(bp))), GET(PREV_NODE(bp)));
    }else if (GET(PREV_NODE(bp))){
        PUT(NEXT_NODE(GET(PREV_NODE(bp))), NULL);
    }
    PUT(PREV_NODE(bp), NULL);
    PUT(NEXT_NODE(bp), NULL);
}


/* 
 * mm_checkheap - Check the heap for correctness
 */
void mm_checkheap(int verbose)  
{ 
    checkheap(verbose);
}

static void printblock(void *bp) 
{
    size_t hsize, halloc, fsize, falloc;

    checkheap(0);
    hsize = GET_SIZE(HDRP(bp));
    halloc = GET_ALLOC(HDRP(bp));  
    fsize = GET_SIZE(FTRP(bp));
    falloc = GET_ALLOC(FTRP(bp));  

    if (hsize == 0) {
        printf("%p: EOL\n", bp);
        return;
    }

    printf("%p: header: [%ld:%c] footer: [%ld:%c]\n", bp, 
           hsize, (halloc ? 'a' : 'f'), 
           fsize, (falloc ? 'a' : 'f')); 
}

static void checkblock(void *bp) 
{
    if ((size_t)bp % 8)
        printf("Error: %p is not doubleword aligned\n", bp);
    if (GET(HDRP(bp)) != GET(FTRP(bp)))
        printf("Error: header does not match footer\n");
}

/* 
 * checkheap - Minimal check of the heap for consistency 
 */
void checkheap(int verbose) 
{
    char *bp = heap_listp;

    if (verbose)
        printf("Heap (%p):\n", heap_listp);

    if ((GET_SIZE(HDRP(heap_listp)) != DSIZE) || !GET_ALLOC(HDRP(heap_listp)))
        printf("Bad prologue header\n");
    checkblock(heap_listp);

    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        if (verbose) 
            printblock(bp);
        checkblock(bp);
    }

    if (verbose)
        printblock(bp);
    if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp))))
        printf("Bad epilogue header\n");
}

static void check_lists()
{
    int i;
    for (i = 0; i < SEG_LEN; i++){
        printf("start to print SEG--%d lists\n",i);
        print_list(seg_list_head + i * WSIZE);
        printf("\n");
    }
}


static void print_list(char* root)
{
    if(GET(NEXT_NODE(root)))
        printf("ROOT: %p --> ", root);
    else {
        printf("ROOT: %p\n", root);
        return;
    }
    char *node = (char *)GET(root);
    while(GET(NEXT_NODE(node))){
        printf("%p ---> ", node);
        node = (char *)GET(NEXT_NODE(node));
    }
    printf("%p\n", node);
}


