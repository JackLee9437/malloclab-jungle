// Perf index = 43 (util) + 40 (thru) = 83/100
// Why not differ from Explicit...? I thought this has better util performance compared to explicit list...
/*
explicit -> segregated 좋은점 :
    잘게 쪼개진 가용영역을 따로 관리하여, 작은 메모리 요청시 빠르게 대응(쓰루풋 좋음)할 수 있고,
    first fit 가정하에 큰 덩어리 explicit에서는 첫번째 발견되는 큰덩어리를 계속 쪼개는 바람에 외부단편화가 커질 수 있었다면
    여기서는 쪼개진 영역을 바로바로 할당해주어 best fit에 가깝고, 추가로 쪼개지는걸 막아 외부단편화를 개선해줄 수 있음
    but, 쪼개진 영역들은 결국 곳곳에 떨어져있을 가능성이 높아 전체적으로 쪼개진 영역의 합이 넉넉하더라도 분리되어있어 할당불가 - 추가 할당이 필요한 상황이 올 가능성이 높음
    즉, 결과적으로 외부단편화에 취약한건 여전함,,,
*/
// Seglist 구현시 firstfit으로 찾아도 bestfit으로 찾을 수 있도록, free영역을 root에 넣지 않고 알맞은 자리에 들어가도록 수정함.
// 그리고 영역을 2의 k제곱으로 세분화하여 구현함
// Perf index = 45 (util) + 27 (thru) = 72/100
// util점수는 높아졌지만 쓰루풋점수는 하락함...........

/*
Results for mm malloc:
trace  valid  util     ops      secs  Kops
 0       yes   99%    5694  0.000226 25150
 1       yes   99%    5848  0.000211 27689
 2       yes   99%    6648  0.000262 25345
 3       yes  100%    5380  0.000213 25246
 4       yes   66%   14400  0.000270 53412
 5       yes   96%    4800  0.001186  4046
 6       yes   95%    4800  0.001121  4282
 7       yes   55%   12000  0.029388   408
 8       yes   51%   24000  0.111255   216
 9       yes   31%   14401  0.121288   119
10       yes   30%   14401  0.003955  3641
Total          75%  112372  0.269376   417

Perf index = 45 (util) + 28 (thru) = 73/100
*/

/*
쓰루풋 개선 - '적절한 size의 seglist를 구하는 함수'에서 bit연산으로 구현시 확실히 개선됨

Results for mm malloc:
trace  valid  util     ops      secs  Kops
 0       yes   99%    5694  0.000222 25603
 1       yes   99%    5848  0.000210 27808
 2       yes   99%    6648  0.000264 25172
 3       yes  100%    5380  0.000218 24713
 4       yes   66%   14400  0.000272 52922
 5       yes   96%    4800  0.001168  4110
 6       yes   95%    4800  0.001107  4336
 7       yes   55%   12000  0.000463 25912
 8       yes   51%   24000  0.001237 19399
 9       yes   31%   14401  0.123736   116
10       yes   30%   14401  0.003911  3682
Total          75%  112372  0.132809   846

Perf index = 45 (util) + 40 (thru) = 85/100
 */

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
#define HDRP(bp) ((void *)(bp)-WSIZE)                               /* 블럭(페이로드)포인터 bp로부터 헤더p 계산 및 반환 */
#define FTRP(bp) ((void *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)        /* 블럭(페이로드)포인터 bp로부터 푸터p 계산 및 반환 */
#define NEXT_BLKP(bp) ((void *)(bp) + GET_SIZE((void *)(bp)-WSIZE)) /* 블럭포인터 bp로부터 다음 블럭포인터 bp 계산 및 반환 */
#define PREV_BLKP(bp) ((void *)(bp)-GET_SIZE((void *)(bp)-DSIZE))   /* 블럭포인터 bp로부터 이전 블럭포인터 bp 계산 및 반환 */

