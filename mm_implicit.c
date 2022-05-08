// Perf index = 44 (util) + 9 (thru) = 53/100

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
    "Sixth",
    /* First member's full name */
    "Jack",
    /* First member's email address */
    "iacog@kakao.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
// #define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
// #define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

// #define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

// malloc 구현 위한 상수 및 매크로 선언부
#define WSIZE 4             /* 워드, 헤더/푸터 사이즈 */
#define DSIZE 8             /* 더블워드 사이즈 */
#define CHUNKSIZE (1 << 12) /* 힙영역 확장시 필요한 사이즈 */

#define MAX(x, y) ((x) > (y) ? (x) : (y))                           /* 최대값 반환 */
#define PACK(size, alloc) ((size) | (alloc))                        /* 할당하는 size와 할당여부 bit를 합쳐준 값을 반환 */
#define GET(p) (*(unsigned int *)(p))                               /* 헤더/푸터 p에 담긴 값 반환 */
#define PUT(p, val) (*(unsigned int *)(p) = (val))                  /* 헤더/푸터 p에 val값 저장 */
#define GET_SIZE(p) (GET(p) & ~0x7)                                 /* 헤더/푸터 p에서 사이즈만 추출 (7:111, ~7:000으로 할당여부 bit를 0으로 마스킹) */
#define GET_ALLOC(p) (GET(p) & 0x1)                                 /* 헤더/푸터 p에서 할당여부 bit만 추출 */
#define HDRP(bp) ((char *)(bp)-WSIZE)                               /* 블럭(페이로드)포인터 bp로부터 헤더p 계산 및 반환 */
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)        /* 블럭(페이로드)포인터 bp로부터 푸터p 계산 및 반환 */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE((char *)(bp)-WSIZE)) /* 블럭포인터 bp로부터 다음 블럭포인터 bp 계산 및 반환 */
#define PREV_BLKP(bp) ((char *)(bp)-GET_SIZE((char *)(bp)-DSIZE))   /* 블럭포인터 bp로부터 이전 블럭포인터 bp 계산 및 반환 */

/*
 * mm_init - initialize the malloc package.
 */
//  초기화. 프로그램의 malloc/free 요청으로부터 메모리를 할당/반환할 수 있도록 heap영역 초기화함.
static void *extend_heap(size_t words);
static void *heap_listp;
int mm_init(void)
{
    // 빈 가용리스트 4워드 할당 및 prologue block 초기화
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); /* prologue header */
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); /* prologue footer */
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     /* epilogue header */
    heap_listp += (2 * WSIZE);                     /* 프롤로그 푸터를 가리킴으로써 힙 영역에서 첫번째 bp의 역할을 함 */

    // 청크사이즈 바이트의 가용블럭으로 힙 확장
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *find_fit(size_t size);
void place(void *bp, size_t asize);
void *mm_malloc(size_t size)
{
    size_t asize;      /* alignment가 적용된, 실제 할당이 필요한 사이즈 */
    size_t extendsize; /* 가용한 영역이 없을 경우 힙을 확장하기 위한 사이즈 */
    void *bp;

    // 잘못된 요청은 무시
    if (size == 0)
        return NULL;

    // 실제 할당이 필요한 사이즈 계산
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    // asize만큼 할당할 수 있는 가용영역이 있으면 할당/영역분할 하고 bp 반환
    if ((bp = find_fit(asize)) != NULL)
    {
        place(bp, asize);
        return bp;
    }

    // 할당가능 영역이 없으면 heap extend후 할당 및 반환
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
static void *coalesce(void *bp);
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0)); /* 헤더의 할당여부bit를 0으로 변경 */
    PUT(FTRP(bp), PACK(size, 0)); /* 푸터의 할당여부bit를 0으로 변경 */
    coalesce(bp);                 /* 이전/이후 블럭이 가용블럭이면 연결 */
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *bp, size_t size)
{
    void *oldbp = bp;
    void *newbp;
    size_t copySize;

    newbp = mm_malloc(size);
    if (newbp == NULL)
        return NULL;
    copySize = GET_SIZE(HDRP(oldbp)) - DSIZE;
    if (size < copySize)
        copySize = size;
    memcpy(newbp, oldbp, copySize);
    mm_free(oldbp);
    return newbp;
}

// 초기화 또는 가용메모리 부족시 힙영역 확장
static void *extend_heap(size_t words)
{
    void *bp;
    size_t size;

    // 입력된 인자로 실제 할당에 필요한 사이즈 계산 및 할당
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1) /* 할당이 안될경우 -1로 반환되기 때문에 long형으로 바꿔서 확인한듯 */
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));         /* 확장된 힙영역을 가용리스트로 초기화(헤더) */
    PUT(FTRP(bp), PACK(size, 0));         /* 확장된 힙영역을 가용리스트로 초기화(푸터) */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* 확장된 힙영역의 마지막 워드를 에필로그 헤더로 초기화 */

    return coalesce(bp); /* 이전 블럭이 가용하면 연결해서 반환 */
}

// 이전 이후 가용한 블럭에 대한 연결
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    // case1 : 이전 이후 모두 할당된 경우
    if (prev_alloc && next_alloc)
    {
        return bp;
    }

    // case2 : 이후 블럭이 가용한 경우
    else if (prev_alloc && !next_alloc)
    {
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    // case3 : 이전 블럭이 가용한 경우
    else if (!prev_alloc && next_alloc)
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    // case4 : 이전 이후 블럭 모두 가용한 경우
    else
    {
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}

// 요구 사이즈에 대해서 가용 영역 찾기
void *find_fit(size_t asize)
{
    void *bp;
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) /* 에필로그 만나기 전까지 반복 */
    {
        if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= asize) /* 찾았으면 리턴 */
            return bp;
    }
    return NULL; /* 못찾았으면 널 리턴 */
}

// 가용 영역의 포인터 bp에 대해서 헤더/푸터 초기화, 요구 사이즈보다 크면 분할
void place(void *bp, size_t asize)
{
    size_t original_size = GET_SIZE(HDRP(bp));

    if (original_size >= asize + 2 * DSIZE)
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(original_size - asize, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(original_size - asize, 0));
    }
    else
    {
        PUT(HDRP(bp), PACK(original_size, 1));
        PUT(FTRP(bp), PACK(original_size, 1));
    }
}
