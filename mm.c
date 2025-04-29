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
    "ㅇㅇㅇ",
    /* First member's full name */
    "홍길동",
    /* First member's email address */
    "ㅇ",
    /* Second member's full name (leave blank if none) */
    "ㅇㅇㅇ",
    /* Second member's email address (leave blank if none) */
    "ㅇㅇㅇㅇ"};

/* 단일 워드(4바이트) 또는 더블 워드(8바이트) 정렬 기준 */
#define ALIGNMENT 8

/* ALIGNMENT 배수로 올림 */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

/* size_t 타입 크기를 ALIGNMENT 배수로 정렬한 크기 */
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* 힙 시작 포인터(global) */
static char *heap_listp = NULL;
static char *free_listp = NULL;
// static void *find_nextp; //전역 포인터: Next-Fit 탐색 시작 위치 저장

/* 함수 프롤토타입 */
static void *coalesce(void *bp);          /* 인접 가용 블록 병합 */
static void *find_fit(size_t asize);      /* 가용 블록 탐색 */
static void place(void *bp, size_t asize);/* 블록 배치 및 분할 처리 */


// // implicit-list
// /////////////////////////////////////////////////////////////////////////
// /////////////////////////////////////////////////////////////////////////
// /////////////////////////////////////////////////////////////////////////


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

// // // // first_fit
// // static void *find_fit(size_t asize)
// // {
// //     void *bp;

// //     for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
// //         if (!GET_ALLOC(HDRP(bp))) {
// //             size_t bsize = GET_SIZE(HDRP(bp));

// //             // for first-fit
// //             if (bsize >= asize)
// //                 return bp;

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
// //     return NULL;
// // }

// // Next - fit
// /* 적합한 블록 탐색 (Next-fit) */
// static void *find_fit(size_t asize) {
//     void *ptr;
//     ptr = find_nextp;   // 현재 탐색 시작 위치를 저장

//     // 첫 번째 루프: 현재 find_nextp부터 힙 끝까지 탐색
//     for (; GET_SIZE(HDRP(find_nextp)) >0; find_nextp = NEXT_BLKP(find_nextp))
//     {   
//         // if (GET_SIZE(HDRP(find_nextp)) == 0) {
//         //     break;
//         // }
//         // 만약 블록이 할당되어 있지 않고, 크기가 요청한 크기 이상이면
//         // printf("1번");
//         if (!GET_ALLOC(HDRP(find_nextp)) && (asize <= GET_SIZE(HDRP(find_nextp))))
//         {
//             // printf("2번");
//             return find_nextp;  // 해당 블록을 반환
//         }
//     }
//     // 두 번째 루프: 힙 처음(heap_listp)부터 아까 저장해둔 ptr까지 탐색
//     for (find_nextp = heap_listp; find_nextp != ptr; find_nextp = NEXT_BLKP(find_nextp))
//     {   
//         // printf("3번");
//         // 마찬가지로 할당되어 있지 않고 크기가 충분하면
//         if (!GET_ALLOC(HDRP(find_nextp)) && (asize <= GET_SIZE(HDRP(find_nextp))))
//         {
//             // printf("4번");
//             return find_nextp;  // 해당 블록을 반환
//         }
//     }
//     // printf("5번");
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
// // /* mm_init: 초기 힙 설정 및 프롤로그/에필로그 블록 생성 */
// // int mm_init(void)
// // {
// //     /* 초기 힙 영역 확보(4워드) */
// //     if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
// //         return -1;

// //     /* 8바이트 정렬 패딩 */
// //     PUT(heap_listp, 0);
// //     /* 프롤로그 헤더(크기=8, 할당됨) */
// //     PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
// //     /* 프롤로그 푸터(크기=8, 할당됨) */
// //     PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
// //     /* 에필로그 헤더(크기=0, 할당됨) */
// //     PUT(heap_listp + (3 * WSIZE), PACK(0, 1));
// //     /* heap_listp를 프롤로그 블록의 페이로드 시작으로 이동 */
// //     heap_listp += (2 * WSIZE);

