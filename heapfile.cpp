#include <string>
#include <vector>
#include <mutex>
#include <cstring>

#include "swatdb_types.h"
#include "swatdb_exceptions.h"
#include "filemgr.h"
#include "heapfile.h"
#include "heappage.h"
#include "bufmgr.h"
#include "catalog.h"
#include "schema.h"
#include "page.h"
#include "data.h"
#include "record.h"
#include "heappagescanner.h"
#include "swatdb_types.h"

const std::size_t MAXIMUM_RECORD_SIZE = 4096;
//static const std::uint32_t MAX_PAGE_NUM = 65536;


/**
 * SwatDB HeapFile Class.
 * Represents heap file in the system. It consists of doubly linked list
 * of HeapPage and is an unsorted collection of records. Provides various
 * methods, including inserting, modifying and retrieving records.
 */
/**
 * @brief Constructor for HeapFile class.
 *
 * @pre A valid Catalog pointer and BufferManager pointer are provided
 *    as inputs.
 * @post HeapFile object is constructed. Catalog and BufferManager data
 *    members are set to the provided inputs. Other values are initialized
 *    after construction.
 *
 * @param catalog Pointer to the SwatDB Catalog object.
 * @param buf_mgr Pointer to the SwatDB BufferManager object.
 * @param schema Pointer to this file's Schema.
 */
HeapFile::HeapFile(Catalog *catalog, BufferManager *buf_mgr,
    Schema *schema): RelationFile(catalog, buf_mgr, schema)
{
  this->header_id = INVALID_PAGE_ID;
}


/**
 * @brief HeapFile destructor. Deletes the mutex in the header
 *
 * @pre None.
 * @post The HeapFile object is destroyed. The file on disk is not removed.
 */
HeapFile::~HeapFile() {
}

/**
 * @brief Allocates and initializes the header Page of the file.
 *
 * @pre There is sufficient space for a Page in buffer pool. The file
 *    with the corresponding FileId is already created.
 * @post A Page is allocated and the header_id field is initialized to the
 *    PageId of the allcated Page. The free and full fields are initialized
 *    to INVALID_PAGE_NUM and free_size and full_size are initialized to 0.
 *
 * @throw InsufficientSpaceBufMgr If there is not enough space in the
 *    bufferpool.
 */
void HeapFile::createHeader(){
  std::pair<Page*, PageId> temp;
  HeapFileHeader* file_header;

  temp = (this->buf_mgr)->allocatePage(this->file_id);
  this->header_id = temp.second;
  file_header = (HeapFileHeader*) temp.first->getData();
  file_header->full = INVALID_PAGE_NUM;
  file_header->free = INVALID_PAGE_NUM;
  file_header->full_size = 0;
  file_header->free_size = 0;
  file_header->num_records = 0;
  (this->buf_mgr)->releasePage(this->header_id, true);

}

/**
 * @brief Flushes header Page to disk.
 *
 * @pre There is sufficient space for the header Page in buffer pool.
 *    header_id is valid.
 * @post Header Page of the File is flushed to disk.
 *
 * @throw InsufficientSpaceBufMgr If there is not enough space in the
 *    bufferpool for the header Page.
 * @throw InvalidPageIdBufMgr If the header_id of the File is not valid.
 */
void HeapFile::flushHeader(){
  (this->buf_mgr)->getPage(this->header_id);
  (this->buf_mgr)->flushPage(this->header_id);
  (this->buf_mgr)->releasePage(this->header_id, false);
}

/**
 * @brief Returns the number of pages in the file.
 */
std::uint32_t HeapFile::getNumPages(){
  //get the header page
  HeapFileHeader *file_header = (HeapFileHeader*)
      ((this->buf_mgr)->getPage(this->header_id))->getData();
  std::uint32_t num_pages = file_header->free_size + file_header->full_size;
  (this->buf_mgr)->releasePage(this->header_id, false);

  return num_pages;
}

/**
 * @brief Returns the number of full pages in the file.
 */
