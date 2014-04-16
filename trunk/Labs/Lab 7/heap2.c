/* heap.c
 * Author: Siavash Zangeneh Kamali
 *         Nick Huang
 * Implements Knuth Heap, each frame stores the size of chunk. 
 *
 * 2014/3/5 Modified to short to reduce waste of space for storing frame markers
 *          Modified to use pointers
 *          Heap_Free will attempt to form a maximum combined chunk possible.
 */
 
#define HEAP_SIZE 3000

short Heap[HEAP_SIZE];

short *LastFreeBlock = Heap;

/* Heap_Init
 * initialize the heap, can be called multiple times to reinitialize the heap. Non-reentrant when reinitializing.
 * i/o: none
 */
void Heap_Init(void) {
	int i;	
	// These now allow Heap to be reinitialized
	LastFreeBlock = Heap;
	for(i = 0; i < HEAP_SIZE; ++i) Heap[i] = 0;
	Heap[0] = Heap[HEAP_SIZE-1] = HEAP_SIZE-2;
}

/* Heap_Malloc
 * Allocates a chunk of memory on heap
 * input: requested size in unit of char, the function will round up to short  
 * output: the void pointer pointing to the first location of the memory chunk.
 */
void* Heap_Malloc(unsigned short requestedSpace) { // in unit of byte
	short *fp;
	short BlockSize;
	requestedSpace = ((requestedSpace+7)/8)*4;
	// round up to unit of 8 bytes, then convert to unit of short
	
	for (fp = LastFreeBlock ; fp < &Heap[HEAP_SIZE-1]; fp += BlockSize+2){
		BlockSize = *fp;
		
		if (BlockSize > 0 ) { // Free block
			if (requestedSpace <= BlockSize) {
				if (BlockSize - requestedSpace < 3) { // Result of (BlockSize - requestedSpace) is non-negative, so no bug
					fp[0] = fp[BlockSize+1] = -BlockSize;
					LastFreeBlock = fp + BlockSize + 2;
					return (void *) fp[1];
				} else { // Cut out a chunk from the big chunk
					fp[0] = fp[requestedSpace + 1] = -requestedSpace;
					fp[requestedSpace+2] = fp[BlockSize+1] = (BlockSize-requestedSpace-2);
					LastFreeBlock = fp + requestedSpace + 2;
					return (void *) fp[1];
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

/* Heap_Free
 * Free a chunk of memory. It does not clear the memory 
 * therefore it is possible to continuing accessing the memory
 * after freeing, however it is not safe.
 * input: Pointer pointing to the first location of the memory chunk
 * output: none
 */
void Heap_Free(void* inPtr) {
	short *fp = (short *) inPtr - 1;
	short blockSize = *fp;
	short *nfp = fp + blockSize + 2;
	short nextBlockSize;
	short combinedSize = blockSize;
	
	if(nfp < &Heap[HEAP_SIZE-1] && (nextBlockSize = nfp[0]) > 0) {
		combinedSize += nextBlockSize + 2;
	}
	
	fp[0] = fp[combinedSize+1] = combinedSize;
	
	if(LastFreeBlock > fp) LastFreeBlock = fp;
}
