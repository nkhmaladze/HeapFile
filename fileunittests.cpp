#include <string>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <vector>
#include <unordered_map>
#include <UnitTest++/UnitTest++.h>
#include <UnitTest++/TestReporterStdout.h>
#include <UnitTest++/TestRunner.h>

#include "swatdb_exceptions.h"
#include "filemgr.h"
#include "heappage.h"
#include "heappagescanner.h"
#include "heapfile.h"
#include "heapfilescanner.h"
#include "data.h"
#include "record.h"
#include "bufmgr.h"
#include "diskmgr.h"
#include "catalog.h"
#include "swatdb.h"

/*
 * Unit tests for HeapFile.
 */

/*
 * TestFixture Class for initializing and cleaning up objects. Any test called
 * with this class as TEST_FIXTURE has access to any public and protected data
 * members and methods of the object as if they were local variables and
 * helper functions of test functions. The constructor is called at the start
 * of the test to initialize the data members and destructor is called at the
 * end of the test to deallocate/free memory associated with the objects.
 * It is possible to declare another custom class and associate with tests via
 * TEST_FIXTURE. It is also possible to add more data members and functions.
 * Students are encouraged to do so as they see fit so lon as they are careful
 * not to cause any naming conflicts with other tests.
 */
class TestFixture{

  public:
    /*
     * Public data members that tests have access to as if they are local
     * variables.
     */
    SwatDB* db;
    BufferManager* buf_mgr;
    FileManager* file_mgr;
    FileId file_id;
    HeapFile* file;
    Record record;
    Data* data;
    /*
     * Initializes all variables needed for testing.
     */
    TestFixture(){
      db = new SwatDB("./");
      buf_mgr = db->getBufMgr();
      file_mgr = db->getFileMgr();
      file_id = file_mgr->createRelation(std::string("Rel1"), nullptr,
          HeapFileT, std::string("testrel1.rel"));
      file = (HeapFile*) file_mgr->getFile(file_id);
      data = new Data(PAGE_SIZE);
      record.setRecordData(data);
    }

    /*
     * Clean up and deallocates all objects initilalized by the constructor.
     */
    ~TestFixture(){
      buf_mgr->clearBuffer();
      file_mgr->removeFile(file_id);
      delete data;
      delete db;
    }
    /*
     * Helper function for checking Header Page state.
     */
    void checkHeader(PageNum full, PageNum free, std::uint32_t full_size,
          std::uint32_t free_size, std::uint32_t num_records){
      HeapFileHeader header = file->getHeader();
      CHECK_EQUAL(full, header.full);
      CHECK_EQUAL(free, header.free);
      CHECK_EQUAL(full_size, header.full_size);
      CHECK_EQUAL(free_size, header.free_size);
      CHECK_EQUAL(num_records, header.num_records);
      //sleep(2);
    }

    /*
     * Helper function for checking Header Page state.
     */
    void checkHeader(std::uint32_t full_size, std::uint32_t free_size,
        std::uint32_t num_records){
      HeapFileHeader header = file->getHeader();
      CHECK_EQUAL(full_size, header.full_size);
      CHECK_EQUAL(free_size, header.free_size);
      CHECK_EQUAL(num_records, header.num_records);
    }

};

/*
 * Tests creatHeader.
 */
SUITE(createHeader){

  /*
   * Checks if the initial state is initialized properly after the HeapFile is
   * created.
   */
  TEST_FIXTURE(TestFixture,createHeader){

    std::cout << "create header\n";
    checkHeader(INVALID_PAGE_NUM, INVALID_PAGE_NUM, 0, 0, 0);

  }
}

/*
 * Tests insertRecord.
 */
