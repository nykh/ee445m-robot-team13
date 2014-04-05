// filename ************** eFile.h *****************************
// Middle-level routines to implement a solid-state disk 
// Jonathan W. Valvano 3/16/11

#include "efile.h"
#include "edisk.h"

/****************************** Global *****************************/
#define NO_FILE_OPEN 0
#define FILE_ROPEN   1
#define FILE_WOPEN   2
unsigned char FILE_OPEN = NO_FILE_OPEN;

// 1 if redirecting, 0 otherwise
unsigned char Redirect_stream;

/**************************** Data Structure ***********************/

#define MAXFILENAME     2 /* bytes */
typedef struct {
	char filename[MAXFILENAME];
	short startAddr; // 2 bytes
	short endAddr;   // 2 bytes
	short size;      // 2 bytes
} DirType;           // 8 bytes

// Definition of SD card space
#define MAXBLOCK            2048 /* 1 MiB */

#define MAXFILETABLE        32
#define START_OF_DATA_BLOCK MAXFILETABLE
#define END_OF_DATA_BLOCK   MAXBLOCK

/* The first table block with the size 8 * 64 = 512 bytes
 * the inititialization function will fill this with the first 64 files in SD card */
#define FREE_SPACE                0
#define NumDirTableEntry          64
#define NumDirTableEntry_One      (NumDirTableEntry-2)

static void longToString (char * buffer, long value);

struct ft1struct {
	short firstFree;                               /*   2 bytes */
	char __wastedspace__2[2];                      /*   2 bytes wasted */
	DirType free;                                  /*   8 bytes */
	DirType datablock[NumDirTableEntry_One];       /* 496 bytes = 62*8 */
	char __wastedspace__4[4];                      /*   4 bytes wasted */
} Filetable_One;                                   /* 512 bytes */

/* a buffer for the dir entry */
char TableNum;
char EntryNum; /* in the abstract filetable */
DirType DirBuf;

/* a buffer for the data block in RAM */
char BlockNum; /* in the physical SD card */
BYTE BlockBuf[512];
char* BlockPt = (char *) &BlockBuf[sizeof(short)];
#define Block_NextAddr (((short *) BlockBuf)[0])

 
/* the SD card can contain a maximum of 2^11 files, which are addressed in 32 file tables
 * the first table is loaded into RAM upon initialization to accelerate the process */

/****************\****** Static Functions **********\***************/
// Function to find the named file in entries
// Input: name - a string of name, note that it must be filled up to 4 characters long with spaces
//        wd - working directory, passed by reference to store the dir info
//        entrynum - passed by reference to store the index of the file in filetable
// Output: -1 if failed to find the file (file does not exist or fail to read from SD card)
//         otherwise returns the table at which it finds the file
// When terminating, the filetable is in BlockBuf or Filetable_One
// Only **reads** the filetables
char openfile(const char *name, DirType *wd, char *entrynum);

// function to save a directory into a filetable
// Input: wd - reference to the working directory
//        wd_at_Table - index of the table
//        entrynum - index of the file in filetable
// Output: 0 if successful and 1 on failure (fails to write)
int savefile(const DirType *wd, char wd_at_Table, char entrynum);

// returns 0 if equal, 1 if not
char my_strcmp2(const char *s, const char *t);

int eFile_Init(void) { // initialize file system
  if(eDisk_Init(0)) return 1;;
  
  eDisk_ReadBlock((BYTE *) &Filetable_One, 0);
	
  return 0; 
}

int eFile_Format(void) { // erase disk, add format
	// Clear file tables
	short i;
	char result = 0;
	
	for(i = 0; i < 512; i++) {
		BlockBuf[i] = 0;
	}
	
	for(i = 1; i < MAXFILETABLE; i++) {
		result |= eDisk_WriteBlock(BlockBuf, i);
	}
	
	// Fails to write the SD card
	if(result) return 1;
	/* Clear the data blocks in the first filetable */ {
		char *name = DirBuf.filename;
		// Clear the filename, using the right associativity of = operator
		*name = *name++ = 0;
		
		DirBuf.startAddr = DirBuf.endAddr = 0;
		DirBuf.size = 0;
		
		for(i = 0; i < NumDirTableEntry_One; i++) {
			Filetable_One.datablock[i] = DirBuf;
		}
	}
	
	// The name of free space block doesn't matter
	Filetable_One.free.startAddr = START_OF_DATA_BLOCK;
	Filetable_One.free.endAddr = END_OF_DATA_BLOCK-1;
	
	Filetable_One.firstFree = 0;
	
	eDisk_WriteBlock((BYTE *) &Filetable_One, 0);
	
	// Link data blocks to each other
	for(i = START_OF_DATA_BLOCK; i < END_OF_DATA_BLOCK; i++) {
		Block_NextAddr = i+1; // the first short of each data block contains the address to the next block
		result |= eDisk_WriteBlock(BlockBuf, i);
	}
	
	FILE_OPEN = NO_FILE_OPEN;
	
	return result;
}

