/*
 * mm-final.c - 명시적 가용 리스트 + 주소순서 삽입 + Best Fit + realloc 최적화
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include "mm.h"
#include "memlib.h"

/* 팀 정보 */
team_t team = {
    "8조", "홍길동", "jungle_campus", "", ""
};

/* 매크로 */
#define WSIZE 8
#define DSIZE 16
// #define WSIZE 4     // implicit
// #define DSIZE 8     // implicit
#define CHUNKSIZE (1<<12)

#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define PACK(size, alloc) ((size) | (alloc))
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE))

#define GET_SUCC(bp) (*(void **)((char *)(bp) + WSIZE))
#define GET_PRED(bp) (*(void **)(bp))

/* 전역변수 */
static char *heap_listp;

/* 함수 선언 */
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void *find_fit(size_t asize);
static void place(void *bp, size_t asize);
static void add_free_block(void *bp);
static void splice_free_block(void *bp);

/* mm_init */
int mm_init(void)
{
    if ((heap_listp = mem_sbrk(8 * WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);     // 맨 앞 패딩. 정렬을 위한 패딩.
    PUT(heap_listp + (1 * WSIZE), PACK(2 * WSIZE, 1));  // 프롤로그 헤더.
    PUT(heap_listp + (2 * WSIZE), PACK(2 * WSIZE, 1));  // 프롤로그 풋터.
    PUT(heap_listp + (3 * WSIZE), PACK(4 * WSIZE, 0));  // dummy free node 헤더.
    PUT(heap_listp + (4 * WSIZE), NULL);                // dummy free node, prev.
    PUT(heap_listp + (5 * WSIZE), NULL);                // dummy free node, next
    PUT(heap_listp + (6 * WSIZE), PACK(4 * WSIZE, 0));  // dummy free node 풋터.
    PUT(heap_listp + (7 * WSIZE), PACK(0, 1));          // 에필로그 헤더.

    // 그래서 처음 payload가 오는 곳은 heap_list에서 dummy free 헤더 다음인
    // 4 * WSIZE 뒤로 설정.
    heap_listp += (4 * WSIZE);

    // 이제 프롤로그, 에필로그, 더미 노드까지 다 세팅해놨으니,
    // 진짜 값이 들어갈 공간들 세팅해줘야 함.
    // 초기 공간 세팅은 CHUNKSIZE만큼 해주기
    // CHUNKSIZE를 WSIZE로 나눠주는 이유는, 정렬기준을 맞춰주기 위해서.
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;
    return 0;
}

/* extend_heap */
static void *extend_heap(size_t words)
{
    // 여기서 받은 words는 추가로 만들 공간을 WSIZE로 나눠준 값
    char *bp;
    // 정렬기준을 맞추려면, 16바이트로 정렬을 맞춰줘야함.
    // words는 8로 나눈 값이기 때문에, 짝수라면? 그대로 다시 WSIZE를 곱해서 사용해도 됨.
    // 홀수라면? 짝수로 맞춰주고, 다시 WSIZE 곱해줘 사용하기.
    size_t size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;

    // 이제 진짜 사이즈 늘려주고, 원래 있던 brk 값 bp에 저장.
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    // 할당할 크기만큼 size에 저장해놨으니,
    // 새롭게 만든 공간의 헤더, 풋터에 사이즈와 할당 정보 넣어주기.
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));

    // 그리고 에필로그 다시 설정.
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    // 새로 만들어진 메모리 공간도 free list에 넣어줘야함.
    // 그 과정은 coalesce 과정에 포함되어 있음.
    return coalesce(bp);
}

/* add_free_block - 주소순서 삽입 */
static void add_free_block(void *bp)
{
    // cur 은 free list의 맨 앞.
    void *cur = heap_listp;
    // // version1(LIFO) : 맨앞에 추가하기 때문에, 추가 시간복잡도 O(1)
    // heap_listp = bp;

    // GET_PRED(heap_listp) = NULL;
    // GET_SUCC(heap_listp) = cur;

    // if (cur != NULL)
    //     GET_PRED(cur) = heap_listp;

    // version2(주소 지정 방식) : 맞는 위치 찾아가야 해서, 시간복잡도 O(N)
    void *prev = NULL;
    // free 블럭의 주소 순서대로 free list의 순서를 설정한 것 같음.
    while (cur != NULL && cur < bp) {
        prev = cur;
        cur = GET_SUCC(cur);
    }

    if (prev != NULL)
        GET_SUCC(prev) = bp;
    else
        heap_listp = bp;

    if (cur != NULL)
        GET_PRED(cur) = bp;

    GET_PRED(bp) = prev;
    GET_SUCC(bp) = cur;

}
// 주소 지정 방식 VS LIFO <- 둘의 장단점 및 비교

/* splice_free_block */
static void splice_free_block(void *bp)
{
    // 받아온 주소의 블록을 free list에서 제거해야 하는상황
    // 설명을 간단히 하기 위해 선임자를 1, 자신을 2, 후임자를 3으로 하겠음.
    // 1 - 2 - 3 이렇게 연결되어 있는 형태에서 2를 제거하고 싶은거임.

    // 만약 2의 선임자가 있다?
    if (GET_PRED(bp))
        // 있으면 선임자의 후임자를 받아온 블록의 후임자로 이어주기.
        // 1 -> 3 이렇게 된거임.
        GET_SUCC(GET_PRED(bp)) = GET_SUCC(bp);
    
    // 만약 2의 선임자가 없다? -> 이말은 2가 리스트의 맨 앞이라는 거임.
    else
        // 그러면 그냥 2의 후임자가 free list의 맨 앞이 되면 끝.
        heap_listp = GET_SUCC(bp);

    // 만약 2의 후임자가 있다?
    if (GET_SUCC(bp))
        // 그러면 2의 후임자의 전임자를, 2의 전임자로 이어주기
        // 1 <- 3 이렇게 만들어준거임.
        GET_PRED(GET_SUCC(bp)) = GET_PRED(bp);

    // 2의 후임자가 없는 경우엔? 아무것도 해줄게 없으므로 패스.
    // ??? : 그럼 2의 전임자가 NULL을 가리키게 설정해줘야 하는거 아닌가요?
    // -> ㅇㅇ 이미 했음. 위에서 2의 선임자가 있는 경우를 잘 읽어봐.
}

