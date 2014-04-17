#ifndef __HEAP_H__
#define __HEAP_H__

// #define __heap_debug__

#define HEAPTYPE  long
#define HEAP_SIZE 3000

#ifdef __heap_debug__
#define HEAP   heap2

extern HEAPTYPE HEAP[HEAP_SIZE];

#define HEAP_INIT    HEAP_Init
#define HEAP_MALLOC  HEAP_Malloc
#define HEAP_FREE    HEAP_Free
#else
#define HEAP   heap

#define HEAP_INIT    Heap_Init
#define HEAP_MALLOC  Heap_Malloc
#define HEAP_FREE    Heap_Free
#endif

#define NULL 0

void HEAP_INIT(void);
void* HEAP_MALLOC(unsigned short requestedSpace);
void HEAP_FREE(void* inPtr);

#endif // __HEAP_H__