std::uint32_t HeapFile::getNumFullPages(){
  //get the header page
  HeapFileHeader *file_header = (HeapFileHeader*)
      ((this->buf_mgr)->getPage(this->header_id))->getData();
  std::uint32_t num_pages = file_header->full_size;
  (this->buf_mgr)->releasePage(this->header_id, false);

  return num_pages;
}


/**
 * @brief Returns the number of records in the file.
 */
std::uint64_t HeapFile::getNumRecords(){

  std::uint32_t num_records = 0;
  //get the header page
  HeapFileHeader *file_header = (HeapFileHeader*)
      ((this->buf_mgr)->getPage(this->header_id))->getData();
  num_records = file_header->num_records; 
  //release the header page
  (this->buf_mgr)->releasePage(this->header_id, false);

  return num_records;
}

/**
 * @brief Inserts a Record into the HeapFile.
 *
 * @pre A valid Record object with a Schema matching that of the HeapFile
 *    is provided as input. There is some Page into which the Record can be
 *    inserted.
 * @post The Record is inserted into some Page that belongs to the HeapFile.
 *    If there is enough space on some Page in the list of free pages, then
 *    the Record is inserted there. If there is not enough space on any
 *    Page in the list of free pages, then a new Page is allocated. The
 *    Record is inserted into this Page. If the the Page is full after
 *    inserting the Record, the Page is moved to the list of full pages.
 *    Otherwise, the Page is added to/remains in free list. The header Page
 *    is updated appropriately. All pages pinned during the operation are
 *    released at the end or before exeption is thrown.
 *
 * @param record Record to be inserted into the HeapFile.
 * @return RecordId RecordId of the inserted Record.
 *
 * @throw InvalidSchemaHeapFile If the given Record's Schema does not match
 *    that of the HeapFile (compares pointers).
 * @throw InsufficientSpaceHeapPage If the given Record's data exceeds the
 *    MAXIMUM_RECORD_SIZE.
 * @throw InsufficientSpaceHeapFile If the number of pages (including the
 *    header) in the HeapFile exceeds MAX_PAGE_NUM.
 */