// segregated 구현을 위한 매크로 (전제 : free된 영역의 bp에만 사용 가능함.)
#define PUT_ADDRESS(p, val) (*((void **)(p)) = (void *)(val)) /* NXTP/PRVP p 에 주소값 저장 */
#define PRVP(bp) ((void *)(bp))                               /* next 가용영역을 가리키는 주소값을 저장하는 word의 주소 반환 */
#define NXTP(bp) ((void *)(bp) + WSIZE)                       /* prev 가용영역을 가리키는 주소값을 저장하는 word의 주소 반환 */
#define PREV_FBLKP(bp) (*((void **)(PRVP(bp))))               /* prev 가용영역의 주소값 반환 */
#define NEXT_FBLKP(bp) (*((void **)(NXTP(bp))))               /* next 가용영역의 주소값 반환 */

/*
 * mm_init - initialize the malloc package.
 */
//  초기화. 프로그램의 malloc/free 요청으로부터 메모리를 할당/반환할 수 있도록 heap영역 초기화함.
static void *extend_heap(size_t words);
static void *heap_listp; /* 루트 */
int mm_init(void)
{
    // 빈 가용리스트 4워드 할당 및 prologue block 초기화
    if ((heap_listp = mem_sbrk(14 * WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);
    PUT(heap_listp + (1 * WSIZE), PACK(6 * DSIZE, 1));  /* prologue header */
    PUT_ADDRESS(heap_listp + (2 * WSIZE), NULL);        /* prologue (~2**1)*DSIZE seglist ptr */
    PUT_ADDRESS(heap_listp + (3 * WSIZE), NULL);        /* prologue (~2**2)*DSIZE seglist ptr */
    PUT_ADDRESS(heap_listp + (4 * WSIZE), NULL);        /* prologue (~2**3)*DSIZE seglist ptr */
    PUT_ADDRESS(heap_listp + (5 * WSIZE), NULL);        /* prologue (~2**4)*DSIZE seglist ptr */
    PUT_ADDRESS(heap_listp + (6 * WSIZE), NULL);        /* prologue (~2**5)*DSIZE seglist ptr */
    PUT_ADDRESS(heap_listp + (7 * WSIZE), NULL);        /* prologue (~2**6)*DSIZE seglist ptr */
    PUT_ADDRESS(heap_listp + (8 * WSIZE), NULL);        /* prologue (~2**7)*DSIZE seglist ptr */
    PUT_ADDRESS(heap_listp + (9 * WSIZE), NULL);        /* prologue (~2**8)*DSIZE seglist ptr */
    PUT_ADDRESS(heap_listp + (10 * WSIZE), NULL);       /* prologue (~2**9)*DSIZE seglist ptr */
    PUT_ADDRESS(heap_listp + (11 * WSIZE), NULL);       /* prologue (2**9)*DSIZE~ seglist ptr */
    PUT(heap_listp + (12 * WSIZE), PACK(6 * DSIZE, 1)); /* prologue footer */
    PUT(heap_listp + (13 * WSIZE), PACK(0, 1));         /* epilogue header */
    heap_listp += (2 * WSIZE);                          /* 프롤로그 푸터를 가리킴으로써 힙 영역에서 첫번째 bp의 역할을 함 */

    // printf("=============== init ===============\n");
    // printf("초기 heap_listp[0] %p\n", heap_listp);
    // printf("초기 heap_listp[1] %p\n", heap_listp + WSIZE);
    // printf("초기 heap_listp[2] %p\n", heap_listp + 2 * WSIZE);
    // printf("초기 heap_listp[3] %p\n", heap_listp + 3 * WSIZE);
    // printf("초기 heap_listp[4] %p\n", heap_listp + 4 * WSIZE);
    // printf("초기 heap_listp[5] %p\n", heap_listp + 5 * WSIZE);
    // printf("초기 heap_listp[6] %p\n", heap_listp + 6 * WSIZE);
    // printf("초기 heap_listp[7] %p\n", heap_listp + 7 * WSIZE);
    // printf("초기 heap_listp[8] %p\n", heap_listp + 8 * WSIZE);
    // printf("초기 heap_listp[9] %p\n", heap_listp + 9 * WSIZE);
    // printf("prologue footer ptr %p\n", heap_listp + 10 * WSIZE);

    // 청크사이즈 바이트의 가용블럭으로 힙 확장
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    // printf("초기 확장 후 가용list ptr %p\n", *((void **)heap_listp + 8));
    return 0;
}

// 디버깅용 임시 - 말록 호출할때마다 연결되어있는 개수 확인
// static void print_cnt_of_seglist(char *str)
// {
//     int i;
//     // printf("--------------- %s 후 seglist 확인 ---------------\n", str);
//     for (i = 0; i < 10; i++)
//     {
//         void **ptr = (void **)heap_listp + i;
//         int cnt = 0;
//         for (void *bp = *ptr; bp != NULL; bp = NEXT_FBLKP(bp))
//         {
//             // printf("ptr : %p\n", ptr);
//             // printf("bp : %p\n", bp);
//             cnt += 1;
//         }
//         // printf("seglist[%d]의 연결된 free area 개수: %d\n", i, cnt);
//         // printf("seglist[%d] 첫번째 연결된 가용블럭 포인터 %p\n", i, *(void **)(heap_listp + i * WSIZE));
//     }
// }

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
static void *find_fit(size_t size);
static void place(void *bp, size_t asize);
void *mm_malloc(size_t size)
{
    size_t asize;      /* alignment가 적용된, 실제 할당이 필요한 사이즈 */
    size_t extendsize; /* 가용한 영역이 없을 경우 힙을 확장하기 위한 사이즈 */
    void *bp;

    // printf("============================================================\n");
    // printf("---------- malloc(%d) ----------\n", size);

    // 잘못된 요청은 무시
    if (size == 0)
        return NULL;

    // 실제 할당이 필요한 사이즈 계산
    if (size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    // print_cnt_of_seglist("findfit 전");
    // printf("find fit 하기 전\n");
    // asize만큼 할당할 수 있는 가용영역이 있으면 할당/영역분할 하고 bp 반환
    if ((bp = find_fit(asize)) != NULL)
    {
        // printf("find fit 성공함\n");
        place(bp, asize);
        // print_cnt_of_seglist("malloc에서 findfit 및 place 후");
        return bp;
    }

    // 할당가능 영역이 없으면 heap extend후 할당 및 반환
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    // printf("heap영역 %d 추가 할당받음\n", extendsize);
    place(bp, asize);
    // print_cnt_of_seglist("malloc에서 findfit 및 place 후");
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
static void *coalesce(void *bp);
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    // printf("---------- free(%p) ----------\n", bp);
    PUT(HDRP(bp), PACK(size, 0)); /* 헤더의 할당여부bit를 0으로 변경 */
    PUT(FTRP(bp), PACK(size, 0)); /* 푸터의 할당여부bit를 0으로 변경 */
    coalesce(bp);                 /* 이전/이후 블럭이 가용블럭이면 연결 */
    // print_cnt_of_seglist("free");
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *bp, size_t size)
{
    void *oldbp = bp;
    void *newbp;
    size_t copySize;

    // printf("---------- realloc(%p, %d) ----------\n", bp, size);

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

    // printf("---------- extend_heap(%d) ----------\n", words);

    // 입력된 인자로 실제 할당에 필요한 사이즈 계산 및 할당
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1) /* 할당이 안될경우 -1로 반환되기 때문에 long형으로 바꿔서 확인한듯 */
        return NULL;

    PUT(HDRP(bp), PACK(size, 0));         /* 확장된 힙영역을 가용리스트로 초기화(헤더) */
    PUT(FTRP(bp), PACK(size, 0));         /* 확장된 힙영역을 가용리스트로 초기화(푸터) */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* 확장된 힙영역의 마지막 워드를 에필로그 헤더로 초기화 */
    PUT_ADDRESS(NXTP(bp), NULL);          /* 일단 NULL로 초기화 */
    PUT_ADDRESS(PRVP(bp), NULL);          /* 일단 NULL로 초기화 */

    // print_cnt_of_seglist("extend_heap");

    return coalesce(bp); /* 이전 블럭이 가용하면 연결해서 반환 */
}

// 이전 이후 가용한 블럭에 대한 연결
static void add_to_freelist(void *bp);
static void change(void *bp);
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    // printf("---------- coalesce(%p) ----------\n", bp);

    // print_cnt_of_seglist("coalesce 전");
    // case1 : 이전 이후 모두 할당된 경우
    if (prev_alloc && next_alloc)
    {
    }

    // case2 : 이후 블럭이 가용한 경우
    else if (prev_alloc && !next_alloc)
    {
        change(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    // case3 : 이전 블럭이 가용한 경우
    else if (!prev_alloc && next_alloc)
    {
        change(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    // case4 : 이전 이후 블럭 모두 가용한 경우
    else
    {
        change(PREV_BLKP(bp));
        change(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    add_to_freelist(bp);
    // print_cnt_of_seglist("coalesce 후");
    return bp;
}

// 요구 사이즈에 대해서 가용 영역 찾기
static void **get_seglist_ptr(size_t asize);
static void *find_fit(size_t asize)
{
    void **ptr;
    void *bp;
    // printf("---------- findfit(%d) ----------\n", asize);
    for (ptr = get_seglist_ptr(asize); ptr != (void **)heap_listp + 10; ptr++)
    {
        // printf("find fit 의 ptr 돌아가면서 for문\n");
        // printf("ptr:%p\n", ptr);
        for (bp = *ptr; bp != NULL; bp = NEXT_FBLKP(bp)) /* 에필로그 만나기 전까지 반복 */
        {
            // printf("find fit 의 list돌면서 가능여부 확인하는 for문\n");
            // printf("bp:%p\n", bp);
            // printf("next bp:%p\n", NEXT_FBLKP(bp));
            if (GET_SIZE(HDRP(bp)) >= asize) /* 찾았으면 리턴 */
                return bp;
        }
    }
    // printf("find_fit 실패함\n");
    // printf("마지막 bp ptr %p\n", bp);
    // print_cnt_of_seglist("findfit");
    return NULL; /* 못찾았으면 널 리턴 */
}

// 가용 영역의 포인터 bp에 대해서 헤더/푸터 초기화, 요구 사이즈보다 크면 분할
static void place(void *bp, size_t asize)
{
    size_t original_size = GET_SIZE(HDRP(bp));

    // printf("---------- place(%p, %d) ----------\n", bp, asize);
    change(bp);
    // printf("%p위치에 %d사이즈 place성공\n", bp, asize);
    if (original_size >= asize + 2 * DSIZE)
    {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));

        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(original_size - asize, 0));
        PUT(FTRP(bp), PACK(original_size - asize, 0));

        add_to_freelist(bp);
    }
    else
    {
        PUT(HDRP(bp), PACK(original_size, 1));
        PUT(FTRP(bp), PACK(original_size, 1));
    }
    // print_cnt_of_seglist("place");
}

// segregated - 메모리 반환시 root와 free된 영역 연결
static void add_to_freelist(void *bp)
{
    void **ptr = get_seglist_ptr(GET_SIZE(HDRP(bp)));
    void *cur;
    void *final = NULL;

    // printf("---------- add_to_freelist(%p) ----------\n", bp);

    size_t bp_size = GET_SIZE(HDRP(bp));
    for (cur = *ptr; cur != NULL; cur = NEXT_FBLKP(cur)) /* 에필로그 만나기 전까지 반복 */
    {
        final = cur;
        if (GET_SIZE(HDRP(cur)) >= bp_size) /* 찾았으면 리턴 */
        {
            PUT_ADDRESS(NXTP(bp), cur);
            PUT_ADDRESS(PRVP(bp), PREV_FBLKP(cur));
            PUT_ADDRESS(PREV_FBLKP(cur) == (void *)ptr ? (void *)ptr : NXTP(PREV_FBLKP(cur)), bp);
            PUT_ADDRESS(PRVP(cur), bp);
            // printf("PREV_FBLKP(cur) == ptr? %d\n", PREV_FBLKP(cur) == (void *)ptr);
            // printf("ptr : %p // PREV_FBLKP(cur) : %p \n", ptr, PREV_FBLKP(cur));
            // printf("새로생긴 %d크기의 free영역 %p 을 seglist ptr %p 앞에 연결완료\n", GET_SIZE(HDRP(bp)), bp, cur);
            return;
        }
    }
    PUT_ADDRESS(NXTP(bp), NULL);
    PUT_ADDRESS(PRVP(bp), final == NULL ? ptr : final);
    PUT_ADDRESS(final == NULL ? (char *)ptr : NXTP(final), bp);
    // printf("새로생긴 %d크기의 free영역 %p 을 맨 처음 또는 맨 뒤에 연결완료\n", GET_SIZE(HDRP(bp)), bp);
    // print_cnt_of_seglist("add");
    return;
}

// static void add_to_freelist(void *bp)
// {
//     void **ptr = get_seglist_ptr(GET_SIZE(HDRP(bp)));
//     PUT_ADDRESS(NXTP(bp), *ptr);
//     PUT_ADDRESS(PRVP(bp), ptr);
//     if (*ptr != NULL)
//         PUT_ADDRESS(PRVP(*ptr), bp);
//     PUT_ADDRESS(ptr, bp);
// }

// segregated - 반환되는 bp 기준 전/후 free 영역 연결
static void change(void *bp)
{
    void *prvp = PREV_FBLKP(bp);
    void *nxtp = NEXT_FBLKP(bp);

    // printf("---------- change(%p) ----------\n", bp);

    // printf("%p를 빼기 위해 %p와 %p 연결\n", bp, prvp, nxtp);
    if ((void **)prvp < (void **)heap_listp + 10) /* 이전꺼가 맨 처음이면 */
        PUT_ADDRESS(prvp, nxtp);
    else
        PUT_ADDRESS(NXTP(prvp), nxtp);
    if (nxtp != NULL)
        PUT_ADDRESS(PRVP(nxtp), prvp);
    // print_cnt_of_seglist("change");
}

// 필요한 사이즈에 따라 필요한 seglist의 ptr를 반환
static void **get_seglist_ptr(size_t asize)
{
    // printf("---------- get_seglist_ptr(%d) ----------\n", asize);
    if (asize <= (1 << 4)) /* DSIZE * (pow(2, 1) + 1)) */
        return (void **)heap_listp;
    else if (asize <= (1 << 5)) /* DSIZE * (pow(2, 2) + 1)) */
        return (void **)heap_listp + 1;
    else if (asize <= (1 << 6)) /* DSIZE * (pow(2, 3) + 1)) */
        return (void **)heap_listp + 2;
    else if (asize <= (1 << 7)) /* DSIZE * (pow(2, 4) + 1)) */
        return (void **)heap_listp + 3;
    else if (asize <= (1 << 8)) /* DSIZE * (pow(2, 5) + 1)) */
        return (void **)heap_listp + 4;
    else if (asize <= (1 << 9)) /* DSIZE * (pow(2, 6) + 1)) */
        return (void **)heap_listp + 5;
    else if (asize <= (1 << 10)) /* DSIZE * (pow(2, 7) + 1)) */
        return (void **)heap_listp + 6;
    else if (asize <= (1 << 11)) /* DSIZE * (pow(2, 8) + 1)) */
        return (void **)heap_listp + 7;
    else if (asize <= (1 << 12))
        return (void **)heap_listp + 8;
    else
        return (void **)heap_listp + 9;
}