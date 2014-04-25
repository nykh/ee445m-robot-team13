#define HEAP_SIZE 1500

long Heap[HEAP_SIZE];
unsigned int LastFreeBlock;

void Heap_Init(void) {
	Heap[0] = HEAP_SIZE-2;
	Heap[HEAP_SIZE-1] = HEAP_SIZE-2;
	LastFreeBlock = 0;
}

long* Heap_Malloc(unsigned int requestedSpace) {
	unsigned int index;
	int interval;
	requestedSpace = (requestedSpace+3) / 4;
	for (index = LastFreeBlock ; index < HEAP_SIZE; index += interval){
		int BlockSize = Heap[index];
		if (BlockSize > 0 ) {
			if (requestedSpace <= BlockSize) {
				if (BlockSize - requestedSpace < 3) {
					Heap[index] = -BlockSize;
					Heap[index+BlockSize+1] = -BlockSize;
					LastFreeBlock = index+BlockSize+2;
					return &Heap[index+1];
				} else {
					Heap[index] = -requestedSpace;
					Heap[index + requestedSpace + 1 ] = -requestedSpace;
					LastFreeBlock = index+requestedSpace+2;
					Heap[LastFreeBlock ] = BlockSize - requestedSpace -2;
					Heap[index + BlockSize + 1 ] = BlockSize - requestedSpace - 2;
					return &Heap[index+1];
				}
			} else {
				interval = Heap[index] + 2;
			}
		} else if (BlockSize < 0 ) {
			interval = (-BlockSize) + 2;
		} else {
			return 0;
		}
	}
	return 0;
}

void Heap_Free(void* inPtr) {
	long *blockPtr = (long *) inPtr;
	int blockSize, combinedSize;
	int index = (blockPtr - Heap);
	blockSize = - Heap[index-1];
	if (Heap[index + blockSize + 1] > 0 ) {
		combinedSize = blockSize + Heap[index + blockSize + 1] + 2;
		Heap [index - 1] = combinedSize;
		Heap [index + combinedSize] = combinedSize;
	} else if (Heap[index + blockSize + 1] < 0 ) {
		Heap [index - 1] = blockSize;
		Heap [index + blockSize] = blockSize;
	}	
	if (LastFreeBlock > index-1){
		LastFreeBlock = index - 1;
	}
}
