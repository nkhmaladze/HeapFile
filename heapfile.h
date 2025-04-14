#ifndef  _SWATDB_HEAPFILE_H_
#define  _SWATDB_HEAPFILE_H_

/**
 * \file 
 */

#include <string>
#include <vector>
#include <mutex>
#include "swatdb_types.h"
#include "relationfile.h" //for inheritance

class BufferManager;
class HeapPage;
class HeapPageScanner;
class Schema;
class Record;

/**
 * HeapFile class.
 */
class HeapFile;

/**
 * Struct for the header metadata of HeapFile object. The header is casted
 * on top of the first Page allocated to the file.
 */
struct HeapFileHeader{

  /**
   * PageNum of the Page at the head of the linked list of free pages.
   */
  PageNum free;

  /**
   * PageNum of the Page at the head of the linked list of full pages.
   */
  PageNum full;

  /**
   * Number of pages in the linked list of free pages.
   */
  std::uint32_t free_size;

  /**
   * Number of pages in the linked list of full pages.
   */
  std::uint32_t full_size;

  /**
   * Number of records in the HeapFile.
   */
  std::uint64_t num_records;
    
};

/**
 * SwatDB HeapFile Class.
 * Represents heap file in the system. It consists of doubly linked list
 * of HeapPage and is an unsorted collection of records. Provides various
 * methods, including inserting, modifying and retrieving records.
 */
class HeapFile : public RelationFile {
  friend class Catalog;
  friend class FileManager;
  friend class HeapFileScanner;
  friend class BlockHeapFileScanner;  

  public:

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
    HeapFile(Catalog *catalog, BufferManager *buf_mgr, Schema *schema);

    /**
     * @brief HeapFile destructor.
     *
     * @pre None.
     * @post The HeapFile object is destroyed. The file on disk is not removed.
     */
    ~HeapFile();


    /**
     * @brief Returns the number of pages in the file.
     */
    std::uint32_t getNumPages();

    /**
     * @brief Returns the number of full pages in the file.
     */
    std::uint32_t getNumFullPages();

    /**
     * @brief Returns the number of records in the file.
     */
    std::uint64_t getNumRecords();

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
    void createHeader();

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
    void flushHeader();

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
     *    MAX_RECORD_SIZE.
     * @throw InsufficientSpaceHeapFile If the number of pages (including the
     *    header) in the HeapFile exceeds MAX_PAGE_NUM.
     */
    RecordId insertRecord(Record record);


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
    void getRecord(RecordId record_id, Record* record);

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
     *    of the page changes after the update, it is moved from one list to
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
    void updateRecord(RecordId record_id, Record record);

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
     * @throw InvalidPageIdBufMgr If the PageNum of the given RecordId is not
     *    valid.
     * @throw InvalidSlotIdHeapPage If the SlotId of the given RecordId is not
     *    valid.
     */
    void deleteRecord(RecordId record_id);

    /**
     * @brief THIS METHOD IS FOR DEBUGGING ONLY.
     *    Returns the current HeapFileHeader.
     */
    HeapFileHeader getHeader();

    /**
     * @brief This method returns the number of records in a file.
     */
    std::uint64_t getNumRecs(); 

  // NOTE:  ignore this method: not part of heapile assn
    PageId anonAppend(PageId orig_pid);

  // NOTE:  ignore this method: not part of heapile assn
    PageId atomicAppend(PageId orig_pid);  

  private:

  /**
     * @brief Moves from free list to full list
     *
     * @pre A pointer to a HeapPage is provided
     * @post 
     *
     * @param hp a HeapPage pointer to the page 
     */
  void freeToFull(HeapPage* hp, PageId target_pid);


   /**
     * @brief Moves from full list to free list
     *
     * @pre A pointer to a HeapPage is provided
     * @post 
     *
     * @param hp a HeapPage pointer to the page 
     */
  void fullToFree(HeapPage* hp, PageId target_pid);

};

#endif
