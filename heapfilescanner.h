#ifndef  _SWATDB_HEAPFILESCANNER_H_
#define  _SWATDB_HEAPFILESCANNER_H_

/**
 * \file 
 */

#include <string>
#include <vector>
#include <mutex>
#include "swatdb_types.h"
#include "file.h" //for inheritance

class HeapPage; 
class HeapFile;
class Record;
class HeapPageScanner;

/**
 * Scanner class for HeapFile. Scanner scans the given HeapFile object and
 * returns the next RecordId and initiliazes the given Record Data whenever
 * getNext is called. Returns INVALID_RECORD_ID if it reaches the end of the
 * file.
 */
class HeapFileScanner{

  public:

    /**
     * @brief Constructor.
     *
     * @pre Valid HeapFile* is provided as input.
     * @post HeapFile object is constructed with initialized data members. The
     *    cur_page is pinned if file is not empty.
     *
     * @param file HeapFile object to be scanned.
     */
    HeapFileScanner(HeapFile* file);

    /**
     * @brief Destructor.
     *
     * @pre If the end of the File has not been reached, cur_page is pinned.
     * @post If the end of the File has not been reached, cur_page is released.
     */
    ~HeapFileScanner();

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
    RecordId getNext(Record* record);

  private:

    /**
     * Pointer to the BufferManager of SwatDB (for retrieving/pinning
     * pages).
     */
    BufferManager* buf_mgr;

    /**
     * PageId of the Page that contains the latest Record scanned.
     */
    PageId cur_pid;

    /**
     * HeapPage object that contains the latest Record scanned.
     */
    HeapPage* cur_page;

    /**
     * HeapPageScanner object pointer for scanning through individual
     * HeapPages.
     */
    HeapPageScanner* scanner;

    /**
     * Pointer to the HeapFile to be scanned.
     */
    HeapFile* file;

    /**
     * bool indicating whether the end of the full pages list has been reached
     * in the scan.
     */
    bool end_of_full;

    /**
     * bool indicating whether the end of the free pages list has been reached
     * in the scan.
     */
    bool end_of_free;
};

#endif