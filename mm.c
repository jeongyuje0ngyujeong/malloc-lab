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
    "Team Six",
    /* First member's full name */
    "Jin Jaewoong",
    /* First member's email address */
    "jaewoong@naver.com",
    /* Second member's full name (leave blank if none) */
    "Jeong Hwigun",
    /* Second member's email address (leave blank if none) */
    "hwigun@naver.com"
};

static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t newsize);
static void place(void *bp, size_t newsize);

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8
/* rounds up to the nearest multiple of ALIGNMENT */
//#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
//#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1<<12)

#define MAX(x, y) ((x) > (y) ? x : y)

#define PACK(size, alloc) ((size) | (alloc))

#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE((char *)(bp) - WSIZE))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE))


static char *heap_listp;
static char *last_place;

/* mm_init - initialize the malloc package. */
int mm_init(void) {

    //(void *)-1 이 sbrk가 error 났을 경우 반환 값
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1) return -1;
    
    //padding
    PUT(heap_listp, 0);
    
    //prologue header, footer
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
    
    //epilogue header
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));
    
    heap_listp += (2 * WSIZE);
    extend_heap(1);
    
    //힙오버플로우 일 경우 -1
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) return -1;   
    return 0;
}

/* 힙 확장 정적 함수 */
static void *extend_heap(size_t words) {
    char *bp;
    size_t size;

    //size가 홀수 일 경우 짝수로 맞춰서 워드 값 만들기
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;

    //힙오버플로우 일 경우 
    if ((long)(bp = mem_sbrk(size)) == -1) return NULL;

    //가용블록 header, footer 
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    //새로운 epilogue header
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));
    
    //가용블록 앞 뒤 확인해서 합친 후 return 
    return coalesce(bp);
}

/* 가용블록 연결 정적 함수 */
static void *coalesce(void *bp){
    //앞, 뒤 할당 여부 확인
    size_t prev_alloc = GET_ALLOC((char *)(bp) - DSIZE); //바꿔 볼 것
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));

    //bp의 블록 size
    size_t size = GET_SIZE(HDRP(bp));
    
    //앞 뒤 모두 할당 되어있을 경우
    if (prev_alloc && next_alloc) return bp;

    //뒤만 가용 블록일 경우
    else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        //bp기준 헤더가 변경 되었으므로 FTRP(bp)로 접근해야 함.
        PUT(FTRP(bp), PACK(size, 0));
    } 
    //앞만 가용 블록일 경우
    else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        //PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    //앞 뒤 모두 가용 블록일 경우
    else {
        size +=  GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        //bp기준 헤더가 된 것이 아니라 prev 헤드가 변경되었으므로 FTRP(bp)로 접근하는 것은 틀림
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    last_place = (char *)bp;
    return bp;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t newsize;
    size_t extendsize;
    char *bp;

    //if(heap_listp == 0) mm_init();

    //할당 할 사이즈가 0일 때
    if (size == 0) return NULL;

    //newsize 업데이트
    if (size <= DSIZE) newsize = 2 * DSIZE;
    else newsize = DSIZE * ((size + DSIZE + (DSIZE - 1)) / DSIZE);

    if ((bp = find_fit(newsize)) != NULL) {
        place(bp, newsize);
        return bp;
    }
    
    extendsize = MAX(newsize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL) return NULL;
    place(bp, newsize);
    return bp;
  
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr) {
    size_t size = GET_SIZE(HDRP(ptr));

    //header, footer 가용 블록으로 변경
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));

    //가용 블록들 연결
    coalesce(ptr);
}


//first-fit 위치 검색
static void *find_fit(size_t newsize) {
    void *bp;

    if (last_place == 0) last_place = heap_listp;    
    
    bp = last_place;
    while (GET_SIZE(HDRP(bp)) > 0) {
        if (!GET_ALLOC(HDRP(bp)) && (newsize <= GET_SIZE(HDRP(bp)))) {
            last_place = bp;
            return bp;
        }
        bp = NEXT_BLKP(bp);        
    }
    return NULL;
}
//     void *bp = heap_listp;

//     while (GET_SIZE(HDRP(bp)) > 0) {
//         if (!GET_ALLOC(HDRP(bp)) && (newsize <= GET_SIZE(HDRP(bp)))) return bp;
//         bp = NEXT_BLKP(bp);        
//     }
//     return NULL;
    
// }


    


//메모리 할당
static void place(void *bp, size_t newsize) {
    size_t size = GET_SIZE(HDRP(bp));

    //가용 블록에 메모리를 넣어도 double words 이상 남는다면
    if ((size - newsize) >= (2 * DSIZE)) {
        //먼저 메모리 넣어주고
        PUT(HDRP(bp), PACK(newsize, 1));
        PUT(FTRP(bp), PACK(newsize, 1));
        bp = NEXT_BLKP(bp);

        //남은 값들은 또 쓸 수 있으니까 가용 블록으로 만들어줌
        PUT(HDRP(bp), PACK(size - newsize, 0));
        PUT(FTRP(bp), PACK(size - newsize, 0));
    }
    else {
        PUT(HDRP(bp), PACK(size, 1));
        PUT(FTRP(bp), PACK(size, 1));
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
    copySize = *(size_t *)((char *)oldptr - WSIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}














