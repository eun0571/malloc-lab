/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
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
    "4zo tuna",
    /* First member's full name */
    "Choi wangyu",
    /* First member's email address */
    "wangyu7958@naver.com",
    /* Second member's full name (leave blank if none) */
    "Kim hyeda",
    /* Second member's email address (leave blank if none) */
    "skdltm622@gmail.com"
};

/*********************************************************
 * For REVIEWERS
 *********************************************************
 * CS 교재의 segregated-fit에 해당
 * size class는 power of 2로 4KB초과는 하나의 class (total 10)
 * (16), (17-32), (33-64), (65-128), (129-256), (257-512), (513-Ki), (Ki+1-2Ki), (2Ki+1-4Ki), (4Ki+1~)
 * 분할 및 coalescing 수행
 * 할당시 footer 지움(괜히 했음..)
 * __builtin_clz 사용했으므로 gcc나 clang에서만 동작
 ********************************************************/

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Basic constants an macros */
#define WSIZE               4
#define DSIZE               8
#define CHUNKSIZE           (1<<12) // 4KB

#define MAX(x,y)            ((x)>(y)?(x):(y))
#define MIN(x,y)            ((x)>(y)?(y):(x))

#define PACK(size, alloc)   ((size)|(alloc))

#define GET(p)              (*(unsigned int*)(p))
#define PUT(p,val)          (*(unsigned int*)(p)=(unsigned int)(val))

#define GET_SIZE(p)         (GET(p) & ~0x7)

#define GET_ALLOC(p)        (GET(p) & 0x1)
#define GET_ALLOC_PREV(p)   (GET(p) & 0x2)  // footer가 없어 prev_blk의 할당여부 표시

#define HDRP(bp)            ((char *)(bp)-WSIZE)
#define FTRP(bp)            ((char *)(bp) + GET_SIZE(HDRP(bp))-DSIZE)

#define NEXT_BLKP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp)       ((char *)(bp) - GET_SIZE(HDRP(bp)-WSIZE))

#define FREE_LIST_COUNT 10

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t size);
static void place(void *bp, size_t size);
static void stitch(void *bp);
int size_class_index(size_t size);
static void insert_to_list(void *bp);


static char * heap_listp;

/* 
 * mm_init - initialize the malloc package.
 */

int mm_init(void)
{
    
    if ((heap_listp = mem_sbrk((FREE_LIST_COUNT+4)*WSIZE)) == (void *)-1){
        return -1;
    }
    PUT(heap_listp,0);
    PUT(heap_listp+(1*WSIZE),PACK((FREE_LIST_COUNT+2)*WSIZE,1));
    for (int i=0;i<FREE_LIST_COUNT;i++) {
        PUT(heap_listp+((i+2)*WSIZE),heap_listp+2*WSIZE);
    }
    PUT(heap_listp+((FREE_LIST_COUNT+2)*WSIZE),PACK((FREE_LIST_COUNT+2)*WSIZE,1));
    PUT(heap_listp+((FREE_LIST_COUNT+3)*WSIZE),PACK(0,3));
    heap_listp += 2*WSIZE;

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL){
        return -1;
    }
    return 0;
}