/* coalesce */
static void *coalesce(void *bp)
{
    // 받아온 노드의 주소 이전, 다음 블록의 주소 저장.
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    // 현재 블록의 크기도 저장.
    size_t size = GET_SIZE(HDRP(bp));

    // 만약 이전, 다음 블록 모두 할당이 되어있다?
    if (prev_alloc && next_alloc) {
        // 그러면 그냥 지금 상태 그대로 free list에 넣어주면 됨.
        add_free_block(bp);
        // 추가했으면 끝.
        return bp;
    }
    // 근데 이전은 할당이 되어있고, 다음은 free 상태다?
    else if (prev_alloc && !next_alloc) {
        // 그러면 일단 다음 블록이랑 합쳐줘야 하니까,
        // 다음 블록을 free list에서 빼줘야 함.
        splice_free_block(NEXT_BLKP(bp));

        // 다음 free 블록의 크기까지 더해줘서
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        // 현재 헤더에 변경된 size와 할당 여부를 바꿔주고,
        PUT(HDRP(bp), PACK(size, 0));
        // size가 바뀌엇기 때문에, 풋터의 위치도 알아서 바뀌어있음.
        PUT(FTRP(bp), PACK(size, 0));

        // ??? : 근데 새로 만들어진 free 블럭은 free list의 맨앞으로 설정하는 과정은 안해도 되나요?
        // -> ㅇㅇ. 좀만 기다려. 밑에서 하니까.
    }
    // 이전은 free 이고, 다음은 할당이 되어있다?
    else if (!prev_alloc && next_alloc) {
        // 마찬가지로 이전 블럭을 free list에서 제거해주고,
        splice_free_block(PREV_BLKP(bp));

        // 이전 블록의 크기만큼 사이즈를 재조정 해준 다음,
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        // 이번엔 풋터 먼저 설정.
        // 왜냐면 헤더의 위치가 이전 블럭의 헤더가 되어야 하기 때문.
        // PUT(FTRP(bp), PACK(size, 0));
        // PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        // 하고 payload의 위치를 이전 블록으로 변경.
        // bp = PREV_BLKP(bp);

        bp = PREV_BLKP(bp);
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        // ??? : 이렇게 하면 안되나요?
        // -> ㅇㅇ 됨. 사실 이게 더 안전함.
    }
    // 이제 둘다 free 인 경우
    else {
        // 이전도, 다음도 모두 free list에서 제거 해줘야 함.
        splice_free_block(PREV_BLKP(bp));
        splice_free_block(NEXT_BLKP(bp));
        // 사이즈도 다시 재조정 해주고,
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        // 주소를 이전 블록의 payload로 다시 맞춰주고
        bp = PREV_BLKP(bp);
        // 헤더, 푸터 다시 설정.
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    // 이제 새로 설정한 free 블럭을 free list의 맨 앞에 넣어줘야 함.
    // add free block 함수를 통해서 진행.
    add_free_block(bp);
    return bp;
}

/////////////////////////////////////////////////////////////////////
/* find_fit - Best Fit */
// static void *find_fit(size_t asize)
// {
//     // asize보다 큰 곳 찾기. 근데 가장 작은 놈으로.
//     void *bp = heap_listp;
//     void *best = NULL;
//     // 이건 -1로 설정하는게 아님. 최대 사이즈로 설정한거임.
//     size_t best_size = (size_t)(-1);

//     // free list 처음부터 쭉 찾기 시작합니다
//     while (bp != NULL) {
//         // free node 의 크기를 파악하고,
//         size_t bsize = GET_SIZE(HDRP(bp));
//         // 만약에 free node의 크기가 더 크다?
//         if (bsize >= asize) {
//             // 근데 free node 의 크기가 현재 best node의 크기보다 작다?
//             // 그럼 best node 갈아껴줘야지.
//             if (bsize < best_size) {
//                 best = bp;
//                 best_size = bsize;
//             }
//         }
//         // 하고 bp 다음으로 바꿔주고.
//         bp = GET_SUCC(bp);
//     }

//     // 다 돌면, best로 설정이 돼있겠지
//     return best;
// }

/////////////////////////////////////////////////////////////////////
// 이건 first-fit 할거.
static void *find_fit(size_t asize)
{
    // free list 시작부터
    void *bp = heap_listp;
    
    while (bp != NULL) {
        size_t bsize = GET_SIZE(HDRP(bp));
        if (bsize >= asize) {
            return bp;
        }
        bp = GET_SUCC(bp);
    }
    
    return NULL;
}



/* place */
static void place(void *bp, size_t asize)
{
    // bp 가 들어갈 free 블럭의 사이즈를 일단 받아오고
    size_t csize = GET_SIZE(HDRP(bp));
    // 해당 free 블록을 free list에서 없애주기
    splice_free_block(bp);

    // 만약에 free 블록을 사용할때, 쓰고 남은 크기가 32바이트보다 크면,
    if ((csize - asize) >= (2 * DSIZE)) {
        // 해당 블록에 사용할 크기만 할당해주고
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));

        // 남은 free 블록은 바로 다음 블록을 테니까
        bp = NEXT_BLKP(bp);
        // 남은 free 블록은 사이즈 재정의
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
        // 다 했으면 남은 free 블록은 free list에 추가해주기.
        add_free_block(bp);
    }
    // 만약 남은 크기가 32바이트보다 작으면 그냥 다 쓰지 뭐. 
    else
    {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

/* mm_malloc */
void *mm_malloc(size_t size)
{
    // size만큼 할당할 공간 만들어줘야 함.
    size_t asize;
    size_t extendsize;
    char *bp;
    // 만약 0을 늘려라? 장난치나; 바로 NULL
    if (size == 0)
        return NULL;

    // 늘려야 할 크기가 정렬 기준인 16바이트보다 작다?
    if (size <= DSIZE)
        // 그럼 바로 16바이트 기준을 맞춰줘야지 ㅇㅇ.
        // 헤더, 풋터까지 합쳐서 32바이트 할당.
        asize = 2 * DSIZE;
    // 크기가 16보다 크면?
    else
        // 이렇게 크기에 따라서 패딩해주면 됨.
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

    // 만약에 fit한곳을 찾지못하면 확장하고, fit한 곳이 있으면 거기 넣기.
    if ((bp = find_fit(asize)) != NULL) {
        // 있으니까 넣어줘야겠지
        place(bp, asize);
        return bp;
    }

    // 만약에 fit 한곳이 없다면 여기로 오는겨.
    // fit한 곳이 없으면, 메모리 확장해줘야겠지?
    // 적당히 확장해줄 사이즈를 정해주는데, CHUNCKSIZE 보다 큰 공간이 필요하면,
    // 그 공간에 맞게 확장해줘야 함.
    extendsize = MAX(asize, CHUNKSIZE);
    // 해서 확장을 할건데 확장에 실패하면 NULL 반환하고,
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    // 확장이 됐으면, 확장이 된 공간에 정보를 넣으면 되겠지?
    place(bp, asize);
    return bp;
}

/* mm_free */
void mm_free(void *bp)
{
    // free로 만들 블록의 크기를 일단 받고
    size_t size = GET_SIZE(HDRP(bp));

    // 그 크기만큼 다 메모리 할당 여부 0으로 해주면 됨.
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    // 이후에 인접 블록들과 병합
    coalesce(bp);
}

/* mm_realloc - in-place 최적화 */
// 이미 할당된 블록의 크기를 변경해주는게 realloc
// 변경된 블록의 payload 시작주소 반환하면 되겠지.
void *mm_realloc(void *ptr, size_t size)
{
    // ptr은 현재 블록의 주소
    // 만약에 블록이 NULL이면, size크기만큼 새로 할당해줘야겠지.
    if (ptr == NULL)
        return mm_malloc(size);
    // 만약에 변경하고 싶은 블록의 크기가 0이면, 메모리를 해제하겠다는 말과 같으니까
    // 바로 free
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    // 현재 블록의 크기 일단 저장해두고
    size_t oldsize = GET_SIZE(HDRP(ptr));
    // 정렬 기준을 맞추기 위해 패딩까지 합칠 블록의 크기 설정 -> asize
    size_t asize = (size <= DSIZE) ? (2 * DSIZE) : DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

    // 만약 asize가 기존 사이즈보다 작다? 즉, 블록을 축소하고 싶다?
    if (asize <= oldsize)
        // 어쩌라고 안줄일거야. 왜냐면 줄이게되면, 데이타 손실 생김.
        return ptr;

    // 여기까지 왔다면, 블록을 확장할거라는 얘기임.
    // 확장할거면 다음 블록을 일단 바라봐야함.
    void *next = NEXT_BLKP(ptr);
    // 다음 블록이 free이고, 크기가 충분하면,
    if (!GET_ALLOC(HDRP(next)) && (oldsize + GET_SIZE(HDRP(next))) >= asize) {
        // free list에서 블록 빼주고
        splice_free_block(next);
        // 다시 크기 재정의 해서 헤더, 풋터에 수정
        size_t newsize = oldsize + GET_SIZE(HDRP(next));
        PUT(HDRP(ptr), PACK(newsize, 1));
        PUT(FTRP(ptr), PACK(newsize, 1));
        return ptr;
    }

    // 근데 다음 블록이 free가 아니거나, 크기가 모자르면?
    // 그러면 그냥 힙에서 새로운 공간을 찾아서 할당해줘야됨.
    void *newptr = mm_malloc(size);
    // 만약에 더이상 할당할 수 없으면 바로 리턴
    if (newptr == NULL)
        return NULL;

    // 할당해줄 수 있으면, 이제 payload 부분만 복사해야하기 때문에,
    // payload 크기에 맞는 사이즈를 저장해주고
    size_t copySize = oldsize - DSIZE;
    // 사이즈가 작아지길 원하면, payload의 부분을 줄여서 ㄱㄱ
    if (size < copySize)
        copySize = size;
    // 복사. newptr에 ptr의 payload를 copysize만큼 복사하겠다.
    memcpy(newptr, ptr, copySize);
    // 이제 원래 메모리 주소는 해제해주면 됨.
    mm_free(ptr);
    
    return newptr;
}


///////////////////////////////////////////////////////////////////////////////
////////////////////////// * implicit * ///////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


// /* 단일 워드(4바이트) 또는 더블 워드(8바이트) 정렬 기준 */
// #define ALIGNMENT 8

// /* ALIGNMENT 배수로 올림 */
// #define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

// /* size_t 타입 크기를 ALIGNMENT 배수로 정렬한 크기 */
// #define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

// // static char *free_listp = NULL;
// static void *find_nextp; //전역 포인터: Next-Fit 탐색 시작 위치 저장

// /* 함수 프롤토타입 */
// static void *coalesce(void *bp);          /* 인접 가용 블록 병합 */
// static void *find_fit(size_t asize);      /* 가용 블록 탐색 */
// static void place(void *bp, size_t asize);/* 블록 배치 및 분할 처리 */

// /* 힙을 words * WSIZE 바이트 만큼 확장하고 새로운 가용 블록 생성 */
// static void *extend_heap(size_t words)
// {
//     char *bp;
//     size_t size;

//     /* 짝수 워드로 맞추어 8바이트 정렬 유지 */
//     size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
//     if ((long)(bp = mem_sbrk(size)) == -1)
//         return NULL;

//     /* 새로 생성된 블록의 헤더와 푸터 설정 (가용 상태) */
//     PUT(HDRP(bp), PACK(size, 0));              /* 가용 블록 헤더 */
//     PUT(FTRP(bp), PACK(size, 0));              /* 가용 블록 푸터 */
//     /* 새로운 에필로그 블록 헤더 설정 (크기=0, 할당됨) */
//     PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

//     return coalesce(bp); /* 인접 블록과 병합하여 커다란 블록 반환 */
// }


// // coalesce: 인접 블록이 free면 합쳐주기
// static void *coalesce(void *bp)
// {
//     // 이전 블록 free, 할당 여부
//     size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); 
//     // 다음 블록 free, 할당 여부
//     size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
//     // 현재 블록 크기
//     size_t size = GET_SIZE(HDRP(bp));             

//     // Case1: 이전과 다음 모두 할당된 경우
//     if (prev_alloc && next_alloc) {
//         return bp;
//     }
//     // Case2: 이전 할당, 다음 free인 경우
//     else if (prev_alloc && !next_alloc) {
//         // 다음 블록 크기 추가
//         size += GET_SIZE(HDRP(NEXT_BLKP(bp)));         

//         // 병합된 내용을 헤더와, 풋터에 추가
//         PUT(HDRP(bp), PACK(size, 0));
//         PUT(FTRP(bp), PACK(size, 0));
//     }
//     // Case3: 이전 free, 다음 할당
//     else if (!prev_alloc && next_alloc) {
//         // 이전 블록 크기 추가
//         size += GET_SIZE(HDRP(PREV_BLKP(bp)));         

//         // 이전 블록의 헤더와 현재 푸터에 병합된 내용 추가
//         PUT(FTRP(bp), PACK(size, 0));
//         PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        
//         // 병합 후 새 블록 포인터로 수정
//         bp = PREV_BLKP(bp);
//     }
//     // Case4: 이전과 다음 모두 free
//     else {
//         size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
//         /* 이전 헤더와 다음 푸터를 병합된 크기로 설정 */
//         PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
//         PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
//         bp = PREV_BLKP(bp);
//     }
    
//     //  병합 끝난 뒤에, 업데이트
//     find_nextp = bp;
        
//     return bp;
// }

// // Next - fit
// /* 적합한 블록 탐색 (Next-fit) */
// static void *find_fit(size_t asize) {
//     void *ptr;
//     ptr = find_nextp;   // 현재 탐색 시작 위치를 저장

//     // 첫 번째 루프: 현재 find_nextp부터 힙 끝까지 탐색
//     for (; GET_SIZE(HDRP(find_nextp)) >0; find_nextp = NEXT_BLKP(find_nextp))
//     {   
//         if (!GET_ALLOC(HDRP(find_nextp)) && (asize <= GET_SIZE(HDRP(find_nextp))))
//         {
//             return find_nextp;  // 해당 블록을 반환
//         }
//     }
//     // 두 번째 루프: 힙 처음(heap_listp)부터 아까 저장해둔 ptr까지 탐색
//     for (find_nextp = heap_listp; find_nextp != ptr; find_nextp = NEXT_BLKP(find_nextp))
//     {   
//         // 마찬가지로 할당되어 있지 않고 크기가 충분하면
//         if (!GET_ALLOC(HDRP(find_nextp)) && (asize <= GET_SIZE(HDRP(find_nextp))))
//         {
//             return find_nextp;  // 해당 블록을 반환
//         }
//     }
//     return NULL;    // 적합한 블록을 찾지 못한 경우
// }



// // // best-fit
// // static void *find_fit(size_t asize)
// // {
// //     void *bp;

// //     // for best-fit
// //     void *save_bp = NULL;

// //     for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
// //         if (!GET_ALLOC(HDRP(bp))) {
// //             size_t bsize = GET_SIZE(HDRP(bp));

// //             // for best-fit
// //             if (bsize >= asize)
// //             {
// //                 if (save_bp == NULL)
// //                     save_bp = bp;
// //                 else
// //                 {
// //                     if (GET_SIZE(HDRP(save_bp)) > GET_SIZE(HDRP(bp)))
// //                         save_bp = bp;
// //                 }
// //             }
            
// //             // // 아니면 지연 coalescing 시도
// //             // void *newbp = coalesce(bp);
// //             // // coalesce 후 bp가 바뀔수도 있으니까 bp 갱신
// //             // bp = newbp;
// //             // bsize = GET_SIZE(HDRP(bp));

// //             // // 병합된 크기가 맞다면 리턴
// //             // if (bsize >= asize)
// //             //     return bp;
// //             // // 이 블록도 안맞으면 다음 블록으로 계속
// //         }
// //     }

// //     // for best-fit
// //     if (save_bp != NULL)
// //         return save_bp;
// //     else
// //         return NULL;
// // }



// /* place: 가용 블록 bp에 asize 크기로 할당, 남으면 분할 처리 */
// static void place(void *bp, size_t asize)
// {
//     size_t csize = GET_SIZE(HDRP(bp)); /* 현재 블록 크기 */

//     /* 남은 공간이 최소 블록 크기(16바이트) 이상이면 분할 */
//     if ((csize - asize) >= (2 * DSIZE)) {
//         /* 할당 블록 헤더/푸터 설정 */
//         PUT(HDRP(bp), PACK(asize, 1));
//         PUT(FTRP(bp), PACK(asize, 1));
//         /* 남는 공간 가용 블록으로 헤더/푸터 설정 */
//         bp = NEXT_BLKP(bp);
//         PUT(HDRP(bp), PACK(csize - asize, 0));
//         PUT(FTRP(bp), PACK(csize - asize, 0));
//     } else {
//         /* 분할 없이 전체 블록 할당 */
//         PUT(HDRP(bp), PACK(csize, 1));
//         PUT(FTRP(bp), PACK(csize, 1));
//     }
// }


// /*
//  * mm_init - initialize the malloc package.
//  */
// /* 메모리 관리자 초기화 */
// int mm_init(void)
// {   
//     // 초기 힙 공간 할당 (4워드: 정렬 패딩 + 프롤로그 + 에필로그)
//     if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)     // 초기 힙 메모리를 할당
//         return -1;
//      // 힙 구조 초기화
//     PUT(heap_listp, 0);                             // 힙의 시작 부분에 0을 저장하여 패딩으로 사용
//     PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1));    // 프롤로그 블럭의 헤더에 할당된 상태로 표시하기 위해 사이즈와 할당 비트를 설정하여 값을 저장
//     PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1));    // 프롤로그 블록의 풋터에도 마찬가지로 사이즈와 할당 비트를 설정하여 값을 저장
//     PUT(heap_listp + (3*WSIZE), PACK(0, 1));        // 에필로그 블록의 헤더를 설정하여 힙의 끝을 나타내는 데 사용
//     heap_listp += (2*WSIZE);                        // 프롤로그 블록 다음의 첫 번째 바이트를 가리키도록 포인터 조정
//     find_nextp = heap_listp;                      // nextfit을 위한 변수(nextfit 할 때 사용)

//     // 초기 힙 확장
//     if (extend_heap(CHUNKSIZE/WSIZE) == NULL)       // 초기 힙을 확장하여 충분한 양의 메모리가 사용 가능하도록 chunksize를 단어 단위로 변환하여 힙 확장
//         return -1;
//     if (extend_heap(4) == NULL)                     //자주 사용되는 작은 블럭이 잘 처리되어 점수가 오름
//         return -1;
//     return 0;
// }


// /*
//  * mm_malloc - brk 포인터를 증가시켜 블록을 할당합니다.
//  *     항상 정렬 단위(8바이트)의 배수 크기로 블록을 할당합니다.
//  */
// void *mm_malloc(size_t size)
// {
//     size_t asize;        // 정렬 및 오버헤드 포함한 조정된 블록 크기
//     size_t extendsize;   // 적절한 블록이 없을 경우 힙을 확장할 크기
//     char  *bp;           // 할당된 블록 포인터

//     // 1. 요청 크기가 0일 경우 아무 것도 하지 않음
//     if (size == 0)
//         return NULL;

//     // 2. 블록 크기를 헤더/푸터 오버헤드 포함하여 정렬 단위로 조정
//     if (size <= DSIZE)               // 최소 블록 크기: 8바이트 payload + 8바이트 헤더/푸터 = 16B
//         asize = 2 * DSIZE;
//     else
//         asize = DSIZE * ((size + DSIZE + (DSIZE - 1)) / DSIZE); // 올림 정렬

//     // 3. 가용 블록 탐색 (first-fit)
//     if ((bp = find_fit(asize)) != NULL) {
//         place(bp, asize);            // 블록을 해당 위치에 배치하고 필요 시 분할
//         return bp;
//     }

//     // 4. 적절한 블록이 없으면 힙을 확장한 뒤 블록을 배치
//     extendsize = MAX(asize, CHUNKSIZE); // 최소 CHUNKSIZE(4096바이트) 만큼 확장
//     if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
//         return NULL;

//     place(bp, asize);
//     return bp;
// }

// /*
//  * mm_free - 블록을 해제하고 인접한 가용 블록과 병합(coalesce)합니다.
//  */
// void mm_free(void *bp)
// {
//     size_t size = GET_SIZE(HDRP(bp)); // 블록의 전체 크기 획득

//     PUT(HDRP(bp), PACK(size, 0));     // 헤더를 가용 상태로 설정
//     PUT(FTRP(bp), PACK(size, 0));     // 푸터를 가용 상태로 설정
//     coalesce(bp);                     // 인접 가용 블록들과 병합
// }

// /*
//  * mm_realloc - realloc을 malloc과 free 기반으로 구현
//  */
// // void *mm_realloc(void *ptr, size_t size)
// // {
// //     void *oldptr = ptr;      // 기존 블록 포인터
// //     void *newptr;            // 새로 할당할 블록 포인터
// //     size_t copySize;         // 복사할 데이터 크기

// //     newptr = mm_malloc(size); // 새로운 크기로 할당 시도
// //     if (newptr == NULL)
// //         return NULL;

// //     // 이전 블록의 크기를 얻어서 복사할 크기를 결정
// //     copySize = *(size_t *)((char *)oldptr - SIZE_T_SIZE);
// //     if (size < copySize)
// //         copySize = size;
// //     memcpy(newptr, oldptr, copySize); // 데이터 복사
// //     mm_free(oldptr);                  // 기존 블록 해제
// //     return newptr;
// // }


// static void remove__(void *bp)
// {
//     // 다음 블록 free, 할당 여부
//     size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
//     // 현재 블록 크기
//     size_t size = GET_SIZE(HDRP(bp));             

//     // Case2: 이전 할당, 다음 free인 경우
//     if (!next_alloc) {
//         // 다음 블록 크기 추가
//         size += GET_SIZE(HDRP(NEXT_BLKP(bp)));         

//         // 병합된 내용을 헤더와, 풋터에 추가
//         PUT(HDRP(bp), PACK(size, 0));
//         PUT(FTRP(bp), PACK(size, 0));
//     }
    
//     //  병합 끝난 뒤에, 업데이트
//     find_nextp = bp;    
// }

// // GPT Said 
// void *mm_realloc(void *ptr, size_t size)
// {
//     if (ptr == NULL) {
//         // realloc(NULL, size)는 malloc(size)와 같음
//         return mm_malloc(size);
//     }

//     if (size == 0) {
//         // realloc(ptr, 0)은 free(ptr) 후 NULL 반환
//         mm_free(ptr);
//         return NULL;
//     }

//     size_t oldsize = GET_SIZE(HDRP(ptr));
//     size_t asize;

//     if (size <= DSIZE) {
//         asize = 2 * DSIZE;
//     } else {
//         asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
//     }

//     // [1] 새 요청 크기가 기존 블록보다 작거나 같으면 그냥 반환
//     if (asize <= oldsize) {
//         return ptr;
//     }

//     // [2] 다음 블록이 free고 합쳐서 충분하면 합치기
//     void *next_bp = NEXT_BLKP(ptr);
//     if (!GET_ALLOC(HDRP(next_bp))) {
//         size_t nextsize = GET_SIZE(HDRP(next_bp));
//         if (oldsize + nextsize >= asize) {
//             // 다음 블록과 합쳐서 크기 키우기
//             remove__(ptr);

//             size_t newsize = oldsize + nextsize;
//             PUT(HDRP(ptr), PACK(newsize, 1));
//             PUT(FTRP(ptr), PACK(newsize, 1));
//             return ptr;
//         }
//     }

//     // [3] 둘 다 안 되면 새로 malloc → 데이터 복사 → old free
//     void *newptr = mm_malloc(size);
//     if (newptr == NULL) {
//         return NULL;
//     }

//     size_t copySize = oldsize - DSIZE; // 실제 payload 크기
//     if (size < copySize) {
//         copySize = size;
//     }
//     memcpy(newptr, ptr, copySize);
//     mm_free(ptr);
//     return newptr;
// }



////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////

/*
 * mm-final.c - 명시적 가용 리스트 + 주소순서 삽입 + Best Fit + realloc 최적화
 */
// #include <stdio.h>
// #include <stdlib.h>
// #include <unistd.h>
// #include <assert.h>
// #include <string.h>
// #include "mm.h"
// #include "memlib.h"

// /* 팀 정보 */
// team_t team = {
//     "ateam", "Harry Bovik", "bovik@cs.cmu.edu", "", ""
// };

// /* 매크로 */
// #define WSIZE 8
// #define DSIZE 16
// // #define WSIZE 4     // implicit
// // #define DSIZE 8     // implicit
// #define CHUNKSIZE (1<<12)

// #define MAX(x, y) ((x) > (y) ? (x) : (y))
// #define PACK(size, alloc) ((size) | (alloc))
// #define GET(p) (*(unsigned int *)(p))
// #define PUT(p, val) (*(unsigned int *)(p) = (val))
// #define GET_SIZE(p) (GET(p) & ~0x7)
// #define GET_ALLOC(p) (GET(p) & 0x1)
// #define HDRP(bp) ((char *)(bp) - WSIZE)
// #define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)
// #define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)))
// #define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE))

// #define GET_SUCC(bp) (*(void **)((char *)(bp) + WSIZE))
// #define GET_PRED(bp) (*(void **)(bp))

// /* 전역변수 */
// static char *heap_listp;
// static char *next_listp;    // next fit을 위한 전역변수

// /* 함수 선언 */
// static void *extend_heap(size_t words);
// static void *coalesce(void *bp);
// static void *find_fit(size_t asize);
// static void place(void *bp, size_t asize);
// static void add_free_block(void *bp);
// static void splice_free_block(void *bp);

// /* mm_init */
// int mm_init(void)
// {
//     if ((heap_listp = mem_sbrk(8 * WSIZE)) == (void *)-1)
//         return -1;
//     PUT(heap_listp, 0);     // 맨 앞 패딩. 정렬을 위한 패딩.
//     PUT(heap_listp + (1 * WSIZE), PACK(2 * WSIZE, 1));  // 프롤로그 헤더.
//     PUT(heap_listp + (2 * WSIZE), PACK(2 * WSIZE, 1));  // 프롤로그 풋터.
//     PUT(heap_listp + (3 * WSIZE), PACK(4 * WSIZE, 0));  // dummy free node 헤더.
//     PUT(heap_listp + (4 * WSIZE), NULL);                // dummy free node, prev.
//     PUT(heap_listp + (5 * WSIZE), NULL);                // dummy free node, next
//     PUT(heap_listp + (6 * WSIZE), PACK(4 * WSIZE, 0));  // dummy free node 풋터.
//     PUT(heap_listp + (7 * WSIZE), PACK(0, 1));          // 에필로그 헤더.

//     // 그래서 처음 payload가 오는 곳은 heap_list에서 dummy free 헤더 다음인
//     // 4 * WSIZE 뒤로 설정.
//     heap_listp += (4 * WSIZE);
//     next_listp = heap_listp;


//     if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
//         return -1;
//     return 0;
// }

// /* extend_heap */
// static void *extend_heap(size_t words)
// {

//     char *bp;

//     size_t size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;

//     // 이제 진짜 사이즈 늘려주고, 원래 있던 brk 값 bp에 저장.
//     if ((long)(bp = mem_sbrk(size)) == -1)
//         return NULL;
    
//     PUT(HDRP(bp), PACK(size, 0));
//     PUT(FTRP(bp), PACK(size, 0));

//     // 그리고 에필로그 다시 설정.
//     PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

//     // for next-fit
//     next_listp = coalesce(bp);
//     return next_listp;
// }

// /* add_free_block - 주소순서 삽입 */
// static void add_free_block(void *bp)
// {
//     // cur 은 free list의 맨 앞.
//     void *cur = heap_listp;
//     void *prev = NULL;

//     // version 1 : 맞는 위치 찾아가야 해서, 시간복잡도 O(N)
//     // free 블럭의 주소 순서대로 free list의 순서를 설정한 것 같음.
//     while (cur != NULL && cur < bp) {
//         prev = cur;
//         cur = GET_SUCC(cur);
//     }

//     if (prev != NULL) {
//         GET_SUCC(prev) = bp;
//     }
//     else {
//         heap_listp = bp;
//     }

//     if (cur != NULL) {
//         GET_PRED(cur) = bp;
//     }
    
//     GET_PRED(bp) = prev;
//     GET_SUCC(bp) = cur;

// }

// /* splice_free_block */
// static void splice_free_block(void *bp)
// {
//     // 받아온 주소의 블록을 free list에서 제거해야 하는상황
//     // 설명을 간단히 하기 위해 선임자를 1, 자신을 2, 후임자를 3으로 하겠음.
//     // 1 - 2 - 3 이렇게 연결되어 있는 형태에서 2를 제거하고 싶은거임.

//     // 만약 2의 선임자가 있다?
//     if (GET_PRED(bp))
//         // 있으면 선임자의 후임자를 받아온 블록의 후임자로 이어주기.
//         // 1 -> 3 이렇게 된거임.
//         GET_SUCC(GET_PRED(bp)) = GET_SUCC(bp);
    
//     // 만약 2의 선임자가 없다? -> 이말은 2가 리스트의 맨 앞이라는 거임.
//     else
//         // 그러면 그냥 2의 후임자가 free list의 맨 앞이 되면 끝.
//         heap_listp = GET_SUCC(bp);

//     // 만약 2의 후임자가 있다?
//     if (GET_SUCC(bp))
//         // 그러면 2의 후임자의 전임자를, 2의 전임자로 이어주기
//         // 1 <- 3 이렇게 만들어준거임.
//         GET_PRED(GET_SUCC(bp)) = GET_PRED(bp);
// }

// /* coalesce */
// static void *coalesce(void *bp)
// {
//     // 받아온 노드의 주소 이전, 다음 블록의 주소 저장.
//     size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
//     size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
//     // 현재 블록의 크기도 저장.
//     size_t size = GET_SIZE(HDRP(bp));

//     // 만약 이전, 다음 블록 모두 할당이 되어있다?
//     if (prev_alloc && next_alloc) {
//         // 그러면 그냥 지금 상태 그대로 free list에 넣어주면 됨.
//         add_free_block(bp);

//         // 추가했으면 끝.
//         return bp;
//     }
//     // 근데 이전은 할당이 되어있고, 다음은 free 상태다?
//     else if (prev_alloc && !next_alloc) {
//         // 그러면 일단 다음 블록이랑 합쳐줘야 하니까,
//         // 다음 블록을 free list에서 빼줘야 함.
//         splice_free_block(NEXT_BLKP(bp));

//         // 다음 free 블록의 크기까지 더해줘서
//         size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
//         // 현재 헤더에 변경된 size와 할당 여부를 바꿔주고,
//         PUT(HDRP(bp), PACK(size, 0));
//         // size가 바뀌엇기 때문에, 풋터의 위치도 알아서 바뀌어있음.
//         PUT(FTRP(bp), PACK(size, 0));
//     }
//     // 이전은 free 이고, 다음은 할당이 되어있다?
//     else if (!prev_alloc && next_alloc) {
//         splice_free_block(PREV_BLKP(bp));

//         size += GET_SIZE(HDRP(PREV_BLKP(bp)));


//         bp = PREV_BLKP(bp);
//         PUT(HDRP(bp), PACK(size, 0));
//         PUT(FTRP(bp), PACK(size, 0));
//         // ??? : 이렇게 하면 안되나요?
//         // -> ㅇㅇ 됨. 사실 이게 더 안전함.
//     }
//     // 이제 둘다 free 인 경우
//     else {
//         // 이전도, 다음도 모두 free list에서 제거 해줘야 함.
//         splice_free_block(PREV_BLKP(bp));
//         splice_free_block(NEXT_BLKP(bp));
//         // 사이즈도 다시 재조정 해주고,
//         size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
//         // 주소를 이전 블록의 payload로 다시 맞춰주고
//         bp = PREV_BLKP(bp);
//         // 헤더, 푸터 다시 설정.
//         PUT(HDRP(bp), PACK(size, 0));
//         PUT(FTRP(bp), PACK(size, 0));
//     }

//     add_free_block(bp);

//     return bp;
// }

// static void *find_fit(size_t asize)
// {
//     // asize보다 큰 곳 찾기. 근데 가장 작은 놈으로.
//     void *bp = next_listp;
//     void *home = heap_listp;

//     // free list 처음부터 쭉 찾기 시작합니다
//     for(;bp != NULL;bp = GET_SUCC(bp))
//     {
//         size_t bsize = GET_SIZE(HDRP(bp));
//         if (bsize >= asize)
//         {
//             return bp;
//         }
//     }
//     for(bp = home; bp != next_listp ; bp = GET_SUCC(bp))
//     {
//         size_t bsize = GET_SIZE(HDRP(bp));
//         if (bsize >= asize)
//         {
//             return bp;
//         }
//     }
    
//     return NULL;
// }


// /* place */
// static void place(void *bp, size_t asize)
// {
//     // bp 가 들어갈 free 블럭의 사이즈를 일단 받아오고
//     size_t csize = GET_SIZE(HDRP(bp));
    
//     ///// - for next-fit - /////

//     next_listp = GET_SUCC(bp);
//     if (next_listp == NULL)
//         next_listp = heap_listp;

//     ////////////////////////////

//     // 해당 free 블록을 free list에서 없애주기
//     splice_free_block(bp);


//     // 만약에 free 블록을 사용할때, 쓰고 남은 크기가 32바이트보다 크면,
//     if ((csize - asize) >= (2 * DSIZE)) {
//         // 해당 블록에 사용할 크기만 할당해주고
//         PUT(HDRP(bp), PACK(asize, 1));
//         PUT(FTRP(bp), PACK(asize, 1));

//         // 남은 free 블록은 바로 다음 블록을 테니까
//         bp = NEXT_BLKP(bp);
//         // 남은 free 블록은 사이즈 재정의
//         PUT(HDRP(bp), PACK(csize - asize, 0));
//         PUT(FTRP(bp), PACK(csize - asize, 0));
//         // 다 했으면 남은 free 블록은 free list에 추가해주기.
//         add_free_block(bp);

//         // for next-fit
//         next_listp = bp;
//     }
//     // 만약 남은 크기가 32바이트보다 작으면 그냥 다 쓰지 뭐. 
//     else
//     {
//         PUT(HDRP(bp), PACK(csize, 1));
//         PUT(FTRP(bp), PACK(csize, 1));
//     }

// }

// /* mm_malloc */
// void *mm_malloc(size_t size)
// {
//     // size만큼 할당할 공간 만들어줘야 함.
//     size_t asize;
//     size_t extendsize;
//     char *bp;
//     // 만약 0을 늘려라? 장난치나; 바로 NULL
//     if (size == 0)
//         return NULL;

//     // 늘려야 할 크기가 정렬 기준인 16바이트보다 작다?
//     if (size <= DSIZE)
//         asize = 2 * DSIZE;
//     // 크기가 16보다 크면?
//     else
//         // 이렇게 크기에 따라서 패딩해주면 됨.
//         asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

//     // 만약에 fit한곳을 찾지못하면 확장하고, fit한 곳이 있으면 거기 넣기.
//     if ((bp = find_fit(asize)) != NULL) {
//         // 있으니까 넣어줘야겠지
//         place(bp, asize);
//         return bp;
//     }

//     extendsize = MAX(asize, CHUNKSIZE);
//     // 해서 확장을 할건데 확장에 실패하면 NULL 반환하고,
//     if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
//         return NULL;
//     // 확장이 됐으면, 확장이 된 공간에 정보를 넣으면 되겠지?
//     place(bp, asize);
//     return bp;
// }

// /* mm_free */
// void mm_free(void *bp)
// {
//     // free로 만들 블록의 크기를 일단 받고
//     size_t size = GET_SIZE(HDRP(bp));

//     // 그 크기만큼 다 메모리 할당 여부 0으로 해주면 됨.
//     PUT(HDRP(bp), PACK(size, 0));
//     PUT(FTRP(bp), PACK(size, 0));
//     // 이후에 인접 블록들과 병합
//     next_listp = coalesce(bp);
// }

// /* mm_realloc - in-place 최적화 */
// // 이미 할당된 블록의 크기를 변경해주는게 realloc
// // 변경된 블록의 payload 시작주소 반환하면 되겠지.
// void *mm_realloc(void *ptr, size_t size)
// {
//     // ptr은 현재 블록의 주소
//     // 만약에 블록이 NULL이면, size크기만큼 새로 할당해줘야겠지.
//     if (ptr == NULL)
//         return mm_malloc(size);
//     // 만약에 변경하고 싶은 블록의 크기가 0이면, 메모리를 해제하겠다는 말과 같으니까
//     // 바로 free
//     if (size == 0) {
//         mm_free(ptr);
//         return NULL;
//     }

//     // 현재 블록의 크기 일단 저장해두고
//     size_t oldsize = GET_SIZE(HDRP(ptr));
//     // 정렬 기준을 맞추기 위해 패딩까지 합칠 블록의 크기 설정 -> asize
//     size_t asize = (size <= DSIZE) ? (2 * DSIZE) : DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);

//     // 만약 asize가 기존 사이즈보다 작다? 즉, 블록을 축소하고 싶다?
//     if (asize <= oldsize)
//         // 어쩌라고 안줄일거야. 왜냐면 줄이게되면, 데이타 손실 생김.
//         return ptr;

//     void *next = NEXT_BLKP(ptr);
//     // 다음 블록이 free이고, 크기가 충분하면,
//     if (!GET_ALLOC(HDRP(next)) && (oldsize + GET_SIZE(HDRP(next))) >= asize) {
//         // free list에서 블록 빼주고
//         splice_free_block(next);
//         // 다시 크기 재정의 해서 헤더, 풋터에 수정
//         size_t newsize = oldsize + GET_SIZE(HDRP(next));
//         PUT(HDRP(ptr), PACK(newsize, 1));
//         PUT(FTRP(ptr), PACK(newsize, 1));
//         return ptr;
//     }

//     void *newptr = mm_malloc(size);
//     // 만약에 더이상 할당할 수 없으면 바로 리턴
//     if (newptr == NULL)
//         return NULL;

//     size_t copySize = oldsize - DSIZE;
//     // 사이즈가 작아지길 원하면, payload의 부분을 줄여서 ㄱㄱ
//     if (size < copySize)
//         copySize = size;

//     memcpy(newptr, ptr, copySize);
//     mm_free(ptr);

//     return newptr;
// }