// //     /* 힙을 CHUNKSIZE 바이트 만큼 확장하여 첫 가용 블록 생성 */
// //     if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
// //         return -1;
// //     return 0;
// // }

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

// // binary, realloc

// // expr, amptjp, cccp
// // cp-decl



// explicit-list
//////////////////////////////////////////////////////////////////////
////////////////////////// explicit-list /////////////////////////////
//////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////
//////////////////////////// * 경호 * //////////////////////////////////
///////////////////////////////////////////////////////////////////////


static void insert_free_block(void *bp);
static void remove_free_block(void *bp);
static void *extend_heap(size_t words);


// 힙 확장: 새로운 메모리를 받아서 free block으로 만듦
static void *extend_heap(size_t words) {
    char *bp;
    size_t size;

    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;

    if ((long)(bp = mem_sbrk(size)) == -1) {
        return NULL;
    }

    PUT(HDRP(bp), PACK(size, 0)); // 새 free block의 헤더
    PUT(FTRP(bp), PACK(size, 0)); // 새 free block의 푸터
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // 새로운 에필로그 블록

    return coalesce(bp); // 주변과 합칠 수 있으면 합치기
}

// 블록 연결(coalesce): 주위 블록과 병합
static void *coalesce(void *bp) {
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) { // 앞뒤 다 사용 중이면 그냥 free list에 추가
        insert_free_block(bp);
        return bp;
    }
    else if (prev_alloc && !next_alloc) { // 뒤가 비어 있으면 합치기
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        remove_free_block(NEXT_BLKP(bp));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc) { // 앞이 비어 있으면 합치기
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        remove_free_block(PREV_BLKP(bp));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp); // 앞으로 포인터 이동
    }
    else { // 앞뒤 둘 다 비어 있으면 셋 다 합치기
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        remove_free_block(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        remove_free_block(PREV_BLKP(bp));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    insert_free_block(bp);
    return bp;
}

// 첫 번째 맞는 블록 찾기 (first fit)
static void *find_fit(size_t asize) {
    void *cur = free_listp;
    while (cur != NULL) {
        if (GET_SIZE(HDRP(cur)) >= asize) {
            return cur;
        }
        cur = SUC(cur); // 다음 free block으로 이동
    }
    return NULL; // 못 찾으면 NULL
}

// 블록 배치(place): 필요한 크기만큼 쪼개고 나머지는 새 free block으로
static void place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp));

    if ((csize - asize) >= 2 * DSIZE) { // 나머지 조각이 최소 블록 크기 이상이면
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        remove_free_block(bp);

        bp = NEXT_BLKP(bp); // 나머지 조각 새 블록
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
        insert_free_block(bp);
    }
    else { // 그냥 다 사용
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
        remove_free_block(bp);
    }
}

// free block을 explicit free list에 추가
static void insert_free_block(void *bp) {
    // 새 블록(bp)의 successor를 현재 free list의 첫 번째 블록(free_listp)으로 설정
    SUC(bp) = free_listp;

    // 새 블록(bp)의 predecessor를 NULL로 설정
    // -> 새 블록이 이제 리스트의 맨 앞(head)이므로
    PRED(bp) = NULL;

    // 만약 기존에 free list에 블록들이 있었던 경우
    if (free_listp != NULL)
        // 기존 첫 번째 블록의 predecessor를 새 블록(bp)으로 설정
        PRED(free_listp) = bp;

    // free_listp를 새 블록(bp)으로 갱신
    // -> 새 블록을 리스트의 첫 번째 블록으로 갱신
    free_listp = bp;
}