RecordId HeapFile::insertRecord(Record record){
    // Pin the header page
    Page* header_page = ( this->buf_mgr )->getPage( this->header_id );
    HeapFileHeader* file_header = ( HeapFileHeader* )( header_page->getData() );

    Schema* schema_type = record.getSchema();
    
    // Check schema, unpin and throw error if not matched
    if( schema_type != this->schema ){
        ( this->buf_mgr )->releasePage( this->header_id, false );
        throw InvalidSchemaHeapFile();
    }

    // Check record size not too big
    std::size_t record_size = record.getRecordData()->getSize();
    if( record_size > MAX_RECORD_SIZE ){
        this->buf_mgr->releasePage( this->header_id, false );
        throw InsufficientSpaceHeapPage();
    }


    // Check the total number of pages (header + free + full)
    std::uint32_t total_pages = file_header->free_size + file_header->full_size + 1; 
    if( total_pages >= MAX_PAGE_NUM ){
        this->buf_mgr->releasePage( this->header_id, false );
        throw InsufficientSpaceHeapFile();
    }


    // This will hold the final page we use:
    PageId target_pid = INVALID_PAGE_ID;
    Page* data_page = nullptr;
    HeapPage* hp = nullptr;

    // Start with the head of the free list
    PageNum free_head = file_header->free;
    PageNum prev_free = INVALID_PAGE_NUM; // If we need to traverse multiple free pages

    bool foundSpace = false;
    // Try to insert in free page where enough space 
    while( free_head != INVALID_PAGE_NUM ){
        // Pin that page
        PageId smthn;
        smthn.file_id = this->file_id;
        smthn.page_num = free_head;
        data_page = this->buf_mgr->getPage( smthn );
        hp = (HeapPage*)( data_page->getData() );

        // If it has enough space, we’ll attempt to insert
        if( hp->getFreeSpace() >= record.getRecordData()->getSize() ){
            target_pid = smthn;
            foundSpace = true;
            break;
        }

        // Not enough space? Move to the next in the free list
        PageNum nxt = hp->getNext();

        // Unpin the old page, we no longer need it pinned
        this->buf_mgr->releasePage( smthn, false );

        // Advance
        prev_free = free_head;
        free_head = nxt;
    }

    // If still not found, allocate a new page
    if( !foundSpace ){

        std::pair<Page*, PageId> new_page = (this->buf_mgr)->allocatePage( this->file_id );
        data_page = new_page.first;
        target_pid = new_page.second;
        hp = (HeapPage*)( data_page->getData() );

        // Initialize the newly allocated page to be a HeapPage
        hp->initializeHeader(); 

        // Insert this page at the head of the free list
        hp->setPrev( INVALID_PAGE_NUM );
        hp->setNext( file_header->free );

        // If old free list head wasn’t INVALID_PAGE_NUM, pin it and fix its prev_page
        if( file_header->free != INVALID_PAGE_NUM ){
            PageId smthn1;
            smthn1.file_id = this->file_id;
            smthn1.page_num = file_header->free;

            Page* old_head_p = this->buf_mgr->getPage( smthn1 );
            HeapPage* old_head_hp = (HeapPage*)( old_head_p->getData() );
            old_head_hp->setPrev( target_pid.page_num );
            this->buf_mgr->releasePage( smthn1, true );
        }

        // The new page = new head
        file_header->free = target_pid.page_num;
        file_header->free_size++;
    }

    // Insert the record
    Data* smthn2 = record.getRecordData();
    SlotId slot_id = hp->insertRecord( smthn2 );

    // Build the RecordId
    RecordId rid;
    rid.page_num = target_pid.page_num;
    rid.slot_id = slot_id;

    // If the page is now considered full, move it from free to full
    if( hp->isFull() ){
      this->freeToFull( hp, target_pid );
    }

    // Bump total record count
    file_header->num_records++;

    //  Unpin the data page mark dirty
    this->buf_mgr->releasePage(target_pid, true);

    // Unpin header, also dirty
    this->buf_mgr->releasePage(this->header_id, true);

    return rid;
}



/**
 * @brief Sets the record_data of the given Record pointer to the data
 *    corresponding to the given RecordId.
 *
 * @pre A valid RecordId and a Record object pointer with a Schema matching
 *    that of the HeapFile.
 * @post The given Record pointer's data field is initialized to the data
 *    identified by the given RecordId. All pages pinned during the
 *    operation are released at the end or before exeption is thrown.
 *
 * @param record_id RecordId of the record_data to be retrieved.
 * @param record Record pointer to store the retrieved data.
 *
 * @throw InvalidSchemaHeapFile If the given Record pointer's Schema does
 *    not match that of the HeapFile (compares pointers).
 * @throw InvalidPageIdBufMgr If the PageNum of the given RecordId is not
 *    valid.
 * @throw InvalidSlotIdHeapPage If the SlotId of the given RecordId is not
 *    valid.
 */
void HeapFile::getRecord(RecordId record_id, Record* record){

    if( record->getSchema() != this->schema ){
        throw InvalidSchemaHeapFile();
    }

    PageId pid;
    pid.file_id = this->file_id;
    pid.page_num = record_id.page_num;
    Page* data_page = this->buf_mgr->getPage( pid );

    HeapPage* hp = (HeapPage*)( data_page->getData() );

    //Data* heap_data = new Data(MAX_RECORD_SIZE);
    Data* heap_data = record->getRecordData();
  


    // Retrieve the record, throws other two errors itself
    //hp->getRecord( record_id.slot_id, heap_data );
    hp->getRecord( record_id.slot_id, heap_data );


    // copy retrieved data into the caller’s Record
    //record->setRecordData( heap_data );
    
    // Unpin page
    this->buf_mgr->releasePage( pid, false );
}