SUITE(insertRecord){

  /*
   * Inserts the record of MAX_RECORD_SIZE, that is initialized to array of 0
   * and checks if file state is updated accordingly. Checks that no page is
   * pinned.
   */
  TEST_FIXTURE(TestFixture,insertRecord1){
    RecordId record_id;

    std::cout << "insertRecord tests:\n";
    std::cout << "  insert one page-sized record\n";
    data->setSize(MAX_RECORD_SIZE);
    memset(data->getData(), 0, MAX_RECORD_SIZE);
    //insert the record
    record_id = file->insertRecord(record);

    //Check the heapfile state
    checkHeader(record_id.page_num, INVALID_PAGE_NUM, 1, 0, 1);
    //check pin count
    CHECK_EQUAL(0, buf_mgr->getBufferState().pinned);
  }

  /*
   * Inserts a record of size 1, that is initialized to array of 0 and
   * checks if file state is updated accordingly. Checks that no page is pinned.
   */
  TEST_FIXTURE(TestFixture,insertRecord2){
    RecordId record_id;

    std::cout << "  insert one small record\n";
    data->setSize(1);
    memset(data->getData(), 0, 1);
    //insert the record
    record_id = file->insertRecord(record);

    //Check the heapfile state
    checkHeader(INVALID_PAGE_NUM, record_id.page_num, 0, 1, 1);
    //check pin count
    CHECK_EQUAL(0, buf_mgr->getBufferState().pinned);
  }

  /*
   * Inserts a record of MAX_RECORD_SIZE+1, that is initialized to array of 0
   * and checks if exception is thrown. Checks file state and that no page
   * is pinned.
   */
  TEST_FIXTURE(TestFixture,insertRecord3){
    RecordId record_id;

    std::cout << "  try to insert a record that is too big\n";
    memset(data->getData(), 0, MAX_RECORD_SIZE+1);
    data->setSize(MAX_RECORD_SIZE+1);
    //insert the record
    CHECK_THROW(record_id = file->insertRecord(record), 
        InsufficientSpaceHeapPage);

    //Check the heapfile state
    checkHeader(INVALID_PAGE_NUM, INVALID_PAGE_NUM, 0, 0, 0);
    CHECK_EQUAL(0, buf_mgr->getBufferState().pinned);
  }

  /*
   * Inserts 10 records of MAX_RECORD_SIZE/2 that is initialized to array of 0
   * and checks if file state is updated accordingly. Checks that no page is
   * pinned.
   */
  TEST_FIXTURE(TestFixture,insertRecord4){
    RecordId record_id;

    std::cout 
      << "  insert 10 MAX_RECORD_SIZE/2 recs (requires 10 pages, none full)\n";
    data->setSize(MAX_RECORD_SIZE/2);
    memset(data->getData(), 0, MAX_RECORD_SIZE/2);
    //insert the record
    for (int i=0; i < 10; i++){
      record_id = file->insertRecord(record);
    }

    //Check the heapfile state
    checkHeader(INVALID_PAGE_NUM, record_id.page_num, 0, 10, 10);
    //check pin count
    CHECK_EQUAL(0, buf_mgr->getBufferState().pinned);
  }

  /*
   * Inserts MAX_PAGE_NUM-1 records of MAX_RECORD_SIZE that is initialized to
   * array of 0. Try inserting another page and check for exception. Checks if
   * file state is updated accordingly. Checks that no page is pinned.
   */
  TEST_FIXTURE(TestFixture,insertRecord5){
    RecordId record_id;

    std::cout 
      << "  insert lots of records (MAX_PAGE_NUM-1 recourds of MAX_RECORD_SIZE)"
      << "\n  (requires MAX_PAGE_NUM pages, all full)\n";
    data->setSize(MAX_RECORD_SIZE);
    memset(data->getData(), 0, MAX_RECORD_SIZE);
    //insert the record
    for (std::uint32_t i=0; i < MAX_PAGE_NUM-1; i++){
      record_id = file->insertRecord(record);
    }
    CHECK_THROW(file->insertRecord(record), InsufficientSpaceHeapFile);

    //Check the heapfile state
    checkHeader(record_id.page_num, INVALID_PAGE_NUM, MAX_PAGE_NUM-1, 0,
          MAX_PAGE_NUM-1);
    //check pin count
    CHECK_EQUAL(0, buf_mgr->getBufferState().pinned);
  }

  /*
  * Checks that exception is thrown in insertRecord for
  * InvalidSchemaHeapFile
  */
  TEST_FIXTURE(TestFixture, insertRecord6){
    RecordId record_id;

    std::cout 
      << "  insert lots of records (MAX_PAGE_NUM-1 recourds of MAX_RECORD_SIZE + 1)"
      << "\n  (Check Insert Record Exceptions Pass )\n";
    data->setSize(MAX_RECORD_SIZE);
    memset(data->getData(), 0, MAX_RECORD_SIZE);

    record_id = file->insertRecord(record);

    //give record invalid schema
    //Schema* h = data->getData();
    record.setSchema((Schema*)data->getData());

    CHECK_THROW(file->insertRecord(record), InvalidSchemaHeapFile);

    //Check the heapfile state
    checkHeader(record_id.page_num, INVALID_PAGE_NUM, 1, 0,
          1);
    //check pin count
    CHECK_EQUAL(0, buf_mgr->getBufferState().pinned);
  }


  /*
  * Checks that insertRecord properly moves pages from free to full list
  */
  TEST_FIXTURE(TestFixture, insertRecord7){
    RecordId record_id;

    std::cout 
      << "  insert 10 MAX_RECORD_SIZE/2 recs (requires 10 pages, none full)\n";
    data->setSize(MAX_RECORD_SIZE/4);
    memset(data->getData(), 0, MAX_RECORD_SIZE/4);
    //insert the record
    for (int i=0; i < 8; i++){
      record_id = file->insertRecord(record);
    }

    //Check the heapfile state
    checkHeader(record_id.page_num, INVALID_PAGE_NUM, 2, 0, 8);
    //check pin count
    CHECK_EQUAL(0, buf_mgr->getBufferState().pinned);
  }
}

