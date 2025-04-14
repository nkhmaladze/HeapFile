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


#include "testerconf.h"

/*
 * Checkpoint Unit tests for HeapFile.
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
    SwatDB *db;
    BufferManager *buf_mgr;
    FileManager *file_mgr;
    FileId file_id;
    HeapFile *file;
    Record record;
    Data *data;

    /*
     * Initializes all variables needed for testing.
     */
    TestFixture(){

      db = new SwatDB(testdb_dir.c_str());
      buf_mgr = db->getBufMgr();
      file_mgr = db->getFileMgr();
      file_id = file_mgr->createRelation(std::string("Rel1"), nullptr,
          HeapFileT, std::string("testrel.rel"));
      file = (HeapFile*) file_mgr->getFile(file_id);

      // init this record data to a whole bunch of x's
      // (you can replace its data value with whatever you want)
      // to use it, set its size to whatever you want
      // (it is too big right now, PAGE_SIZE, to insert, which
      // can be used to test inserting too large, or change its
      // size to insert something smaller)
      data = new Data(PAGE_SIZE);
      setRecData(data, 'x', PAGE_SIZE);
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

    /**
     * Helper function to set record data to
     * some number of record data to a char value, and updates
     * it size.
     *
     * It is up to the caller to ensure that the passed rec_data
     * is allocated and has a data field with enough capacity
     * to handle the request.
     *
     * rec_data: Data * to fill
     * val: char value to fill it with
     * size: number of bytes to fill in the buffer
     */
    void setRecData(Data *rec_data, char val, std::uint32_t size) {
        memset(rec_data->getData(), val, size);
        rec_data->setSize(size);
    }
    /**
     * returns true if the test_rec data and size matches
     * the answer_rec data and size
     *
     * It is up to the caller to ensure that the passed rec_data
     * is allocated and has a data field with enough capacity
     * to handle the request.
     */
    bool compareRecRec(Data *answer_rec, Data *test_rec) {

      if(answer_rec->getSize() != test_rec->getSize()) {
        return false;
      }
      if( !memcmp(answer_rec->getData(), test_rec->getData(),
            answer_rec->getSize()) ) {
        return true;
      }
      return false;
    }
    /**
     * returns true if the test_rec data matches the value
     * in the test_value buffer up to answer_rec size bytes
     *
     * It is up to the caller to ensure that the passed rec_data
     * is allocated and has a data field with enough capacity
     * to handle the request.
     */
    bool compareRecMem(Data *answer_rec, char *test_value) {

      if( !memcmp(answer_rec->getData(), test_value,
            answer_rec->getSize()) ) {
        return true;
      }
      return false;
    }
    /**
     * returns true if the test_rec data matches the value
     * in the test_value buffer up to answer_rec size bytes
     *
     * It is up to the caller to ensure that the passed rec_data
     * is allocated and has a data field with enough capacity
     * to handle the request.
     */
    bool compareMemMem(char *answer, char *test, std::uint32_t size) {

      if( !memcmp(answer, test, size) ) {
        return true;
      }
      return false;
    }

    /**
     * Helper function for checking Header Page state.
     * @param head: the expected page number of the first page on the list
     * @param num_pages: the expected number of pages
     * @param num_records: the expected number of records
     */
    void checkHeader(PageNum head, std::uint32_t num_pages,
        std::uint32_t num_records)
    {

      int result = 1;
      HeapFileHeader header;

      header = file->getHeader();
      if ((head != header.free) && (head != header.full)) {
        result = 0;
      }

      CHECK_EQUAL(1, result);
      CHECK_EQUAL(num_pages, (header.full_size + header.free_size));
      CHECK_EQUAL(num_records, header.num_records);
    }
    /**
     * Helper function for checking Header Page state.
     * @param num_pages: the expected number of pages
     * @param num_records: the expected number of records
     */
    void checkHeader(std::uint32_t num_pages, std::uint32_t num_records){

      HeapFileHeader header = file->getHeader();

      CHECK_EQUAL(num_pages, (header.full_size + header.free_size));
      CHECK_EQUAL(num_records, header.num_records);
    }


};

/*
 * Tests createHeader.
 */
SUITE(createHeader){

  /*
   * Checks if the initial state is initialized properly after the
   * HeapFile is created.
   * (initializeHeader is called in the TestFixture constructor
   */
  TEST_FIXTURE(TestFixture, createHeader){

    std::cout << "check createHeader\n";
    checkHeader(INVALID_PAGE_NUM, 0, 0);

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
    std::cout << "  insertRec1: insert one record check file & BP state\n";
    // this is using the data, and record data members
    // of the TestFixture class (could also use local vars)
    data->setSize(MAX_RECORD_SIZE);
    setRecData(data, 0, MAX_RECORD_SIZE);
    //insert the record
    record_id = file->insertRecord(record);

    //Check the heapfile state: should be 1 page allocated, and 1 record
    checkHeader(record_id.page_num, 1, 1);

    //check pin count
    CHECK_EQUAL(0, buf_mgr->getBufferState().pinned);
  }

  /*
   * Try to inserts a record that is too big to store in the file
   * (size MAX_RECORD_SIZE+1), and checks if exception is thrown.
   * Checks resulting file state and that no page is pinned.
   */
  TEST_FIXTURE(TestFixture,insertRecord2){
    RecordId record_id;

    std::cout << "  insertRecord2: try to insert too big rec, checks state\n";
    setRecData(data, 0, MAX_RECORD_SIZE+1);
    data->setSize(MAX_RECORD_SIZE+1);

    //insert the record
    CHECK_THROW(record_id = file->insertRecord(record),
        InsufficientSpaceHeapPage);

    //Check the heapfile state
    checkHeader(INVALID_PAGE_NUM, 0, 0);
    CHECK_EQUAL(0, buf_mgr->getBufferState().pinned);

  }

  /*
   * Inserts 4 copies of a record of MAX_RECORD_SIZE/4, initialized to `b`.
   * (the first 3 should fit on the first page, the 4th on a 2nd page)
   * Checks if file state is updated accordingly.
   * Checks that no page is pinned in Buffer Pool after inserRecord.
   */
  TEST_FIXTURE(TestFixture,insertRecord3){
    RecordId record_id;

    std::cout << "  insertRecord3: insert 4 recs should span 2 page\n";
    data->setSize(MAX_RECORD_SIZE/4);
    setRecData(data, 'b', MAX_RECORD_SIZE/4);

    // insert the record  times, the first three should be on the same page
    for (int i=0; i < 4; i++){
      record_id = file->insertRecord(record);
    }

    // Check the heapfile state
    checkHeader(2, 4);

    // check pin count on the page in the Buffer pool
    CHECK_EQUAL(0, buf_mgr->getBufferState().pinned);
  }

}