/**
 * @brief Updates the record data in the HeapFile identified by the given
 *    RecordId to the data in the provided Record.
 *
 * @pre A valid RecordId and a Record object with a Schema matching that of
 *    the HeapFile are provided as inputs. There is enough space in the
 *    HeapPage containing the Record identified by RecordId for the data
 *    in the provided record.
 * @post The Record data of the Record identified by the RecordId is
 *    replaced with the data of the provided Record. If the "full" state
 *    of the apge changes after the update, it is moved from one list to
 *    the other (free to full or full to free). The header Page is updated
 *    appropriately. All pages pinned during the operation are released
 *    at the end or before exeption is thrown.
 *
 * @param record_id RecordId identifying the record data to be updated.
 * @param record Record object containing data to overwrite the record data
 *    identified by the given RecordId.
 *
 * @throw InvalidSchemaHeapFile If the given Record's Schema does
 *    not match that of the HeapFile (compares pointers).
 * @throw InvalidPageIdBufMgr if the PageNum of the given RecordId is not
 *    valid.
 * @throw InvalidSlotIdHeapPage if the SlotId of the given RecordId is not
 *    valid.
 * @throw InsufficientSpaceHeapPage if there is not enough space for the
 *    updated record in the corresponding HeapPage.
 */
void HeapFile::updateRecord(RecordId record_id, Record record){
  if( record.getSchema() != this->schema ){
    throw InvalidSchemaHeapFile();
  }

  PageId pid;
  pid.file_id = this->file_id;
  pid.page_num = record_id.page_num;
  Page* data_page = this->buf_mgr->getPage( pid );
  HeapPage* hp = (HeapPage*)( data_page->getData() );

  bool beforeIsFull = hp->isFull();

  Data* heap_data = record.getRecordData();

  try{
    hp->updateRecord( record_id.slot_id, heap_data );
  } catch( InsufficientSpaceHeapPage & e ){
    //not enough space = unpin
    (this->buf_mgr)->releasePage( pid, false );
    throw; //rethrow InsufficientSpaceHeapPage
  }
  
  //hp->updateRecord(record_id.slot_id, heap_data);
  bool afterIsFull = hp->isFull();

  //must move from full to free
  if(beforeIsFull && !afterIsFull){
    this->fullToFree(hp, pid);
  }

  //must move from free to fell
  else if(!beforeIsFull && afterIsFull){
    this->freeToFull(hp, pid);
  }

  (this->buf_mgr)->releasePage(pid, true); //check whether dirty really
}

/**
 * @brief Deletes the Record identified by the given RecordId.
 *
 * @pre A valid RecordId is provided.
 * @post Deletes the Record identified by the given RecordId. If the Page
 *    was in the list of full pages, and it is no longer full after
 *    deletion, then the Page is moved to the list of free pages.
 *    If the Page is empy after deletion, it is completely removed from
 *    any list, released, and deallocated. The header Page is updated
 *    appropriately. All pages pinned during the operation are released at
 *    the end or before exception is thrown.
 *
 * @param record_id RecordId identifying the record data to be deleted.
 *
 * @throw InvalidSchemaHeapFile If the given Record's Schema does
 *    not match that of the HeapFile(compare pointers).
 * @throw InvalidPageIdBufMgr If the PageNum of the given RecordId is not
 *    valid.
 * @throw InvalidSlotIdHeapPage If the SlotId of the given RecordId is not
 *    valid.
 */
