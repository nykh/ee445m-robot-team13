#ifndef __HEAP_H__
#define __HEAP_H__

#define NULL 0

void Heap_Init(void);
long* Heap_Malloc(unsigned int requestedSpace);
void Heap_Free(void* inPtr);

#endif // __HEAP_H__