int eFile_Create(char *name) {  // create new file, make it empty 
	DirType *newdir;
	char entry_at_table = -1;
	
	//Checking whether the file already 
	if(openfile(name, &DirBuf, &EntryNum) != -1) {
		return 3;
	}
	
	// Find an empty entry
	if(Filetable_One.firstFree < MAXBLOCK) {
		TableNum = entry_at_table = Filetable_One.firstFree / 64;
		EntryNum = Filetable_One.firstFree % 64;
		if(entry_at_table == 0)
			newdir = &Filetable_One.datablock[EntryNum];
		else {
			if(eDisk_ReadBlock(BlockBuf, entry_at_table)) return 1;
			newdir = (DirType *) &BlockBuf[EntryNum];
		}
	} else { // All table are filled up
		return 1;
	}
	
	/* Create a dir entry in Filetable */ {
		unsigned char counter;
		
		for(counter = 0; counter < 2; counter++) {
			if(*name) {
				newdir->filename[counter] = *name++;
			} else {
				newdir->filename[counter] = ' ';
			}
		}
		
		BlockNum = newdir->startAddr   \
		  = newdir->endAddr \
		  = Filetable_One.free.startAddr;
		
		newdir->size = 1;
		
		// write back to the SD card copy
		if(entry_at_table == 0) {
			if(eDisk_WriteBlock((BYTE *) &Filetable_One, 0)) return 1;
		} else {
			if(eDisk_WriteBlock((BYTE *) BlockBuf, entry_at_table)) return 1;
		}
		
		DirBuf = *newdir; // a readonly copy for reference
	}
	
	if(eDisk_ReadBlock((BYTE *) BlockBuf, DirBuf.startAddr)) return 1;
	// Unlink the block from free space list
	if(Filetable_One.free.startAddr == Filetable_One.free.endAddr) return 2; // SD card is full
	
	Filetable_One.free.startAddr = Block_NextAddr;
	Block_NextAddr = 0;
	
	// Prepare for write operation
	BlockPt = (char *) &BlockBuf[sizeof(short)];
	*BlockPt = EOF;
	
	// !! Commiting here is kinf od optional
	if(eDisk_WriteBlock((BYTE *) BlockBuf, DirBuf.startAddr)) return 1;
		
	// increment first free entry
	// It is safe to assume contiguous allocation, when delete I will always fill up the space with the globally last entry.
	// Because the first filetable is not full (only 62 blocks), for convenience I skip 62, 63 in the firstFree value
	// This ensures that I can always take / 64 as the table number and % 64 as the entry number
	if(++Filetable_One.firstFree == NumDirTableEntry_One) Filetable_One.firstFree = 64;
	
	if(eDisk_WriteBlock((BYTE *) &Filetable_One, 0)) return 1;
	
	FILE_OPEN = FILE_WOPEN;
	
	return 0;
}

int eFile_Close(void) {
	if(!FILE_OPEN) return 1;
	
	// save any opened files
	// savefile may possibly overwrite BlockBuf, therefore it needs to be called last
	if(eDisk_WriteBlock((BYTE *) BlockBuf, BlockNum)) return 1;
	savefile(&DirBuf, TableNum, EntryNum);
	
	FILE_OPEN = NO_FILE_OPEN;
	
	return 0;
}