void HeapFile::deleteRecord(RecordId record_id) {
   
   Page* header_page = (this->buf_mgr)->getPage( this->header_id );
   HeapFileHeader* file_header = (HeapFileHeader*) header_page->getData();

   PageId pid;
   pid.file_id = this->file_id;
   pid.page_num = record_id.page_num;    
   Page* data_page = (this->buf_mgr)->getPage( pid );
   HeapPage* hp = (HeapPage*) data_page->getData();

   //check if page was previously full
   bool was_full = hp->isFull();

   //delete record from page, throws InvalidSlotIdHeapPage if necessary
   hp->deleteRecord( record_id.slot_id );

   //decrement total record count in the file
   file_header->num_records--;

   //if page now empty, remove it from the applicable list and deallocate
   if( hp->getNumRecs() == 0 ){
       PageNum pprev = hp->getPrev();
       PageNum pnxt = hp->getNext();
       
       //fix next pointer of the previous page
       if( pprev != INVALID_PAGE_NUM ){
           PageId pprev_id;
           pprev_id.file_id = this->file_id;
           pprev_id.page_num = pprev;

           Page* pprev_page = (this->buf_mgr)->getPage( pprev_id );
           HeapPage* pprev_hp = (HeapPage*) pprev_page->getData();
           pprev_hp->setNext( pnxt );
           (this->buf_mgr)->releasePage( pprev_id, true );
       }
       //fix prev pointer of the next page
       if( pnxt != INVALID_PAGE_NUM ){
           PageId pnxt_id;
           pnxt_id.file_id = this->file_id;
           pnxt_id.page_num = pnxt;

           Page* pnxt_page = (this->buf_mgr)->getPage( pnxt_id );
           HeapPage* pnxt_hp = (HeapPage*) pnxt_page->getData();
           pnxt_hp->setPrev( pprev );
           (this->buf_mgr)->releasePage( pnxt_id, true );
       }
      
       //if page was the head of the free list
       if( file_header->free == record_id.page_num ){
        file_header->free = pnxt;
        file_header->free_size--;
    }
    //else if page was the head of the full list
    else if( file_header->full == record_id.page_num ){
        file_header->full = pnxt;
        file_header->full_size--;
    } 
    //if in middle
    else{ 
         if( was_full ){
           file_header->full_size--;
         } else {
           file_header->free_size--;
          }
    }
       //mark dirty, unpin it and deallocate
       (this->buf_mgr)->releasePage( pid, true );
       (this->buf_mgr)->deallocatePage( pid );
   }
   else{
       //the page is not empty; check if used to be full
       bool is_full = hp->isFull();
       if( was_full && !is_full ){
           //move from full to free list
           this->fullToFree( hp, pid );
       }

       //unpin the data page and mark it dirty
       (this->buf_mgr)->releasePage( pid, true );
   }

   //unpin the header page, marking it dirty since we updated num_records 
   (this->buf_mgr)->releasePage( this->header_id, true );
}

/**
 * @brief THIS METHOD IS FOR DEBUGGING ONLY.
 *    Returns the current HeapFileHeader.
 */
HeapFileHeader HeapFile::getHeader(){
  HeapFileHeader* header_ptr = (HeapFileHeader*)
      (this->buf_mgr)->getPage(this->header_id);
  HeapFileHeader header = *header_ptr;
  (this->buf_mgr)->releasePage(this->header_id, false);
  return header;
}

/**
 * @brief This method returns the number of records in a file.
 */
std::uint64_t HeapFile::getNumRecs(){

  HeapFileHeader* file_header;
  std::uint64_t num = 0;
  
  //get the header page
  file_header = (HeapFileHeader*)
      ((this->buf_mgr)->getPage(this->header_id))->getData();
  num = file_header->num_records;
  (this->buf_mgr)->releasePage(this->header_id, false);

  return num;
}

