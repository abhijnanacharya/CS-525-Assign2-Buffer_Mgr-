#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "buffer_mgr.h"
#include "replacement_mgr_strat.h"
#include "storage_mgr.h"

int maxBufferSize = 0;
int numberOfPagesRead = 0;
int numberOfPagesWritten = 0;
int hit = 0;
int clockPointer = 0;

RC initBufferPool(BM_BufferPool *const bm, const char *const pageFileName,
                  const int numPages, ReplacementStrategy strategy,
                  void *stratData)
{
  // Initialize return code to indicate success by default
  RC returnCode = RC_OK;

  // Check if buffer pool pointer is null
  if (!bm) {
    // Initialize buffer pool properties
    bm->numPages = numPages;         // Set number of pages
    bm->strategy = strategy;         // Set replacement strategy
    bm->pageFile = (char *) pageFileName;  // Set page file name
    maxBufferSize = numPages;        // Set maximum buffer size
  }

  // Allocate dynamic memory for frames stored as an array
  Frame *frame = malloc(numPages * sizeof(Frame));

  // Initialize each frame in the buffer pool
  int currVal = 0;
  while (currVal < maxBufferSize) {
    // Initialize frame properties
    frame[currVal].bm_PageHandle.data = NULL; // Set data pointer to null
    frame[currVal].bm_PageHandle.pageNum = -1; // Set page number to -1 (invalid)
    frame[currVal].dirtyCount = 0;   // Initialize dirty count to 0
    frame[currVal].fixCount = 0;     // Initialize fix count to 0
    frame[currVal].hit = 0;          // Initialize hit count to 0

    // Move to the next frame
    currVal++;
  }

  // Set buffer pool management data to point to the allocated frames
  bm->mgmtData = frame;

  // Return the initialized buffer pool status
  return returnCode;
}


// Buffer Manager Interface Access Pages
RC markDirty(BM_BufferPool *const bm, BM_PageHandle *const page) {
  // Checking if buffer manager, page file, or page handle pointers are NULL.
    if (bm == NULL || bm->pageFile == NULL || page == NULL) 
        return RC_ERROR;
    
    buffer *b = (buffer*)bm->mgmtData;
    if (b->head == NULL)
        return RC_READ_NON_EXISTING_PAGE; // page not found

    Frame *frame = b->head;
    do {
        if (frame->currentPage == page->pageNum) {
            frame->isDirty = true; // Mark the frame as dirty since the page has been modified
            return RC_OK; 
        }
        frame = frame->nextFrame;
    } while (frame != b->head);

    return RC_READ_NON_EXISTING_PAGE; // specified page was not found
}


RC unpinPage (BM_BufferPool *const bm, BM_PageHandle *const page){
    if (bm == NULL || bm->pageFile == NULL || page == NULL) 
    return RC_ERROR;
    buffer *b = (buffer*)bm->mgmtData;
    Frame *frame = b->head;
    while (frame->currentPage!=page->pageNum){
        frame=frame->nextFrame;
        if (frame==b->head)
            return RC_READ_NON_EXISTING_PAGE;
    }
    if (frame->fixCount > 0){
        frame->fixCount--;
        if (frame->fixCount == 0)
            frame->referenceBit = false;
    }
    else
        return RC_READ_NON_EXISTING_PAGE;

    return RC_OK;
}

RC forcePage (BM_BufferPool *const bm, BM_PageHandle *const page){
    if (bm == NULL || bm->pageFile == NULL || page == NULL) 
    return RC_ERROR;
    buffer *b = (buffer*)bm->mgmtData;
    SM_FileHandle fHandle;
    RC stat;
    // Attempt to open the page file
    stat = openPageFile(bm->pageFile, &fHandle);
    if (stat!=RC_OK) return RC_FILE_NOT_FOUND;
    // Write the page data to disk
    stat = writeBlock(page->pageNum, &fHandle, page->data);
    if (stat!=RC_OK){
        closePageFile(&fHandle);
        return RC_FILE_NOT_FOUND;
    }
    
    b->writeCount++;
    closePageFile(&fHandle); // Close the file handle after successful write
    return RC_OK;
}