int eFile_WOpen(char name[]) {      // open a file for writing 
	if(FILE_OPEN) return 1;         // cannot open multiple files
	
	if((TableNum = openfile(name, &DirBuf, &EntryNum)) == -1) {
		return 1;
	}
	
	if(eDisk_ReadBlock((BYTE *) BlockBuf, BlockNum = DirBuf.endAddr)) return 1;
	
	/* Linear search to find where the EOF marker is */
	while(*BlockPt != EOF && BlockPt < (char *) &BlockBuf[512]) ++BlockPt;
	
	if(BlockPt == (char *) &BlockBuf[512]) { // A new block needs to be appended
		// Unlink a block from the free space and link into the file
		if(Filetable_One.free.startAddr == Filetable_One.free.endAddr) return 1; // SD card is full
		
		Block_NextAddr = Filetable_One.free.startAddr;
		if(eDisk_WriteBlock((BYTE *) BlockBuf, BlockNum)) return 1;
		if(eDisk_ReadBlock((BYTE *) BlockBuf, BlockNum = DirBuf.endAddr = Block_NextAddr)) return 1;
		Filetable_One.free.startAddr = Block_NextAddr;
		Block_NextAddr = 0;
		DirBuf.size += 1;
		if(eDisk_WriteBlock((BYTE *) &Filetable_One, 0)) return 1;
		
		BlockPt = (char *) &BlockBuf[sizeof(short)];
		*BlockPt = EOF;
	}
		
	
	FILE_OPEN = FILE_WOPEN;	
	
	return 0;
}

int eFile_Write(char data) {
	if(FILE_OPEN != FILE_WOPEN) return 2; // Can only write in Write mode
	
	/* We can assume that BlockPt now points at a the EOF marker, one place beyond all written characters
	 * the first write will overwrite the EOF marker. And when file is closed the EOF marker will be place
	 * at one place beyond. Therefore, when data is equal to EOF it will mess up the file and should be forbidden. */
	if(data == EOF) return 3; // illegal input
	
	if(BlockPt == (char *) &BlockBuf[512]) { // Append a new block
		// Unlink a block from the free space and link into the file
		if(Filetable_One.free.startAddr == Filetable_One.free.endAddr) return 4; // SD card is full
		
		Block_NextAddr = Filetable_One.free.startAddr;
		if(eDisk_WriteBlock((BYTE *) BlockBuf, BlockNum)) return 1;
		if(eDisk_ReadBlock((BYTE *) BlockBuf, BlockNum = DirBuf.endAddr = Block_NextAddr)) return 1;
		Filetable_One.free.startAddr = Block_NextAddr;
		Block_NextAddr = 0;
		DirBuf.size += 1;
		if(eDisk_WriteBlock((BYTE *) &Filetable_One, 0)) return 1;
		
		BlockPt = (char *) &BlockBuf[sizeof(short)];
	}
	
	*BlockPt++ = data;
	
	return 0;
} 

int eFile_WClose(void) { // close the file for writing
	if(!FILE_WOPEN) return 2;
	
	// If the BlockPt is at the end of a file then skip the EOF marker
	// When the file is opened next time a new block should be appended
	if(BlockPt < (char *) &BlockBuf[512]) {
	  *BlockPt = EOF;	
	}
	
	// save any opened files
	// savefile may possibly overwrite BlockBuf, therefore it needs to be called last
	if(eDisk_WriteBlock((BYTE *) BlockBuf, BlockNum)) return 1;
	savefile(&DirBuf, TableNum, EntryNum);
	
	FILE_OPEN = NO_FILE_OPEN;
	
	return 0;
}

int eFile_ROpen( char name[]) {      // open a file for reading 
	if(FILE_OPEN) return 2; // cannot open multiple files
	
	if((TableNum = openfile(name, &DirBuf, &EntryNum)) == -1) {
		return 3;
	}
	
	if(eDisk_ReadBlock((BYTE *) BlockBuf, BlockNum = DirBuf.startAddr)) return 1;
	BlockPt = (char *) &BlockBuf[sizeof(short)];  // the first (short) is the next pointer
	
	FILE_OPEN = FILE_ROPEN;
	
	return 0;
}

int eFile_ReadNext(char *pt) {       // get next byte 
	if(FILE_OPEN != FILE_ROPEN) {   // Can only read in read mode
		return 2; 
	}
	
	if(*BlockPt == EOF) {
		return 3;
	}
	
	if(BlockPt == (char *) &BlockBuf[512]) {
		// Because we always open a new block for reading, the pointer is beyond the end of block
		// only we are the end of file.
			return 3; 
	}
	
	*pt = *BlockPt++;
	
	if(BlockPt == (char *) &BlockBuf[512]) {
		if(Block_NextAddr == 0) { // Reached end of file
			return 0; 
		}
		else {  // Read next block
			if(eDisk_ReadBlock((BYTE *) BlockBuf, Block_NextAddr)) return 1;
			BlockPt = (char *) &BlockBuf[sizeof(short)];
			
			if(*BlockPt == EOF) {
				return 3;
			}
		}
	}
	
	return 0;
}