/*************************************/
 /**
     * @brief Moves from free list to full list
     *
     * @pre A pointer to a HeapPage is provided
     * @post page is moved from free list to full list
     *
     * @param hp a HeapPage pointer to the page 
     */
    void HeapFile::freeToFull(HeapPage* hp, PageId target_pid){
      Page* header_page = ( this->buf_mgr )->getPage( this->header_id );
      HeapFileHeader* file_header = ( HeapFileHeader* )( header_page->getData() );

      // remove from free list
      PageNum pprev = hp->getPrev();
      PageNum pnxt = hp->getNext();

      // If this page was the free list head
      if( file_header->free == target_pid.page_num ){
          file_header->free = pnxt;
      }

      // Fix pprev->next_page = pnxt
      if( pprev != INVALID_PAGE_NUM ){
          PageId smthn3;
          smthn3.file_id = this->file_id;
          smthn3.page_num = pprev;

          Page* prev_page = this->buf_mgr->getPage( smthn3 );
          HeapPage* prev_hp = (HeapPage*)( prev_page->getData() );
          prev_hp->setNext( pnxt );
          this->buf_mgr->releasePage( smthn3, true );
      }
      // Fix pnxt->prev_page = pprev
      if( pnxt != INVALID_PAGE_NUM ){
          PageId smthn4;
          smthn4.file_id = this->file_id;
          smthn4.page_num = pnxt;

          Page* next_page = this->buf_mgr->getPage( smthn4 );
          HeapPage* next_hp = (HeapPage*)( next_page->getData() );
          next_hp->setPrev( pprev );
          this->buf_mgr->releasePage( smthn4, true);
      }

      file_header->free_size--;

      // add to head of full list
      hp->setPrev( INVALID_PAGE_NUM );
      hp->setNext( file_header->full );
      
      if (file_header->full != INVALID_PAGE_NUM) {
          // fix the old head’s prev
          PageId smthn5;
          smthn5.file_id = this->file_id;
          smthn5.page_num = file_header->full;

          Page* old_free_p = this->buf_mgr->getPage( smthn5 );
          HeapPage* old_free_hp = (HeapPage*)( old_free_p->getData() );
          old_free_hp->setPrev( target_pid.page_num );
          this->buf_mgr->releasePage( smthn5, true );
      }
      file_header->full = target_pid.page_num;
      file_header->full_size++;

      (this->buf_mgr)->releasePage(this->header_id, false);
    }

   /**
     * @brief Moves from full list to free list
     *
     * @pre A pointer to a HeapPage is provided
     * @post 
     *
     * @param hp a HeapPage pointer to the page 
     */
    void HeapFile::fullToFree(HeapPage* hp, PageId target_pid){
      Page* header_page = ( this->buf_mgr )->getPage( this->header_id );
      HeapFileHeader* file_header = ( HeapFileHeader* )( header_page->getData() );

      // remove from free list
      PageNum pprev = hp->getPrev();
      PageNum pnxt = hp->getNext();

      // If this page was the free list head
      if( file_header->full == target_pid.page_num ){
          file_header->full = pnxt;
      }

      // Fix pprev->next_page = pnxt
      if( pprev != INVALID_PAGE_NUM ){
          PageId smthn3;
          smthn3.file_id = this->file_id;
          smthn3.page_num = pprev;

          Page* prev_page = this->buf_mgr->getPage( smthn3 );
          HeapPage* prev_hp = (HeapPage*)( prev_page->getData() );
          prev_hp->setNext( pnxt );
          this->buf_mgr->releasePage( smthn3, true );
      }
      // Fix pnxt->prev_page = pprev
      if( pnxt != INVALID_PAGE_NUM ){
          PageId smthn4;
          smthn4.file_id = this->file_id;
          smthn4.page_num = pnxt;

          Page* next_page = this->buf_mgr->getPage( smthn4 );
          HeapPage* next_hp = (HeapPage*)( next_page->getData() );
          next_hp->setPrev( pprev );
          this->buf_mgr->releasePage( smthn4, true);
      }

      file_header->full_size--;

      // add to head of full list
      hp->setPrev( INVALID_PAGE_NUM );
      hp->setNext( file_header->free );
      
      if (file_header->free != INVALID_PAGE_NUM) {
          // fix the old head’s prev
          PageId smthn5;
          smthn5.file_id = this->file_id;
          smthn5.page_num = file_header->free;

          Page* old_free_p = this->buf_mgr->getPage( smthn5 );
          HeapPage* old_free_hp = (HeapPage*)( old_free_p->getData() );
          old_free_hp->setPrev( target_pid.page_num );
          this->buf_mgr->releasePage( smthn5, true );
      }
      file_header->free = target_pid.page_num;
      file_header->free_size++;

      (this->buf_mgr)->releasePage(this->header_id, false);
    }

/*************************************/
PageId HeapFile::anonAppend(PageId anon_pid) {

  PageId ret_page_id = INVALID_PAGE_ID;
  return ret_page_id;
}


PageId HeapFile::atomicAppend(PageId orig_pid){
  
  PageId new_pid = INVALID_PAGE_ID;
  return new_pid;
}