static void *extend_heap(size_t words) 
{
    char *bp;
    size_t size;

    size = (words % 2)? (words+1)*WSIZE : words * WSIZE;

    if ((long)(bp = mem_sbrk(size))==-1) {
        return NULL;
    }

    PUT(HDRP(bp),PACK(size,GET_ALLOC_PREV(HDRP(bp))));
    PUT(FTRP(bp),PACK(size,0));
    PUT(HDRP(NEXT_BLKP(bp)),PACK(0,1));

    return coalesce(bp);
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    char *bp;
    size_t extend_size;

    if (size==0) {
        return NULL;
    }

    size_t newsize = ALIGN(size + WSIZE);

    if (newsize<=16) {
        newsize = 16;
    }
    
    if ((bp = find_fit(newsize))!=NULL){
        place(bp,newsize);
        return bp;
    }

    extend_size = MAX(newsize,CHUNKSIZE);
    if ((bp = extend_heap(extend_size/WSIZE)) == NULL)
	    return NULL;
    place(bp,newsize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr),PACK(GET_SIZE(HDRP(ptr))+GET_ALLOC_PREV(HDRP(ptr)),0));
    PUT(FTRP(ptr),PACK(size,0));
    coalesce(ptr);
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC_PREV(HDRP(bp));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {
        PUT(HDRP(bp),PACK(GET(HDRP(bp)),2));
        PUT(HDRP(NEXT_BLKP(bp)),PACK(GET_SIZE(HDRP(NEXT_BLKP(bp))),1));
        insert_to_list(bp);
        
    } else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        
        stitch(NEXT_BLKP(bp));
        PUT(HDRP(bp),PACK(size,2));
        PUT(FTRP(bp),PACK(size,0));
        insert_to_list(bp);

    } else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        
        bp = PREV_BLKP(bp);
        stitch(bp);
        PUT(HDRP(bp),PACK(size,2));
        PUT(FTRP(bp),PACK(size,0));
        PUT(HDRP(NEXT_BLKP(bp)),PACK(GET_SIZE(HDRP(NEXT_BLKP(bp))),1));
        insert_to_list(bp);

    } else {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        
        stitch(PREV_BLKP(bp));
        stitch(NEXT_BLKP(bp));
        bp = PREV_BLKP(bp);
        PUT(HDRP(bp),PACK(size,2));
        PUT(FTRP(bp),PACK(size,0));
        PUT(HDRP(NEXT_BLKP(bp)),PACK(GET_SIZE(HDRP(NEXT_BLKP(bp))),1));
        insert_to_list(bp);

    }
    return bp;
}

static void stitch(void *bp) {
    
    int index = size_class_index(GET_SIZE(HDRP(bp)));

    if (GET(bp)==(int)heap_listp && GET(bp+WSIZE)==(int)heap_listp) {
        PUT(heap_listp+(index*WSIZE),heap_listp);
    } else if (GET(bp)==(int)heap_listp) {
        PUT(GET(bp+WSIZE),heap_listp);
        PUT(heap_listp+(index*WSIZE),GET(bp+WSIZE));
    } else if (GET(bp+WSIZE)==(int)heap_listp) {
        PUT(GET(bp)+WSIZE,heap_listp);
    } else {
        PUT(GET(bp)+WSIZE,GET(bp+WSIZE));
        PUT(GET(bp+WSIZE),GET(bp));    
    }

}

int size_class_index(size_t size) {
    int result = 27 - __builtin_clz(1<<(32 - __builtin_clz(size-1)));
    return MIN(result,9);
}

static void insert_to_list(void *bp) {

    int index = size_class_index(GET_SIZE(HDRP(bp)));
    
    if (GET(heap_listp+(index*WSIZE))==(int)heap_listp) {
        PUT(heap_listp+(index*WSIZE),bp);
        PUT(bp,heap_listp);
        PUT(bp+WSIZE,heap_listp);
    } else {
        PUT(GET(heap_listp+(index*WSIZE)),bp);
        PUT(bp+WSIZE,GET(heap_listp+(index*WSIZE)));

        PUT(heap_listp+(index*WSIZE),bp);
        PUT(bp,heap_listp);
    }

}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    copySize = GET_SIZE(HDRP(oldptr))-WSIZE;
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

static void *find_fit(size_t size)
{
    void *bp;

    for (int index = size_class_index(size); index<FREE_LIST_COUNT; index++) {
        for (bp = (void *)GET(heap_listp+(index*WSIZE)); bp != heap_listp; bp = (void *)GET(bp+WSIZE)) {
            if (GET_SIZE(HDRP(bp))>=size){
                return bp;
            }
        }
    }
    return NULL;
}

// footer없으니 할당할때 next block header에 표시
static void place(void *bp, size_t size)
{
    size_t least_size = 16;
    size_t current_size = GET_SIZE(HDRP(bp));

    if (current_size-size < least_size){

        stitch(bp);
        PUT(HDRP(bp),PACK(current_size,3));
        PUT(HDRP(NEXT_BLKP(bp)),PACK(GET(HDRP(NEXT_BLKP(bp))),2));

    } else {

        stitch(bp);
        PUT(HDRP(bp),PACK(size,3));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp),PACK(current_size-size,2));
        PUT(FTRP(bp),PACK(current_size-size,0));
        PUT(HDRP(NEXT_BLKP(bp)),PACK(GET(HDRP(NEXT_BLKP(bp))),2));
        insert_to_list(bp);

    }
}