int eFile_RClose(void) { // close the file for writing
	if(FILE_OPEN != FILE_ROPEN) return 1;
	
	FILE_OPEN = NO_FILE_OPEN;
	
	return 0;
}

int eFile_Directory(char *filelist){
	int i,j=0,k,l;
	char buffer[8];
	for (i=0; i<NumDirTableEntry-2; i++ ) {
		if (!Filetable_One.datablock[i].filename[0]){
			return 0;
		}
		for (k=0;k<2;k++) {
			filelist[j++] = Filetable_One.datablock[i].filename[k];
		}
		longToString(buffer, (long)(Filetable_One.datablock[i].size)*512);
		filelist[j++] = ' ';
		for (k=0; buffer[k]; k++) {
			filelist[j++] = buffer[k];
		}
		filelist[j++] = ' ';
		filelist[j++] = ' ';
		filelist[j++] = 'B';
		filelist[j++] = '\r';
		filelist[j++] = '\n';
	}
	for (l=1;l<MAXFILETABLE; l++ ) {
		DirType *DirBuffer;
		if(eDisk_WriteBlock(BlockBuf, l)) return 1;
		DirBuffer = (DirType *) (BlockBuf);
		for (i=0; i<NumDirTableEntry; i++ ) {
			if (!DirBuffer[i].filename[0]){
				return 0;
			}
			for (k=0;k<2;k++) {
				filelist[j++] = DirBuffer[i].filename[k];
			}
			longToString(buffer, (long)(Filetable_One.datablock[i].size)*512);
			filelist[j++] = ' ';
			for (k=0; buffer[k]; k++) {
				filelist[j++] = buffer[k];
			}
			filelist[j++] = ' ';
			filelist[j++] = 'B';
			filelist[j++] = '\r';
			filelist[j++] = '\n';
		}
	}
	return 0;
}   

int eFile_Delete(char name[]) { // remove this file 
	DirType dir;
	DirType repdir;
	DirType *reppt;
	char wd_at_Table, entrynum;
	char replace_dir_at_Table;
	/* 62 and 63 are skipped in the first table */
	short lastDir = (Filetable_One.firstFree==64)? 61 : Filetable_One.firstFree-1;
	
	// Trivial case there is no file to delete
	if(Filetable_One.firstFree == 0) return 2;
	
	//Close any file that is open
	switch(FILE_OPEN) {
		case FILE_WOPEN:
			eFile_WClose();
			break;
		case FILE_ROPEN:
			FILE_OPEN = NO_FILE_OPEN;
			break;
		default:
			break;
	}
	
	/* find the globally last file for replacement */  {
	
	/* Because the allocation is contiguous it can be easily found by */
		replace_dir_at_Table = lastDir / 64;
		if(replace_dir_at_Table == 0) {
			reppt = &Filetable_One.datablock[lastDir % 64];
		} else {
			if(eDisk_ReadBlock((BYTE *) BlockBuf, replace_dir_at_Table)) return 1;
			reppt = (DirType *) &BlockBuf[lastDir % 64];
		} 
		
		repdir = *reppt;
	}
	
	// find the file to delete, this will change the BlockBuf
	if((wd_at_Table = openfile(name, &dir, &entrynum)) != -1) {
	// now the information of the file is in the dir
		if(wd_at_Table == 0) {
			Filetable_One.datablock[entrynum] = repdir;
			if(eDisk_WriteBlock((BYTE *) &Filetable_One, 0)) return 1;
		} else {
			// Then the filetable should be in the BlockBuf
			((DirType *) BlockBuf)[entrynum] = repdir;
			if(eDisk_WriteBlock((BYTE *) BlockBuf, wd_at_Table)) return 1;
		}
		
		
	// Link all the data block of the file into the free space list
		if(eDisk_ReadBlock((BYTE *) BlockBuf, dir.endAddr)) return 1;
		Block_NextAddr = Filetable_One.free.startAddr;
		if(eDisk_WriteBlock((BYTE *) BlockBuf, dir.endAddr)) return 1;
		Filetable_One.free.startAddr = dir.startAddr;
		
	} else return 2; // no such file was found, at this point the last filetable is in the BlockBuf
	

	/* clear the replace file */ {
		char *name = reppt->filename;
		
		// Because the previous step changed the BlockBuf, it may be necessary to reload it
		if(replace_dir_at_Table > 0)
			if(eDisk_ReadBlock((BYTE *) BlockBuf, replace_dir_at_Table)) return 1;
		// note that even though the BlockBuf is unloaded and reloaded, reppt still points to the same address
		
		// Clear the filename, using the right associativity of = operator
		*name = *name++ = 0;

		reppt->startAddr = reppt->endAddr = 0;
		
		Filetable_One.firstFree = lastDir;
	}

	// Commit changes to the replacement and/or First table
	if(eDisk_WriteBlock((BYTE *) &Filetable_One, 0)) return 1;
	if(replace_dir_at_Table > 0)
		if(eDisk_WriteBlock((BYTE *) BlockBuf, replace_dir_at_Table)) return 1;
	
	return 0;
}

