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
 * PREV_BLK으로 넘어갈 일이 없으니 format에 footer는 필요없어 사용 x
 * allocated block은 Header+data, free block은 Header+PREV+NEXT
 * size class는 power of 2이다.
   buddy system에서 size class는 할당요청을 커버하는 모든 size class가 존재해야하는데
   trace에서 확인해보면 random*.rep 생성시 args없으면 1<<15가 max block size인데 Makefile에 따로 args가 없는 듯 함.
   realloc*.rep 생성에서는 realloc을 최대 614,784까지 요청
   따라서 1MB까지 size class있어야함. (1<<20)
 * free block에 Header, Prev, Next 있어 최소 사이즈 16B(1<<4)
   (1<<4), (1<<5), (1<<6), (1<<7), (1<<8), (1<<9), (1<<10), ... (1<<19), (1<<20)
   총 17개의 size class존재
 * 분할 및 coalescing은 2의 제곱수 기준으로 수행
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
#define PUT(p,val)          (*(unsigned int*)(p)=(int)(val))

#define GET_SIZE(p)         (GET(p) & ~0x7)

#define GET_ALLOC(p)        (GET(p) & 0x1)
#define GET_ALLOC_PREV(p)   (GET(p) & 0x2)

#define HDRP(bp)            ((char *)(bp)-WSIZE)
// footer 안씀
// #define FTRP(bp)            ((char *)(bp) + GET_SIZE(HDRP(bp))-DSIZE)

#define NEXT_BLKP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp)       ((char *)(bp) - GET_SIZE(HDRP(bp)-WSIZE))

#define FREE_LIST_COUNT 17

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
    
    if ((heap_listp = mem_sbrk((FREE_LIST_COUNT+3)*WSIZE)) == (void *)-1){
        return -1;
    }
    PUT(heap_listp,0);
    PUT(heap_listp+(1*WSIZE),PACK((FREE_LIST_COUNT+1)*WSIZE,1));
    for (int i=0;i<FREE_LIST_COUNT;i++) {
        PUT(heap_listp+((i+2)*WSIZE),heap_listp+2*WSIZE);
    }
    PUT(heap_listp+((FREE_LIST_COUNT+2)*WSIZE),PACK(0,1));
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

    PUT(HDRP(bp),PACK(size,0));
    // PUT(FTRP(bp),PACK(size,0));
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
    // size_t extend_size;

    if (size==0) {
        return NULL;
    }

    size_t newsize = ALIGN(size + WSIZE);
    newsize = 1<<(32-__builtin_clz(newsize-1));

    if (newsize<=16) {
        newsize = 16;
    }
    
    if ((bp = find_fit(newsize))!=NULL){
        place(bp,newsize);
        return bp;
    }

    // extend_size = MAX(newsize,CHUNKSIZE);
    // if ((bp = extend_heap(extend_size/WSIZE)) == NULL)
    if ((bp = extend_heap(newsize/WSIZE)) == NULL) // 할당 요청 받은 사이즈보다 CHUNK(1MB)가 더큼
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
    coalesce(ptr);
}

static void *coalesce(void *bp)
{

    while (!GET_ALLOC(HDRP(NEXT_BLKP(bp))) && (GET_SIZE(HDRP(NEXT_BLKP(bp)))==GET_SIZE(HDRP(bp)))) {
        stitch(NEXT_BLKP(bp));
        PUT(HDRP(bp),GET_SIZE(HDRP(bp))<<1);
    }
    insert_to_list(bp);
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
    return MIN(result,FREE_LIST_COUNT-1);
}

static void insert_to_list(void *bp) {
    
    // Address Ordered
    // int index = size_class_index(GET_SIZE(HDRP(bp)));
    
    // if (GET(heap_listp+(index*WSIZE))==(int)heap_listp) {
    //     PUT(heap_listp+(index*WSIZE),bp);
    //     PUT(bp,heap_listp);
    //     PUT(bp+WSIZE,heap_listp);
    // } else {
    //     void * succ_listp = (void *)GET(heap_listp+(index*WSIZE));
        
    //     while ((succ_listp < bp) && ((void *)GET(succ_listp+WSIZE)!=heap_listp)) {
    //         succ_listp = (void *)GET(succ_listp+WSIZE);
    //     }

    //     if (bp < succ_listp) {
    //         if ((void *)GET(succ_listp) == heap_listp) {
    //             PUT(heap_listp+(index*WSIZE),bp);
    //             PUT(bp,heap_listp);
    //         } else {
    //             PUT(GET(succ_listp)+WSIZE,bp);
    //             PUT(bp,GET(succ_listp));
    //         }
    //         PUT(bp+WSIZE,succ_listp);
    //         PUT(succ_listp,bp);
    //     } else {
    //         PUT(succ_listp+WSIZE,bp);
    //         PUT(bp,succ_listp);
    //         PUT(bp+WSIZE,heap_listp);
    //     }
    // }

    // LIFO
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

    // 지연 병합 구현 ing...
    //     void *bp;

    // int index = size_class_index(size);

    // for (bp = (void *)GET(heap_listp+(index*WSIZE)); bp != heap_listp; bp = (void *)GET(bp+WSIZE)) {
    //     if (GET_SIZE(HDRP(bp))>=size){
    //         return bp;
    //     }
    // }

    // index += 1;

    // for (; index<FREE_LIST_COUNT; index++) {
    //     for (bp = (void *)GET(heap_listp+(index*WSIZE)); bp != heap_listp; bp = (void *)GET(bp+WSIZE)) {
    //         if (GET_SIZE(HDRP(bp))>=size){
    //             return bp;
    //         }
    //     }
    // }
    // return NULL;
}

static void place(void *bp, size_t size)
{
    // size_t least_size = 16;
    // size_t current_size = GET_SIZE(HDRP(bp));

    stitch(bp);
    while (GET_SIZE(HDRP(bp))>size) {
        PUT(HDRP(bp),PACK((GET_SIZE(HDRP(bp))>>1),0));
        PUT(HDRP(NEXT_BLKP(bp)),PACK(GET_SIZE(HDRP(bp)),0));
        insert_to_list(NEXT_BLKP(bp));
    }

    PUT(HDRP(bp),PACK(size,1));

}

