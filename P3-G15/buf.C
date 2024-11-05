#include <memory.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <iostream>
#include <stdio.h>
#include "page.h"
#include "buf.h"

#define ASSERT(c)  { if (!(c)) { \
		       cerr << "At line " << __LINE__ << ":" << endl << "  "; \
                       cerr << "This condition should hold: " #c << endl; \
                       exit(1); \
		     } \
                   }

//----------------------------------------
// Constructor of the class BufMgr
//----------------------------------------

BufMgr::BufMgr(const int bufs)
{
    numBufs = bufs;

    bufTable = new BufDesc[bufs];
    memset(bufTable, 0, bufs * sizeof(BufDesc));
    for (int i = 0; i < bufs; i++) 
    {
        bufTable[i].frameNo = i;
        bufTable[i].valid = false;
    }

    bufPool = new Page[bufs];
    memset(bufPool, 0, bufs * sizeof(Page));

    int htsize = ((((int) (bufs * 1.2))*2)/2)+1;
    hashTable = new BufHashTbl (htsize);  // allocate the buffer hash table

    clockHand = bufs - 1;
}


BufMgr::~BufMgr() {

    // flush out all unwritten pages
    for (int i = 0; i < numBufs; i++) 
    {
        BufDesc* tmpbuf = &bufTable[i];
        if (tmpbuf->valid == true && tmpbuf->dirty == true) {

#ifdef DEBUGBUF
            cout << "flushing page " << tmpbuf->pageNo
                 << " from frame " << i << endl;
#endif

            tmpbuf->file->writePage(tmpbuf->pageNo, &(bufPool[i]));
        }
    }

    delete [] bufTable;
    delete [] bufPool;
}


const Status BufMgr::allocBuf(int & frame) 
{
    // Initialize the status as OK and set up tracking for the clock search
    Status status = OK;
    int numScanned = 0;
    bool found = false;

    // Loop to search for an available buffer frame, using the clock algorithm
    while (numScanned < 2 * numBufs)
    {
        // Move the clock hand forward
        advanceClock();
        numScanned++;

        // Check if the current frame is invalid; if so, it can be used directly
        if (!bufTable[clockHand].valid)
        {
            break;
        }

        // If the frame is valid, check the reference bit
        if (!bufTable[clockHand].refbit)
        {
            // If not pinned, it’s safe to use this frame
            if (bufTable[clockHand].pinCnt == 0)
            {
                // Remove the current entry from the hash table
                status = hashTable->remove(bufTable[clockHand].file, bufTable[clockHand].pageNo);
                found = true;
                break;
            }
        }
        else
        {
            // If referenced, reset the reference bit
            bufStats.accesses++;
            bufTable[clockHand].refbit = false;
        }
    }
    
    // If no suitable frame is found after scanning the buffer pool, return BUFFEREXCEEDED
    if (!found && numScanned >= 2 * numBufs)
    {
        return BUFFEREXCEEDED;
    }
    
    // If the selected frame has unsaved changes, write them to disk
    if (bufTable[clockHand].dirty)
    {
        bufStats.diskwrites++;
        status = bufTable[clockHand].file->writePage(bufTable[clockHand].pageNo, &bufPool[clockHand]);
        if (status != OK) return status;
    }

    // Set the output frame to the current clock hand position
    frame = clockHand;

    return OK;
}

	
const Status BufMgr::readPage(File* file, const int PageNo, Page*& page)
{
    // Attempt to find the page in the buffer pool
    int frameNo = 0;
    Status status = hashTable->lookup(file, PageNo, frameNo);
    if (status == OK)
    {
        // If found, update the reference bit and pin count
        bufTable[frameNo].refbit = true;
        bufTable[frameNo].pinCnt++;
        page = &bufPool[frameNo];
    }
    else // If not in the buffer pool, allocate a new frame
    {
        // Request a new buffer frame
        status = allocBuf(frameNo);
        if (status != OK) return status;

        // Load the requested page into the newly allocated frame
        bufStats.diskreads++;
        status = file->readPage(PageNo, &bufPool[frameNo]);
        if (status != OK) return status;

        // Set up buffer table entry for this frame
        bufTable[frameNo].Set(file, PageNo);
        page = &bufPool[frameNo];

        // Add the page to the hash table
        status = hashTable->insert(file, PageNo, frameNo);
        if (status != OK) return status;
    }

    return OK;
}



const Status BufMgr::unPinPage(File* file, const int PageNo, const bool dirty)
{
    // Search for the page in the hash table
    Status status = OK;
    int frameNo = 0;
    status = hashTable->lookup(file, PageNo, frameNo);
    if (status != OK) return status;

    // If the page is marked as dirty, update its dirty status in the buffer table
    if (dirty) bufTable[frameNo].dirty = true;

    // Ensure the page is currently pinned; if not, return an error
    if (bufTable[frameNo].pinCnt == 0)
    {
        return PAGENOTPINNED;
    }
    else
    {
        // Decrement the pin count, effectively unpinning the page
        bufTable[frameNo].pinCnt--;
    }

    return OK;
}


const Status BufMgr::allocPage(File* file, int& pageNo, Page*& page)
{
    int frameNo;

    // Request a new page within the specified file
    Status status = file->allocatePage(pageNo);
    if (status != OK) return status;

    // Allocate a buffer frame for the new page
    status = allocBuf(frameNo);
    if (status != OK) return status;

    // Initialize the buffer table entry for this frame
    bufTable[frameNo].Set(file, pageNo);
    page = &bufPool[frameNo];

    // Insert the page and frame information into the hash table
    status = hashTable->insert(file, pageNo, frameNo);
    if (status != OK) return status;

    return OK;
}


const Status BufMgr::disposePage(File* file, const int pageNo) 
{
    // see if it is in the buffer pool
    Status status = OK;
    int frameNo = 0;
    status = hashTable->lookup(file, pageNo, frameNo);
    if (status == OK)
    {
        // clear the page
        bufTable[frameNo].Clear();
    }
    status = hashTable->remove(file, pageNo);

    // deallocate it in the file
    return file->disposePage(pageNo);
}

const Status BufMgr::flushFile(const File* file) 
{
  Status status;

  for (int i = 0; i < numBufs; i++) {
    BufDesc* tmpbuf = &(bufTable[i]);
    if (tmpbuf->valid == true && tmpbuf->file == file) {

      if (tmpbuf->pinCnt > 0)
	  return PAGEPINNED;

      if (tmpbuf->dirty == true) {
#ifdef DEBUGBUF
	cout << "flushing page " << tmpbuf->pageNo
             << " from frame " << i << endl;
#endif
	if ((status = tmpbuf->file->writePage(tmpbuf->pageNo,
					      &(bufPool[i]))) != OK)
	  return status;

	tmpbuf->dirty = false;
      }

      hashTable->remove(file,tmpbuf->pageNo);

      tmpbuf->file = NULL;
      tmpbuf->pageNo = -1;
      tmpbuf->valid = false;
    }

    else if (tmpbuf->valid == false && tmpbuf->file == file)
      return BADBUFFER;
  }
  
  return OK;
}


void BufMgr::printSelf(void) 
{
    BufDesc* tmpbuf;
  
    cout << endl << "Print buffer...\n";
    for (int i=0; i<numBufs; i++) {
        tmpbuf = &(bufTable[i]);
        cout << i << "\t" << (char*)(&bufPool[i]) 
             << "\tpinCnt: " << tmpbuf->pinCnt;
    
        if (tmpbuf->valid == true)
            cout << "\tvalid\n";
        cout << endl;
    };
}