// free block을 explicit free list에서 제거
static void remove_free_block(void *bp) {
    // bp 블록의 이전 블록(pred)
    void *pred = PRED(bp);
    // bp 블록의 다음 블록(succ)
    void *succ = SUC(bp);

    // bp 앞에 다른 블록(pred)이 있는 경우
    if (pred != NULL)
        // pred 블록의 successor를 bp의 successor로 연결
        SUC(pred) = succ;
    else
        // bp가 free list의 맨 앞이라면 첫 블록 주소를 bp 블록의 다음 블록으로 변경
        free_listp = succ;

    // bp 뒤에 다른 블록(succ)이 있는 경우
    if (succ != NULL)
        // succ 블록의 predecessor를 bp의 predecessor로 연결
        PRED(succ) = pred;
}

/*********************************************************
 * 외부 인터페이스 함수들 (main 함수에서 호출)
 ********************************************************/

// 힙 초기화
int mm_init(void) {
    if ((heap_listp = mem_sbrk(8*WSIZE)) == (void *)-1) {
        return -1;
    }

    PUT(heap_listp, 0);                         // 패딩
    PUT(heap_listp + (1*WSIZE), PACK(DSIZE, 1)); // 프로로그 헤더
    PUT(heap_listp + (2*WSIZE), PACK(DSIZE, 1)); // 프로로그 푸터
    PUT(heap_listp + (3*WSIZE), PACK(2*DSIZE, 0)); // dummy free block 헤더
    PUT(heap_listp + (4*WSIZE), NULL);           // dummy block pred
    PUT(heap_listp + (5*WSIZE), NULL);           // dummy block succ
    PUT(heap_listp + (6*WSIZE), PACK(2*DSIZE, 0)); // dummy block 푸터
    PUT(heap_listp + (7*WSIZE), PACK(0, 1));     // 에필로그 헤더
    heap_listp += (4*WSIZE);

    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) {
        return -1;
    }
    // if (extend_heap(16) == NULL)                     //자주 사용되는 작은 블럭이 잘 처리되어 점수가 오름
    //     return -1;

    return 0;
}

// 메모리 할당
void *mm_malloc(size_t size) {
    size_t asize;
    size_t extendsize;
    char *bp;

    if (size == 0) {
        return NULL;
    }

    if (size <= DSIZE) {
        asize = 2 * DSIZE;
    } else {
        asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
    }

    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL) {
        return NULL;
    }
    place(bp, asize);
    return bp;
}

// 메모리 해제
void mm_free(void *ptr) {
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));
    coalesce(ptr);
}

// 메모리 크기 재조정
void *mm_realloc(void *ptr, size_t size) {
    if (ptr == NULL)
        return mm_malloc(size);
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    size_t old_block = GET_SIZE(HDRP(ptr));
    size_t old_payload = old_block - WSIZE;
    size_t new_asize;

    if (size <= DSIZE) {
        new_asize = 2 * DSIZE;
    } else {
        new_asize = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
    }

    // 1. in-place 축소
    if (new_asize <= old_block) {
        if (old_block - new_asize >= 2 * DSIZE) {
            PUT(HDRP(ptr), PACK(new_asize, 1));
            PUT(FTRP(ptr), PACK(new_asize, 1));
            void *nbp = NEXT_BLKP(ptr);
            size_t remain = old_block - new_asize;
            PUT(HDRP(nbp), PACK(remain, 0));
            PUT(FTRP(nbp), PACK(remain, 0));
            insert_free_block(nbp);
        }
        return ptr;
    }

    // 2. in-place 확장
    if (!GET_ALLOC(HDRP(NEXT_BLKP(ptr)))) {
        size_t nsize = old_block + GET_SIZE(HDRP(NEXT_BLKP(ptr)));
        if (nsize >= new_asize) {
            remove_free_block(NEXT_BLKP(ptr));
            PUT(HDRP(ptr), PACK(nsize, 1));
            PUT(FTRP(ptr), PACK(nsize, 1));
            return ptr;
        }
    }

    // 3. 새로 malloc -> 데이터 복사
    void *new_ptr = mm_malloc(size);
    if (new_ptr == NULL)
        return NULL;

    size_t copy = (old_payload < size) ? old_payload : size;
    memcpy(new_ptr, ptr, copy);
    mm_free(ptr);
    return new_ptr;
}