/*
 * Tests getRecord.
 */
SUITE(getRecord) {

  /*
   * Inserts the record of MAX_RECORD_SIZE, that is initialized to array of 1
   * and get it. Checks if the record data is consistent. Checks if file state
   * is updated accordingly. Checks that no page is pinned.
   */
  TEST_FIXTURE(TestFixture,getRecord1){
    RecordId record_id;
    char* temp = new char[MAX_RECORD_SIZE];


    std::cout << "getRecord tests:\n";
    std::cout << "  check that getRecord gets record data that were inserted\n";
    //initialize temp array and record data
    data->setSize(MAX_RECORD_SIZE);
    memset(temp, 1, MAX_RECORD_SIZE);
    memset(data->getData(), 1, MAX_RECORD_SIZE);
    //insert the record
    record_id = file->insertRecord(record);

    //reset record data
    data->setSize(0);
    memset(data->getData(), 0, MAX_RECORD_SIZE);

    file->getRecord(record_id, &record);
    CHECK(!memcmp(temp,data->getData(),MAX_RECORD_SIZE));
    CHECK_EQUAL(MAX_RECORD_SIZE, data->getSize());

    //Check the heapfile state
    checkHeader(record_id.page_num, INVALID_PAGE_NUM, 1, 0, 1);
    //check pin count
    CHECK_EQUAL(0, buf_mgr->getBufferState().pinned);

    //clean up
    delete[] temp;
  }

  /*
   * Checks the exception cases for getRecord, i.e. InvalidSchemaHeapFile
   * and SwatDBException propogated for invalid RecordId.
   */
  TEST_FIXTURE(TestFixture, getRecord2){
    //Check that invalid rids throw SwatDBException
    RecordId invalid_rid = {1,1};

    std::cout << "  check exception cases for getRecord\n";
    CHECK_THROW(file->getRecord(invalid_rid, &record), SwatDBException);
    RecordId record_id;

    //set the record's data field
    data->setSize(MAX_RECORD_SIZE);
    memset(data->getData(), 0, MAX_RECORD_SIZE);

    //get a valid rid by inserting a valid record
    record_id = file->insertRecord(record);
    record.setSchema((Schema*) 1);

    //The thrown error should be a result of improper schema, not invalid rid
    CHECK_THROW(file->getRecord(record_id, &record), InvalidSchemaHeapFile);

    //The record should have filled up an entire page, putting it in the full
    //list.
    checkHeader(1, 0, 1);
    CHECK_EQUAL(0, buf_mgr->getBufferState().pinned);
  }
  /*
   * Checks that getRecord can retrieve records from both free and full pages.
   * Inserts 10 records into the HeapFile, alernating between records of
   * MAX_RECORD_SIZE and records of 1 byte. The result should be five pages
   * in the full list and one page with five 1-byte records in the free list.
   * Checks that each record can be retrieved, and that the data for each is
   * correct.
   */
  TEST_FIXTURE(TestFixture, getRecord3){
    //Initialize a vector for record_ids and a temp char array for memory
    //checking.
    std::vector<RecordId> record_ids;
    char* temp = new char[MAX_RECORD_SIZE];

    std::cout 
      << "  check getRecords can retrieve records from both free & full list\n";
    //Insert 10 records, alternting between records of MAX_RECORD_SIZE and
    //1-byte records. Each record should be filld with a unique char.
    for(int i=0; i<10; i++){
      if(i%2){
        data->setSize(1);
        memset(data->getData(), i%128, 1);
      }
      else{
        data->setSize(MAX_RECORD_SIZE);
        memset(data->getData(), i%128, MAX_RECORD_SIZE);
      }
      record_ids.push_back(file->insertRecord(record));
    }

    //Check that the state of the heapfile is as we expect
    // i.e. (5 full pages, 1 free)
    checkHeader(5, 1, 10);

    //Check that each record can be retrieved and that its data is correct.
    for(int i=0; i<10; i++){
      file->getRecord(record_ids.at(i), &record);
      if(i%2){
        memset(temp, i%128, 1);
        CHECK(!memcmp(data->getData(), temp, 1));
        CHECK_EQUAL(1, data->getSize());
      }
      else{
        memset(temp, i%128, MAX_RECORD_SIZE);
        CHECK(!memcmp(data->getData(), temp, MAX_RECORD_SIZE));
        CHECK_EQUAL(MAX_RECORD_SIZE, data->getSize());
      }
    }

    //Check that getRecord() did not alter the state of the HeapFile.
    checkHeader(5, 1, 10);
    CHECK_EQUAL(0, buf_mgr->getBufferState().pinned);

    //clean up
    delete[] temp;
  }
}

