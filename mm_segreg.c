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

/* single word (4) or double word (8) alignment */
// #define ALIGNMENT 8
#define ALIGNMENT 4

/* rounds up to the nearest multiple of ALIGNMENT */
// #define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x3)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t))) // 4 in -m32

/* Basic constants an macros */
#define WSIZE               4
#define DSIZE               8
#define CHUNKSIZE           (1<<12) // 4KB
// #define CHUNKSIZE           (1<<12) // 8KB

#define MAX(x,y)            ((x)>(y)?(x):(y))

#define PACK(size, alloc)   ((size)|(alloc))

#define GET(p)              (*(unsigned int*)(p))
#define PUT(p,val)          (*(unsigned int*)(p)=(val))

#define GET_SIZE(p)         (GET(p) & ~0x7)
#define GET_ALLOC(p)        (GET(p) & 0x1)
#define GET_ALLOC_PREV(p)   ((GET(p) & 0x2)>>1)

#define HDRP(bp)            ((char *)(bp)-WSIZE)
#define FTRP(bp)            ((char *)(bp) + GET_SIZE(HDRP(bp))-DSIZE)

#define NEXT_BLKP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp)       ((char *)(bp) - GET_SIZE(HDRP(bp)-WSIZE))

/*********************************************************
 * For REVIEWERS
 *********************************************************
 * CS 교재의 segregated-fit에 해당
 * size class는 power of 2로 4B초과는 하나의 class (total 12)
 * (3-4), (5-8), (9-16), (17-32), (33-64), (65-128), (129-256), (257-512), (513-Ki), (Ki+1-2Ki), (2Ki+1-4Ki), (4Ki+1~)
 * 분할 및 coalescing 수행
 * 할당시 footer 지움 - 최소 block size는 3B(할당시 H+Data, 해제시 H+Next_ptr+F)
 * 
 ********************************************************/

#define FREE_LIST_COUNT 12

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t size);
static void place(void *bp, size_t size);
static void stitch(void *bp);

static void insert_head(void *bp);
static void stitch_move(void *bp, void *np);


static char * heap_listp;

/* 
 * mm_init - initialize the malloc package.
 */

int mm_init(void)
{
    
    if ((heap_listp = mem_sbrk((FREE_LIST_COUNT+3)*WSIZE)) == (void *)-1){
        return -1;
    }
    PUT(heap_listp,0);
    PUT(heap_listp+(1*WSIZE),PACK((FREE_LIST_COUNT+2)*WSIZE,1));
    for (int i=0;i<FREE_LIST_COUNT;i++) {
        PUT(heap_listp+((i+2)*WSIZE),heap_listp);
    }
    PUT(heap_listp+(14*WSIZE),PACK(0,3));
    heap_listp += 2*WSIZE;

    // if (extend_heap(4) == NULL){
    //     return -1;
    // }

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

    PUT(HDRP(bp),PACK(size,0));
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
    size_t newsize = ALIGN(size + 3*DSIZE);
    
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
    PUT(HDRP(ptr),PACK(size,0));
    PUT(FTRP(ptr),PACK(size,0));
    coalesce(ptr);
}

static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(HDRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {
        insert_head(bp);
        return bp;
    } else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        
        // 앞쪽으로 꺼내기 score:77
        // stitch(NEXT_BLKP(bp));
        // insert_head(bp);
        // 순서 그대로 두기 score:83
        stitch_move(NEXT_BLKP(bp),bp);

        PUT(HDRP(bp),PACK(size,0));
        PUT(FTRP(bp),PACK(size,0));
    } else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        
        // 앞쪽으로 꺼내기 score:82
        // bp = PREV_BLKP(bp);
        // stitch(bp);
        // insert_head(bp);
        // 순서 그대로 두기 score:83
        bp = PREV_BLKP(bp);

        PUT(HDRP(bp),PACK(size,0));
        PUT(FTRP(bp),PACK(size,0));
    } else {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))) + GET_SIZE(HDRP(PREV_BLKP(bp)));
        
        // 앞쪽으로 꺼내기 score:83
        // stitch(PREV_BLKP(bp));
        // stitch(NEXT_BLKP(bp));
        // bp = PREV_BLKP(bp);
        // insert_head(bp);
        // prev에 두기 score:83
        stitch(NEXT_BLKP(bp));
        bp = PREV_BLKP(bp);
        // next에 두기 score:82
        // stitch(PREV_BLKP(bp));
        // stitch_move(NEXT_BLKP(bp),PREV_BLKP(bp));
        // bp = PREV_BLKP(bp);
        

        PUT(HDRP(bp),PACK(size,0));
        PUT(FTRP(bp),PACK(size,0));
    }
    return bp;
}

static void stitch(void *bp) {
    PUT_LONG(GET_LONG(bp)+DSIZE,GET_LONG(bp+DSIZE));
    PUT_LONG(GET_LONG(bp+DSIZE),GET_LONG(bp));
}

static void insert_head(void *bp) {
    PUT_LONG(bp+DSIZE,GET_LONG(heap_listp+DSIZE));
    PUT_LONG(GET_LONG(heap_listp+DSIZE),bp);
    PUT_LONG(bp,heap_listp);
    PUT_LONG(heap_listp+DSIZE,bp);
}

static void stitch_move(void *bp, void *np) {
    PUT_LONG(GET_LONG(bp)+DSIZE,np);
    PUT_LONG(GET_LONG(bp+DSIZE),np);
    memcpy(np,bp,2*DSIZE);
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
    copySize = GET_SIZE(HDRP(oldptr))-DSIZE;
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}

static void *find_fit(size_t size)
{
    void *bp;
    for (bp = (void *)GET_LONG(heap_listp+DSIZE);GET_SIZE(HDRP(bp))>0;bp = (void *)GET_LONG(bp+DSIZE)) {
        // if (GET_SIZE(HDRP(bp))>=size){
        if ((GET_ALLOC(HDRP(bp))==0) && (GET_SIZE(HDRP(bp))>=size)){
            return bp;
        }
    }
    return NULL;
}

static void place(void *bp, size_t size)
{
    size_t least_size = ALIGN(1 + 3*DSIZE);
    size_t current_size = GET_SIZE(HDRP(bp));

    if (current_size-size < least_size){
        stitch(bp);
        PUT(HDRP(bp),PACK(current_size,1));
        PUT(FTRP(bp),PACK(current_size,1));
    } else {
        PUT(HDRP(bp),PACK(size,1));
        PUT(FTRP(bp),PACK(size,1));

        // 남은 메모리 앞으로 빼기
        // stitch(bp);
        // bp = NEXT_BLKP(bp);
        // insert_head(bp);
        // 순서 유지
        stitch_move(bp,NEXT_BLKP(bp));
        bp = NEXT_BLKP(bp);

        PUT(HDRP(bp),PACK(current_size-size,0));
        PUT(FTRP(bp),PACK(current_size-size,0));
    }
}