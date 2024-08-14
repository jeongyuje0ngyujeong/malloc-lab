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
 * provide your team information in the following struct..
 ********************************************************/
team_t team = {
    /* Team name */
    "team6",
    /* First member's full name */
    "Jin Jaewoong",
    /* First member's email address */
    "johnjin56@gmail.com",
    /* Second member's full name (leave blank if none) */
    "JeongUJeong",
    /* Second member's email address (leave blank if none) */
    "johnjin56@gmail.com"};
/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* 기본 매크로 */
#define WSIZE 4             // 워드와 헤더 푸터 사이즈를 4바이트로 설정
#define DSIZE 8             // 더블워드 사이즈를 8바이트로 설정
#define CHUNKSIZE (1 << 12) // heap을 늘릴 사이즈를 설정. *비트연산자* 약 4kb.

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word. Size와 할당된 비트를 워드로 pack함. */
#define PACK(size, alloc) ((size) | (alloc))
/* p의 주소에 있는 워드를 읽고 쓰기. */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* p의 주소에서 size와 할당된 field를 불러와 읽기 */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* bp : 블록 포인터, header 와 footer의 주소를 계산. */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* bp : 블록 포인터, 앞&뒤 블록의 주소를 계산한다. */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))
static char *heap_listp;  // 처음에 쓸 큰 가용블록 힙을 만들어줌.
static char *saved_listp; // next_fit에 쓸 변수

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* 가용 영역을 합쳐주는 함수. mm_free 블록을 반환하고 경계 태그 연결을 사용해서
상수 시간에 인접 가용 블록들과 통합한다. */
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); // 이전 블록이 할당인지
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); // 이후 블록이 할당인지
    size_t size = GET_SIZE(HDRP(bp));                   // 지금 블록의 사이즈

    if (prev_alloc && next_alloc)
    {              // case #1 이전과 다음 블록이 모두 할당되어있는 경우. 현재 블록의 상태는 가용으로 변경
        return bp; // 이미 free에서 가용이 되어있으니 따로 free해줄 필요는 없다
    }
    else if (prev_alloc && !next_alloc)
    {                                          // case #2 이전 블록은 할당상태, 다음 블록은 가용 상태일 때, 현재 블록은 다음블록돠 통합됨.
        size += GET_SIZE(HDRP(NEXT_BLKP(bp))); // 다음 블록의 헤더를 보고, 그 블록의 크기만큼 지금 블록의 사이즈에 추가함.
        PUT(HDRP(bp), PACK(size, 0));          // 헤더 갱신(더 큰 크기로 PUT)
        PUT(FTRP(bp), PACK(size, 0));          // 푸터 갱신
    }
    else if (!prev_alloc && next_alloc)
    {                                            // case #3 이전 블록은 가용상태, 다음 블록은 할당 상태. 이전 블록은 현재 블록과 통합.
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));   // 다음 블록의 헤더를 보고, 그 블록의 크기만큼 지금 블록의 사이즈에 추가함.
        PUT(FTRP(bp), PACK(size, 0));            // 푸터에 새로운 크기값을 넣어줌
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); // 이전 블록의 헤더 위치로 이동, 새로운 크기값을 넣어줌.
        bp = PREV_BLKP(bp);                      // bp를 그 앞블록의 헤더로 이동.
    }
    else
    {                                                                          // case #4 이전 블록과 다음 블록 모두 가용상태. 이전 현재 다음 블록 통합.
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp))); // 이전 블록 헤더부터 다음 블록 푸터까지 사이즈를 구해서 더함
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));                               // 이전 블록 헤더에 사이즈 넣음
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));                               // 다음 블록 푸터에 사이즈 넣음
        bp = PREV_BLKP(bp);                                                    // 포인터를 이전블록 포인터로 바꿈(통합됐으니)
    }
    saved_listp = bp;
    return bp;
}

/* 힙을 늘려주는 함수. */
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;
    /* 할당을 유지하기 위해 짝수개의 워드를 할당함. -> 8바이트 단위로 할당한다는 의미인듯 */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
    {
        return NULL;
    }

    /* free block의 헤더/풋터/에필로그 헤더를 초기화. */
    PUT(HDRP(bp), PACK(size, 0));         // free block header 초기화. prologue block이랑 다름.
    PUT(FTRP(bp), PACK(size, 0));         // free block footer 초기화.
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // 블록을 추가했으니, epilogue header의 위치를 새롭게 조정해줌.

    /* 만약 앞뒤 블록이 free 라면, coalesce해라. */
    return coalesce(bp);
}

/*
 * mm_init - initialize the malloc packagee.
 */
