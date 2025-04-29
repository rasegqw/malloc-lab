#include <stdio.h>

#define WSIZE        8
#define DSIZE        16
#define CHUNKSIZE   (1<<12)

#define MAX(x, y)   ((x) > (y) ? (x) : (y))

#define PACK(size, alloc)   ((size) | (alloc))

#define GET(p)          (*(unsigned int *)(p))
#define PUT(p, val)     (*(unsigned int *)(p) = (val))

#define GET_SIZE(p)     (GET(p) & ~0x7)
#define GET_ALLOC(p)    (GET(p) & 0x1)

#define HDRP(bp)        ((char *)(bp) - WSIZE)
#define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

#define NEXT_BLKP(bp)   ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp)   ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

//////////////////////////////////////////////////////////////////////////////
////////////////////////////* implicit *//////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

// #define HDRP(bp)        ((char *)(bp) - WSIZE)
// #define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) //헤더+데이터+풋터 -(헤더+풋터)

// #define NEXT_BLKP(bp)   ((char *)(bp) + GET_SIZE((char *)(bp)))
// #define PREV_BLKP(bp)   ((char *)(bp) - GET_SIZE((bp-3*DSIZE)))

//////////////////////////////////////////////////////////////////////////////
////////////////////////////* explicit *//////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

// #define HDRP(bp)        ((char *)(bp) - WSIZE - 2*DSIZE)
// #define FTRP(bp)        ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) //헤더+데이터+풋터 -(헤더+풋터)

// #define NEXT_BLKP(bp)   ((char *)(bp) + GET_SIZE((char *)(bp)))
// #define PREV_BLKP(bp)   ((char *)(bp) - GET_SIZE(((char *)(bp)-3*DSIZE)))

// #define PRED_FREEP(bp)  (*(void**)(bp - 2*DSIZE))
// #define SUCC_FREEP(bp)  (*(void**)(bp - DSIZE))

#define SUC(bp)  (*(void **)((char *)(bp) + WSIZE)) // 다음 Free Block 주소
#define PRED(bp) (*(void **)(bp))                   // 이전 Free Block 주소


extern int mm_init (void);
extern void *mm_malloc (size_t size);
extern void mm_free (void *ptr);
extern void *mm_realloc(void *ptr, size_t size);


/* 
 * Students work in teams of one or two.  Teams enter their team name, 
 * personal names and login IDs in a struct of this
 * type in their bits.c file.
 */
typedef struct {
    char *teamname; /* ID1+ID2 or ID1 */
    char *name1;    /* full name of first member */
    char *id1;      /* login ID of first member */
    char *name2;    /* full name of second member (if any) */
    char *id2;      /* login ID of second member */
} team_t;

extern team_t team;