/*
 * Tests deleteRecord.
 */
SUITE(deleteRecord) {

  /*
   * Inserts the record created in the TestFixture, and then deletes it. Checks
   * that the record can no longer be retrieved from the file.
   */
  TEST_FIXTURE(TestFixture, deleteRecord1){
    //set the record's data field
    data->setSize(MAX_RECORD_SIZE);
    memset(data->getData(), 0, MAX_RECORD_SIZE);

    std::cout << "deleteRecord teststs: \n";
    std::cout << "  insert and delete a record, checks\n";

    //Insert default record into the file
    RecordId record_id = file->insertRecord(record);

    //Check that upon inserting the record, which should occupy an entire page,
    //that there exists one full page in the HeapFile.
    checkHeader(record_id.page_num, INVALID_PAGE_NUM, 1, 0, 1);

    //Delete the record
    file->deleteRecord(record_id);

    //Check that the record can no longer be retrieved
    CHECK_THROW(file->getRecord(record_id, &record), SwatDBException);

    //Check that upon deleting the only record, the header information for
    //pages is correct, i.e. no pages exist in the HeapFile.
    checkHeader(INVALID_PAGE_NUM, INVALID_PAGE_NUM, 0, 0, 0);
    CHECK_EQUAL(0, buf_mgr->getBufferState().pinned);
  }


  /*
   * Checks that multiple records can be correctly deleted. Inserts 10 records,
   * deletes 5 records, and checks the final state of the HeapFile.
   */
  TEST_FIXTURE(TestFixture, deleteRecord2){
    //prepares a vector to store record ids, intializes record data
    std::vector<RecordId> record_ids;

    std::cout 
      << "  check multiple records correctly deleted\n";
    data->setSize(MAX_RECORD_SIZE);
    memset(data->getData(), 0, MAX_RECORD_SIZE);

    //insert 10 identical records
    for(int i=0; i<10; i++){
      record_ids.push_back(file->insertRecord(record));
    }

    //delete 5 of the inserted records
    for(int i=0; i<5; i++){
      file->deleteRecord(record_ids.at(i*2));
    }

    //check that 5 full pages remain
    checkHeader(5, 0, 5);
    CHECK_EQUAL(0, buf_mgr->getBufferState().pinned);
  }

  /*
   * Checks Exceptions are thrown in deleteRecord
   */
  TEST_FIXTURE(TestFixture, deleteRecord3){
    RecordId record_id;
    PageNum wrong_num;
    SlotId wrong_id;

    std::cout 
      << "  insert lots of records (MAX_PAGE_NUM-1 recourds of MAX_RECORD_SIZE + 1)"
      << "\n  (Check Insert Record Exceptions Pass )\n";
    data->setSize(MAX_RECORD_SIZE);
    memset(data->getData(), 0, MAX_RECORD_SIZE);

    record_id = file->insertRecord(record);

    wrong_num = INVALID_PAGE_NUM;
    record_id.page_num = wrong_num;

    CHECK_THROW(file->deleteRecord(record_id), InvalidPageIdBufMgr);

    wrong_id = INVALID_SLOT_OFFSET;
    record_id.slot_id = wrong_id;
    CHECK_THROW(file->deleteRecord(record_id), InvalidSlotIdHeapPage);

    //check pin count
    CHECK_EQUAL(0, buf_mgr->getBufferState().pinned);
  }


  /*
   * Ensures deleting records moves pages to free list
   */
  TEST_FIXTURE(TestFixture, deleteRecord4){
    //prepares a vector to store record ids, intializes record data
    std::vector<RecordId> record_ids;

    std::cout 
      << "  check multiple records correctly deleted\n";
    data->setSize(MAX_RECORD_SIZE);
    memset(data->getData(), 0, MAX_RECORD_SIZE);

    //insert 10 identical records
    for(int i=0; i<10; i++){
      record_ids.push_back(file->insertRecord(record));
    }

    //delete 5 of the inserted records
    for(int i=0; i<5; i++){
      file->deleteRecord(record_ids.at(i*2));
    }

    //check that 5 full pages remain
    checkHeader(5, 0, 5);
    CHECK_EQUAL(0, buf_mgr->getBufferState().pinned);
  }
}

