/* heap2.c
 * Author: Siavash Zangeneh Kamali
 *         Nick Huang
 * Implements Knuth HEAP, each frame stores the size of chunk. 
 *
 * 2014/3/5 Modified to short to reduce waste of space for storing frame markers
 *          Modified to use pointers
 *          HEAP_Free will attempt to form a maximum combined chunk possible.
 *
 * 2014/4/17 Figured out why this crashes the system: alignment problem
 *           Solved by changing the data type to long
 */
 
 
#include "heap2.h"

HEAPTYPE HEAP[HEAP_SIZE];

static HEAPTYPE *LastFreeBlock = HEAP;

/* HEAP_Init
 * initialize the heap, can be called multiple times to reinitialize the heap. Non-reentrant when reinitializing.
 * i/o: none
 */
void HEAP_INIT(void) {
	int i;	
	// These now allow HEAP to be reinitialized
	LastFreeBlock = HEAP;
	for(i = 1; i < HEAP_SIZE-1; ++i) HEAP[i] = 0;
	HEAP[0] = HEAP[HEAP_SIZE-1] = HEAP_SIZE-2;
}

/* HEAP_Malloc
 * Allocates a chunk of memory on heap
 * input: requested size in unit of char, the function will round up to short  
 * output: the void pointer pointing to the first location of the memory chunk.
 */
void* HEAP_MALLOC(unsigned short requestedSpace) { // in unit of byte
	HEAPTYPE *fp;
	short BlockSize;
	requestedSpace = ((requestedSpace+7)/8)*2;
	// round up to unit of 8 bytes, then convert to unit of short
	
	for (fp = LastFreeBlock ; fp < &HEAP[HEAP_SIZE-1]; fp += BlockSize+2){
		BlockSize = *fp;
		
		if (BlockSize > 0 ) { // Free block
			if (requestedSpace <= BlockSize) {
				if (BlockSize - requestedSpace < 3) { // Result of (BlockSize - requestedSpace) is non-negative, so no bug
					fp[0] = fp[BlockSize+1] = -BlockSize;
					LastFreeBlock = fp + BlockSize + 2;
					return (void *) &fp[1];
				} else { // Cut out a chunk from the big chunk
					fp[0] = fp[requestedSpace + 1] = -requestedSpace;
					fp[requestedSpace+2] = fp[BlockSize+1] = (BlockSize-requestedSpace-2);
					LastFreeBlock = fp + requestedSpace + 2;
					return (void *) &fp[1];
				}
			} 
		} else if (BlockSize < 0 ) { // Occupied
			BlockSize = -BlockSize;  // Take the absolute value of BlockSize to skip this block
		} else { // ERROR
			return 0;
		}
	}
	
	// Only here when the whole heap is full
	return 0;
}

/* HEAP_Free
 * Free a chunk of memory. It does not clear the memory 
 * therefore it is possible to continuing accessing the memory
 * after freeing, however it is not safe.
 * input: Pointer pointing to the first location of the memory chunk
 * output: none
 */
void HEAP_FREE(void* inPtr) {
	HEAPTYPE *fp = (HEAPTYPE *) inPtr - 1;
	short blockSize = *fp;
	HEAPTYPE *nfp = fp + blockSize + 2;
	short nextBlockSize;
	short combinedSize = blockSize;
	
	if(nfp < &HEAP[HEAP_SIZE-1] && (nextBlockSize = nfp[0]) > 0) {
		combinedSize += nextBlockSize + 2;
	}
	
	fp[0] = fp[combinedSize+1] = combinedSize;
	
	if(LastFreeBlock > fp) LastFreeBlock = fp;
}