int mm_init(void)
{
    /* 빈 힙공간을 초기화. */
    // old brk에서 4만큼 늘려서 mem brk로 늘림.
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
    {
        return -1;
    }
    PUT(heap_listp, 0);                            // padding을 한 워드 크기만큼생성.
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); // 프롤로그 헤더 생성. PACK(~)을 해석하면, 할당된(1) 8바이트의 공간(DSIZE)
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); // 프롤로그 풋터 생성. PACK(~)을 해석하면, 할당된(1) 8바이트의 공간(DSIZE)
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     // 에필로그 풋터 생성.
    heap_listp += (2 * WSIZE);                     // 포인터를 프롤로그 헤더와 풋터 사이로 옮긴다.
    // saved_listp = heap_listp;

    /* 빈 힙공간을 CHUNKSIZE 바이트 공간의 블록의 크기 만큼 늘린다 */
    extend_heap(1);
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
    {
        return -1;
    }
    return 0;
}

/* first fit 구현 */
static void *find_fit(size_t asize)
// {
//     char *bp;                // 비교할 포인터 생성
//     bp = heap_listp + DSIZE; // 첫 블록(init에서 초기화한)
//     while (GET_SIZE(HDRP(bp)) != 0)
//     {
//         if ((GET_SIZE(HDRP(bp)) >= asize) && GET_ALLOC(HDRP(bp)) == 0) // 해당블록이 asize보다 크거나 같다면, 가용하다면
//         {
//             return bp; // 그 포인터 반환
//         }
//         bp = NEXT_BLKP(bp); // 아니라면 포인터 다음으로 넘겨.
//     }
//     return NULL;
// }
{
    char *bp;         // 비교할 포인터 생성
    bp = saved_listp; // 첫 블록(init에서 초기화한)
    while (GET_SIZE(HDRP(bp)) != 0)
    {
        if ((GET_SIZE(HDRP(bp)) >= asize) && GET_ALLOC(HDRP(bp)) == 0) // 해당블록이 asize보다 크거나 같다면, 가용하다면
        {
            saved_listp = bp;
            return bp; // 그 포인터 반환
        }
        bp = NEXT_BLKP(bp); // 아니라면 포인터 다음으로 넘겨.
    }

    // 힙의 시작부터 이전 위치까지 검색
    bp = heap_listp;
    while (bp != saved_listp)
    {
        if ((GET_SIZE(HDRP(bp)) >= asize) && GET_ALLOC(HDRP(bp)) == 0) // 해당블록이 asize보다 크거나 같다면, 가용하다면
        {
            saved_listp = bp;
            return bp; // 그 포인터 반환
        }
        bp = NEXT_BLKP(bp); // 아니라면 포인터 다음으로 넘겨.
    } // next-fit

    return NULL;
}

/* place 함수 구현 */
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp)); // 현재 있는 블록의 사이즈.
    if ((csize - asize) >= (2 * DSIZE))
    {                                          // 현재 블록 사이즈안에서 asize를 넣어도 2*DSIZE(헤더와 푸터를 감안한 최소 사이즈)만큼 남냐? 남으면 다른 data를 넣을 수 있으니까.
        PUT(HDRP(bp), PACK(asize, 1));         // 헤더위치에 asize만큼 넣고 1(alloc)로 상태변환. 원래 헤더 사이즈에서 지금 넣으려고 하는 사이즈(asize)로 갱신.(자르는 효과)
        PUT(FTRP(bp), PACK(asize, 1));         // 푸터 위치도 변경.
        bp = NEXT_BLKP(bp);                    // regular block만큼 하나 이동해서 bp 위치 갱신.
        PUT(HDRP(bp), PACK(csize - asize, 0)); // 나머지 블록은(csize-asize) 다 가용하다(0)하다라는걸 다음 헤더에 표시.
        PUT(FTRP(bp), PACK(csize - asize, 0)); // 푸터에도 표시.
    }
    else
    {
        PUT(HDRP(bp), PACK(csize, 1)); // 위의 조건이 아니면 asize만 csize에 들어갈 수 있으니까 내가 다 먹는다.
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;      // 블록 사이즈 조정
    size_t extendsize; // fit하지 않는다면 이만큼 extended될것이다.
    char *bp;

    /* 비형식적인 요청 무시 */
    if (size == 0)
    {
        return NULL;
    }

    /* 블록 사이즈를 overhead와 alignment요건들을 포함하기위해 조정 */
    if (size <= DSIZE)
    {
        asize = 2 * DSIZE;
    }
    else
    {
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    }

    /* fit한 가용리스트를 찾기 위해 탐색 */
    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }

    /* fit한 공간을 찾지못했다. 메모리를 더 할당받고, 블록을 넣자. */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
    {

        return NULL;
    }
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr)); // 범위를 정해줌.

    PUT(HDRP(ptr), PACK(size, 0)); // free 하는 것이 아니라 비가용 상태로 만듬
    PUT(FTRP(ptr), PACK(size, 0)); // 마찬가지
    coalesce(ptr);
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