/*
 * Tests updateRecord.
 */
SUITE(updateRecord) {

  /*
   * Initalizes the record created in the TestFixture to a single char of 1 and
   * inserts it. Updates the record to array of 2's of MAX_RECORD_SIZE. Check
   * for consistency. (Page should be moved from free to full list).
   */
  TEST_FIXTURE(TestFixture, updateRecord1){
    char* temp = new char[MAX_RECORD_SIZE];

    std::cout  << "updateRecord tests:\n";
    std::cout << "  insert small update to max, page should be moved to full\n";
    //set the record's data field to a single char of 1.
    data->setSize(1);
    memset(data->getData(), 1, 1);

    //Insert default record into the file
    RecordId record_id = file->insertRecord(record);

    //check the Header Page.
    checkHeader(INVALID_PAGE_NUM, record_id.page_num, 0, 1, 1);
    //update the record to char array of MAX_RECORD_SIZE, filled with 2.
    data->setSize(MAX_RECORD_SIZE);
    memset(data->getData(), 2, MAX_RECORD_SIZE);
    file->updateRecord(record_id, record);

    //reset record data and get the updated record.
    data->setSize(0);
    memset(data->getData(), 0, MAX_RECORD_SIZE);
    file->getRecord(record_id, &record);
    //Check that record data is updated properly.
    memset(temp, 2, MAX_RECORD_SIZE);
    CHECK(!memcmp(temp, data->getData(),MAX_RECORD_SIZE));
    CHECK_EQUAL(MAX_RECORD_SIZE, data->getSize());
    checkHeader(record_id.page_num, INVALID_PAGE_NUM, 1, 0, 1);
    CHECK_EQUAL(0, buf_mgr->getBufferState().pinned);

    delete[] temp;
  }

  /*
   * Checks if updateRecord moves record from full to free list. Initalizes the
   * record created in the TestFixture to a array of 1s of MAX_RECORD_SIZE and
   * inserts it. Updates the record to a single char of 2. Check for
   * consistency.
   */
  TEST_FIXTURE(TestFixture, updateRecord2){

    std::cout << "  check updateRecord from huge to small moves "
      << "page from full to free list.\n";
    //set the record's data field to a single char of 1.
    data->setSize(MAX_RECORD_SIZE);
    memset(data->getData(), 1, MAX_RECORD_SIZE);

    //Insert default record into the file
    RecordId record_id = file->insertRecord(record);

    //check the Header Page.
    checkHeader(record_id.page_num, INVALID_PAGE_NUM, 1, 0, 1);
    //update the record to single char of 2.
    data->setSize(1);
    data->getData()[0] = 2;
    file->updateRecord(record_id, record);

    //reset record data and get the updated record.
    data->setSize(0);
    memset(data->getData(), 0, MAX_RECORD_SIZE);
    file->getRecord(record_id, &record);
    //Check that record data is updated properly.
    CHECK_EQUAL(2, data->getData()[0]);
    CHECK_EQUAL(1, data->getSize());
    checkHeader(INVALID_PAGE_NUM,record_id.page_num, 0, 1, 1);
    CHECK_EQUAL(0, buf_mgr->getBufferState().pinned);

  }

  /*
   * Checks if the previous record is not deleted if update fails. Initalizes
   * the record created in the TestFixture to a single char of 1 and
   * inserts it. Updates the record to array of 2's of MAX_RECORD_SIZE+1. Checks
   * if update throws an exception and state is consistenct.
   */
  TEST_FIXTURE(TestFixture, updateRecord3){

    std::cout << "  check previous record not deleted if update fails\n";
    //set the record's data field to a single char of 1.
    data->setSize(1);
    memset(data->getData(), 1, 1);

    //Insert default record into the file
    RecordId record_id = file->insertRecord(record);

    //check the Header Page.
    checkHeader(INVALID_PAGE_NUM, record_id.page_num, 0, 1, 1);
    //update the record to char array of MAX_RECORD_SIZE, filled with 2.
    data->setSize(MAX_RECORD_SIZE+1);
    memset(data->getData(), 2, MAX_RECORD_SIZE+1);
    CHECK_THROW(file->updateRecord(record_id, record), InsufficientSpaceHeapPage);

    //reset record data and get the updated record.
    data->setSize(0);
    memset(data->getData(), 0, MAX_RECORD_SIZE);
    file->getRecord(record_id, &record);
    //Check that record data is updated properly.

    CHECK_EQUAL(1, data->getData()[0]);
    CHECK_EQUAL(1, data->getSize());
    checkHeader(INVALID_PAGE_NUM, record_id.page_num, 0, 1, 1);
    CHECK_EQUAL(0, buf_mgr->getBufferState().pinned);
  }
} 

