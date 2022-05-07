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
int mm_init(void)
{
    
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