/*
 * Tests getRecord.
 */
SUITE(getRecord){

  /*
   * Inserts one record of size 20, initialized to array of 6, and get it.
   * Checks if the record data is consistent. Checks if file state
   * is updated accordingly. Checks that no page is pinned.
   */
  TEST_FIXTURE(TestFixture,getRecord1){
    RecordId record_id;
    char* test_val = new char[20];

    std::cout << "getRecord tests:\n";
    std::cout << "  getRecord1: insert a record gets it checks state\n";
    // initialize test_val array and record data
    memset(test_val, 6, 20);  // our copy to check result
    data->setSize(20);
    setRecData(data, 6, 20);

    // insert the record
    record_id = file->insertRecord(record);

    // reset record data
    data->setSize(0);
    setRecData(data, 0, 20);

    // call getRecord
    file->getRecord(record_id, &record);

    CHECK(compareMemMem(test_val, data->getData(), 20));
    CHECK_EQUAL(20, data->getSize());

    // check the heapfile state
    checkHeader(record_id.page_num, 1, 1);
    //check pin count
    CHECK_EQUAL(0, buf_mgr->getBufferState().pinned);

    //clean up
    delete[] test_val;
  }

  /*
   * Checks the exception cases for getRecord, i.e. InvalidSchemaHeapFile
   * and SwatDBException propogated for invalid RecordId.
   */
  TEST_FIXTURE(TestFixture, getRecord2){

    RecordId record_id, invalid_rid;

    std::cout << "  getRecord2: checks some exception cases\n";
    // Check that invalid rids throw SwatDBException
    invalid_rid.page_num = 1;
    invalid_rid.slot_id = 1;
    CHECK_THROW(file->getRecord(invalid_rid, &record), SwatDBException);


    //set the record's data field to all 'w'
    data->setSize(MAX_RECORD_SIZE);
    setRecData(data, 'w', MAX_RECORD_SIZE);

    //get a valid rid by inserting a valid record
    record_id = file->insertRecord(record);
    // set its schema to something bogus
    record.setSchema((Schema *) 1);

    //The thrown error should be a result of improper schema, not invalid rid
    CHECK_THROW(file->getRecord(record_id, &record), InvalidSchemaHeapFile);

    //The record should have filled up an entire page
    checkHeader(1, 1);
    CHECK_EQUAL(0, buf_mgr->getBufferState().pinned);
  }

  /*
   * Checks that getRecord can retrieve records of various size.
   * Inserts 10 records into the HeapFile, alernating between records of
   * MAX_RECORD_SIZE and records of 1 byte. The result should be six pages
   * in the HeapFile, 5 pages containing one record each and one page
   * containing five records. Checks that each record can be retrieved, and
   * that the data for each is correct.
   */
  TEST_FIXTURE(TestFixture, getRecord3){
    //Initialize a vector for record_ids and a temp char array for memory
    //checking.
    std::vector<RecordId> record_ids;
    char* temp = new char[MAX_RECORD_SIZE];

    std::cout << "  getRecord3: insert 5 small & 5 big recs: six pages \n";
    //Insert 10 records, alternting between records of MAX_RECORD_SIZE and
    //1-byte records. Each record should be filld with a unique char.
    for(int i=0; i<10; i++){
      if(i%2){
        data->setSize(1);
        setRecData(data, i%128, 1);
      }
      else{
        data->setSize(MAX_RECORD_SIZE);
        setRecData(data, i%128, MAX_RECORD_SIZE);
      }
      record_ids.push_back(file->insertRecord(record));
    }

    //Check that the state of the heapfile is as we expect
    // i.e. (6 pages, 10 records)
    checkHeader(6, 10);

    //Check that each record can be retrieved and that its data is correct.
    for(int i=0; i<10; i++){
      file->getRecord(record_ids.at(i), &record);
      if(i%2){
        memset(temp, i%128, 1);
        CHECK(compareMemMem(temp, data->getData(), 1));
        CHECK_EQUAL(1, data->getSize());
      }
      else{
        memset(temp, i%128, MAX_RECORD_SIZE);
        CHECK(compareMemMem(temp, data->getData(), MAX_RECORD_SIZE));
        CHECK_EQUAL(MAX_RECORD_SIZE, data->getSize());
      }
    }

    //Check that getRecord() did not alter the state of the HeapFile.
    checkHeader(6, 10);
    CHECK_EQUAL(0, buf_mgr->getBufferState().pinned);

    //clean up
    delete[] temp;
  }
}

/*
 * Prints usage
 */
void usage(){
  std::cout << "Usage: ./unittests -s <suite_name> -h help\n";
  std::cout << "Available Suites: " <<
      "createHeader, insertRecord, getRecord, updateRecord\n";
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