/* 
 * Tests various methods of HeapFile.
 */
SUITE(variousMethods) {
  /*
   * Inserts records of various length, ranging from 1 to min(MAX_RECORD_SIZE,
   * MAX_PAGE_NUM). Get all the inserted records and check for consistency in
   * record data.
   */
  TEST_FIXTURE(TestFixture,variousMethods1){
    std::vector<RecordId> record_ids;
    char* temp = new char[MAX_RECORD_SIZE];

    std::cout << "various Methods:\n";
    std::cout << "  insert recs of variaous lengths, checks:\n";

    //min(MAX_RECORD_SIZE, MAX_PAGE_NUM)
    std::uint32_t num = (MAX_RECORD_SIZE < MAX_PAGE_NUM) ? MAX_RECORD_SIZE :
        MAX_PAGE_NUM;

    //insert records of varying size, increasing from 1 to num
    for (std::uint32_t i=1; i < num; i++){
      data->setSize(i);
      memset(data->getData(), i%128, i);
      record_ids.push_back(file->insertRecord(record));
    }

    for (std::uint32_t i=1; i < num; i++){
      memset(temp, i%128, i);
      file->getRecord(record_ids.at(i-1), &record);
      //check data
      CHECK(!memcmp(temp,data->getData(),i));
      CHECK_EQUAL(i, data->getSize());
      //reset data
      data->setSize(0);
      memset(data->getData(), 0, MAX_RECORD_SIZE);
    }

    //check pin count
    CHECK_EQUAL(0, buf_mgr->getBufferState().pinned);
    delete[] temp;
  }

  /*
   * Inserts records of various length, ranging from 1 to min(MAX_RECORD_SIZE,
   * MAX_PAGE_NUM). Delete all the inserted records and check if getting all
   * records throws exception. Checks the final header page state.
   */
  TEST_FIXTURE(TestFixture,variousMethods2){

    std::vector<RecordId> record_ids;

    std::cout << "  test inserting records of various lengths\n";
    //min(MAX_RECORD_SIZE, MAX_PAGE_NUM)
    std::uint32_t num = (MAX_RECORD_SIZE < MAX_PAGE_NUM) ? MAX_RECORD_SIZE :
        MAX_PAGE_NUM;

    //insert records of varying size, increasing from 1 to num
    for (std::uint32_t i=1; i < num; i++){
      data->setSize(i);
      memset(data->getData(), i%128, i);
      record_ids.push_back(file->insertRecord(record));
    }
    //delete all records
    for (std::uint32_t i=0; i < num-1; i++){
      file->deleteRecord(record_ids.at(i));
    }

    for (std::uint32_t i=0; i < num-1; i++){
      CHECK_THROW(file->getRecord(record_ids.at(i), &record),
          SwatDBException);
    }
    //check header page
    checkHeader(0,0,0);
    //check pin count
    CHECK_EQUAL(0, buf_mgr->getBufferState().pinned);
  }
}

