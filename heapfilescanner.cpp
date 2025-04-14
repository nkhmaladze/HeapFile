#include <string>
#include <vector>
#include <mutex>

#include "swatdb_types.h"
#include "swatdb_exceptions.h"
#include "file.h"
#include "bufmgr.h"
#include "heapfile.h"
#include "heapfilescanner.h"
#include "heappagescanner.h"
#include "heappage.h"
#include "page.h"


/**
 * Scanner class for HeapFile. Scanner scans the given HeapFile object and
 * returns the next RecordId whenever getNext is called. Returns
 * INVALID_RECORD_ID if it reaches the end of the file.
 */

/**
 * @brief Constructor.
 *
 * @pre Valid HeapFile* is provided as input.
 * @post HeapFile object is constructed with initialized data members. The
 *    cur_page is pinned if file is not empty.
 *
 * @param file HeapFile object to be scanned.
 */
HeapFileScanner::HeapFileScanner(HeapFile* file) {
  this->cur_page = nullptr;
  this->file = file;
  this->buf_mgr = file->buf_mgr;
  this->end_of_full = false;
  this->end_of_free = false;
  this->scanner = new HeapPageScanner(nullptr);
  this->cur_pid = INVALID_PAGE_ID;

  Page* header_page = ( file->buf_mgr )->getPage( file->header_id );
  HeapFileHeader* file_header = ( HeapFileHeader* )( header_page->getData() );
  PageId page_id;

  //initialize cur_pid to first page_id in full list
  page_id.file_id = file->file_id;
  page_id.page_num = file_header->full;
  this->cur_pid = page_id;

  //initialize cur_page to first page in full list
  Page* cur_page;
  cur_page = ( file->buf_mgr )->getPage( this->cur_pid );
  this->cur_page = (HeapPage*)cur_page;

  this->scanner = new HeapPageScanner(this->cur_page);

  /*
  while(file_header->full && file_header->free != INVALID_PAGE_NUM){
    PageId page_id;
    
    //First time scan full list
    if(this->end_of_full == false){
      PageNum full_head = file_header->full;
      page_id.file_id = this->file_id;
      page_id.page_num = full_head;
      Page* data_page = this->buf_mgr->getPage( page_id );
      this->cur_page = (HeapPage*)( data_page->getData() );
      this->scanner = new HeapPageScanner(this->cur_page);
    }

    //Second time scan free list
    else{
      PageNum free_head = file_header->free;
      page_id.file_id = this->file_id;
      page_id.page_num = free_head;
      Page* data_page = this->buf_mgr->getPage( page_id );
      this->cur_page = (HeapPage*)( data_page->getData() );
      this->scanner = new HeapPageScanner(this->cur_page);
    }
  }
  */
}

/**
 * @brief Destructor.
 *
 * @pre If the end of the File has not been reached, cur_page is pinned.
 * @post If the end of the File has not been reached, cur_page is released.
 */
HeapFileScanner::~HeapFileScanner(){
  delete this->scanner;
  if (!(this->end_of_full && this->end_of_free)) {
    (this->buf_mgr)->releasePage(this->cur_pid, false);
  }
}

/**
 * @brief Returns RecordId of the next Record in the HeapFile and
 *    initializes the given Record object to the data of the identified
 *    Record. Scans for records by iterating through the linked list
 *    of full pages, then the linked list of free pages.
 *
 * @pre None
 * @post RecordId of the next Record is returned and data of the given
 *    Record object is initialized to that of the Record identified by
 *    the RecordId. If there are no more records in the HeapFile,
 *    INVALID_RECORD_ID is returned. cur_page is the Page that is being
 *    scanned and is pinned. Once the end of cur_page is reached, it is
 *    the next Page in the File is pinned (if there is next Page) and
 *    cur_page is unpinned. cur_pid and cur_page are updated accordingly.
 *    If the end of the File is reached, no Page is pinned by the
 *    HeapFileScanner.
 *
 * @return Next valid RecordId. INVALID_RECORD_ID if the end of the file is
 *    reached.
 */
RecordId HeapFileScanner::getNext(Record* record) {
  SlotId slot_id;
  RecordId ret_rec_id = INVALID_RECORD_ID;
  PageNum pnxt;
  PageId page_id;
  Page* header_page = ( file->buf_mgr )->getPage( file->header_id );
  HeapFileHeader* file_header = ( HeapFileHeader* )( header_page->getData() );

  slot_id = this->scanner->getNext();

  //Case for when all records on current page have been scanned
  if (slot_id != INVALID_SLOT_ID){
    ret_rec_id.slot_id = slot_id;
    ret_rec_id.page_num = this->cur_pid.page_num;
    return ret_rec_id;
  }

  //case for when there's still records on current page to be scanned
  pnxt = this->cur_page->getNext();
  page_id = this->cur_pid;
  this->buf_mgr->releasePage( page_id, false);

  //If we reach end of full list
  //find first page on free list
  if((pnxt == INVALID_PAGE_NUM) && (this->end_of_full = false)){
    this->end_of_full = true;
    PageNum free_head = file_header->free;
    page_id.file_id = file->file_id;
    page_id.page_num = free_head;
    Page* data_page = this->buf_mgr->getPage( page_id );
    this->cur_page = (HeapPage*)( data_page->getData() );
    this->scanner = new HeapPageScanner(this->cur_page);
  }

  //If we reach end of free list
  //scan is complete
  if((file_header->free_size == 0) || (pnxt = INVALID_PAGE_NUM)){
    this->end_of_free = true;
    return INVALID_RECORD_ID;
  }

  //release header page
  this->buf_mgr->releasePage(file->header_id, false);

  //update cur_page and cur_pid
  page_id.file_id = file->file_id;
  page_id.page_num = INVALID_PAGE_NUM;
  this->cur_pid = page_id;
  Page* cur_page;
  cur_page = ( file->buf_mgr )->getPage( this->cur_pid );
  this->cur_page = (HeapPage*)cur_page;

  ret_rec_id.slot_id = slot_id;
  ret_rec_id.page_num = INVALID_PAGE_NUM;
  this->scanner = new HeapPageScanner(this->cur_page);

  return ret_rec_id;
}