int eFile_RedirectToFile(char *name) {
  if(eFile_WOpen(name) == 0) {
		Redirect_stream = 1;
  } else return 1; 
	
  return 0;
}

int eFile_EndRedirectToFile(void) {
	// Clear redirecting flag, it wouldn't hurt if no file is open
	Redirect_stream = 0;
	
	return eFile_Close();
}

/********************************* static **********************************/
char openfile(const char *name, DirType *wd, char *entrynum) {
  DirType *d;
	char lastIndex, lastPage;

  // Trivial case: there is no file to open
  if(Filetable_One.firstFree == 0) return -1;
  
  lastIndex = Filetable_One.firstFree % 64; // one beyond
  // However, if the lastIndex is 0, then the last page is in fact empty
  lastPage = Filetable_One.firstFree / 64 - (lastIndex == 0); // last real one
  // Ensure lastIndex is one beyond
  if(lastIndex == 0) lastIndex = 64;
  
  /* look for the file in Filetable_One */ {
		char FirstPageLimit = (lastPage == 0)? lastIndex : 64; // one beyond
		
		for(d = Filetable_One.datablock; d < &Filetable_One.datablock[FirstPageLimit]; d++) {
			if(my_strcmp2(d->filename, name) == 0) {
				*entrynum = (char) (d - Filetable_One.datablock);
				*wd = *d;
				return 0;
			}
		}
		
		// this ensures the remainder part below doesn't do unnecessary work
		if(lastPage == 0) return -1;
  }
  
  /* look for the file in all the other (non-empty) filetables */ {
		char i;
		
		// Full pages if any
		for(i = 1; i < lastPage; i++) {
			if(eDisk_ReadBlock((BYTE *) BlockBuf, i)) {
				return *entrynum = -1;
			}
			
			for(d = (DirType *) BlockBuf; d < &((DirType *) BlockBuf)[64]; d++) {
				if(my_strcmp2(d->filename, name) == 0) {
					*entrynum = (char) (d - (DirType *) BlockBuf);
					*wd = *d;
					return i;
				}
			}
		}
		
		// The remainder
		// This will not repeat the work of when lastPage = 0
		if(eDisk_ReadBlock((BYTE *) BlockBuf, lastPage)) {
				return *entrynum = -1;
		}
		for(d = (DirType *) BlockBuf; d < &((DirType *) BlockBuf)[lastIndex]; d++) {
			if(my_strcmp2(d->filename, name) == 0) {
				*entrynum = (char) (d - (DirType *) BlockBuf);
				*wd = *d;
				return i;
			}
		}
	
	}
	
	// Still cannot find
	return *entrynum = -1;
}

int savefile(const DirType *wd, char wd_at_Table, char entrynum) {
	if(wd_at_Table == 0) {
		Filetable_One.datablock[entrynum] = *wd;
		if(eDisk_WriteBlock((BYTE *) &Filetable_One, 0)) return 1;
	} else {
		if(eDisk_ReadBlock((BYTE *) BlockBuf, wd_at_Table)) return 1;
		((DirType *)BlockBuf)[entrynum] = *wd;
		if(eDisk_WriteBlock((BYTE *) BlockBuf, wd_at_Table)) return 1;
	}
	
	return 0;
}

// returns 0 if equal, 1 if not
char my_strcmp2(const char *s, const char *t) {
	char result = (*s++ != *t++);
	result |= (*s != *t);
	
	return result;
}

