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
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
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

// malloc 구현 위한 상수 및 매크로 선언부
#define WSIZE   4   /* 워드, 헤더/푸터 사이즈 */
#define DSIZE   8   /* 더블워드 사이즈 */
#define CHUNKSIZE   (1<<12) /* 힙영역 확장시 필요한 사이즈 */

#define MAX(x, y)   ((x) > (y)? (x) : (y)) /* 최대값 반환 */
#define PACK(size, alloc)   ((size) | (alloc)) /* 할당하는 size와 할당여부 bit를 합쳐준 값을 반환 */
#define GET(p)  (*(unsigned int *)(p))  /* 헤더/푸터 p에 담긴 값 반환 */
#define PUT(p, val) (*(unsigned int *)(p) = (val))  /* 헤더/푸터 p에 val값 저장 */
#define GET_SIZE(p) (GET(p) & ~0x7) /* 헤더/푸터 p에서 사이즈만 추출 (7:111, ~7:000으로 할당여부 bit를 0으로 마스킹) */
#define GET_ALLOC(p)    (GET(p) & 0x1)  /* 헤더/푸터 p에서 할당여부 bit만 추출 */
#define HDRP(bp)    ((char *)(bp) - WSIZE) /* 블럭(페이로드)포인터 bp로부터 헤더p 계산 및 반환 */
#define FTRP(bp)    ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) /* 블럭(페이로드)포인터 bp로부터 푸터p 계산 및 반환 */
#define NEXT_BLKP(bp)   ((char *)(bp) + GET_SIZE((char *)(bp) - WSIZE)) /* 블럭포인터 bp로부터 다음 블럭포인터 bp 계산 및 반환 */
#define PREV_BLKP(bp)   ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE)) /* 블럭포인터 bp로부터 이전 블럭포인터 bp 계산 및 반환 */


/* 
 * mm_init - initialize the malloc package.
 */
// 초기화 또는 가용메모리 부족시 힙영역 확장
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    // 입력된 인자로 실제 할당에 필요한 사이즈 계산 및 할당
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1) /* 할당이 안될경우 -1로 반환되기 때문에 long형으로 바꿔서 확인한듯 */
        return NULL;

    PUT(HDRP(bp), PACK(size, 0)); /* 확장된 힙영역을 가용리스트로 초기화(헤더) */
    PUT(FTRP(bp), PACK(size, 0)); /* 확장된 힙영역을 가용리스트로 초기화(푸터) */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1); /* 확장된 힙영역의 마지막 워드를 에필로그 헤더로 초기화 */
    
    return coalesce(bp); /* 이전 블럭이 가용하면 연결해서 반환 */
}

//  초기화. 프로그램의 malloc/free 요청으로부터 메모리를 할당/반환할 수 있도록 heap영역 초기화함.
static char *heap_listp;
int mm_init(void)
{
    // 빈 가용리스트 4워드 할당 및 prologue block 초기화
    if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); /* prologue header */
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); /* prologue footer */
    PUT(heap_listp + (3*WSIZE), PACK(0, 1)); /* epilogue header */
    heap_listp += (2*WSIZE); /* 왜 프롤로그 푸터를 가리키지..? */

    // 청크사이즈 바이트의 가용블럭으로 힙 확장
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    int newsize = ALIGN(size + SIZE_T_SIZE);
    void *p = mem_sbrk(newsize);
    if (p == (void *)-1)
	return NULL;
    else {
        *(size_t *)p = size;
        return (void *)((char *)p + SIZE_T_SIZE);
    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
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
    copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}














