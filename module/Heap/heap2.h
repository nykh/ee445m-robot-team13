#ifndef __HEAP_H__
#define __HEAP_H__

#define NULL 0

void Heap_Init(void);
void* Heap_Malloc(unsigned short requestedSpace);
void Heap_Free(void* inPtr);

#endif // __HEAP_H__