/*
 * Prints usage
 */
void usage(){
  std::cout << "Usage: ./unittestsf -s <suite_name> -h help\n";
  std::cout << "Available Suites: " <<
      "createHeader, insertRecord, getRecord, updateRecord, deleteRecord,\n" 
      << "variousMethods, mytests" << std::endl;
}

/*
 * The main program either run all tests or tests a specific SUITE, given its
 * name via command line argument option 's'. If no argument is given by argument
 * option 's', main runs all tests by default. If invalid argument is given by
 * option 's', 0 test is run
 */
int main(int argc, char** argv){
  const char* suite_name;
  bool test_all = true;
  int c;

  //check for suite_name argument if provided
  while ((c = getopt (argc, argv, "hs:")) != -1){
    switch(c) {
      case 'h': usage();
                exit(1);
      case 's': suite_name = optarg;
                test_all  = false;
                break;
      default: printf("optopt: %c\n", optopt);

    }
  }

  //run all tests
  if (test_all){
    return UnitTest::RunAllTests();
  }

  //run the SUITE of the given suite name
  UnitTest::TestReporterStdout reporter;
  UnitTest::TestRunner runner(reporter);
  return runner.RunTestsIf(UnitTest::Test::GetTestList(), suite_name,
      UnitTest::True(), 0);
}