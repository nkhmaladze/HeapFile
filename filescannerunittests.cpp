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
 * Unit tests for HeapFileScanner.
 */

/*
 * Hash function used by HeapFileScanner test to use std::unordered_map with
 * key type of RecordId.
 */
struct RecordHash{
 std::size_t operator()(const RecordId& record_id) const{
   return (std::hash<unsigned int>() (record_id.page_num)) ^
       (std::hash<unsigned int>() (record_id.slot_id));
 }
};

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
class TestFixture {

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

SUITE(mytests){
  TEST_FIXTURE(TestFixture, test1) {
    std::cout << "add some of your own tests here\n";
  }
}

SUITE(HeapFileScanner) {

  /*
   * Checks that the HeapFileScanner can retrieve a record from both the full
   * and free list of pages, using 'getNext()'.
   *
   * Inserts a 1-byte record into the file, and a MAX_RECORD_SIZE bytes record
   * into the file. (Checks the state -- the result should be one page in the
   * free list and one page in the full list). Retrieves each of these records
   * using the HeapFileScanner object's 'getNext()' method.
   * The test performs the following checks:
   * 1) HeapFileScanner->getNext() returns the proper record_ids
   * 2) HeapFileScanner->getNext() initializes its given record pointer data
   *    correctly
   * 3) HeapFileScanner->getNext() returns INVALID_RECORD_ID on its third call
   * 4) The state of the HeapFile is unaffected by the use of HeapFileScanner.
   */
  TEST_FIXTURE(TestFixture, HeapFileScanner1) {
    // RecordIds for the full and free pages (set after insertRecord())
    RecordId record_id_free;
    RecordId record_id_full;

    std::cout << "check HeapFileScanner can get recs from both lists\n";
    // create and insert a 1-byte record into the HeapFile
    data->setSize(1);
    memset(data->getData(), 1, 1);
    record_id_free = file->insertRecord(record);

    // create and insert a max size record into the HeapFile
    // Init data to unique value
    data->setSize(MAX_RECORD_SIZE);
    memset(data->getData(), 2, MAX_RECORD_SIZE);
    record_id_full = file->insertRecord(record);

    // Check HeapFile state (not check of scanner correctness)
    checkHeader(record_id_full.page_num, record_id_free.page_num, 1, 1, 2);

    // create HeapFileScanner object and related varaibles for checking
    RecordId scanner_id;
    int free = 0;
    int full = 0;
    char* temp = new char[MAX_RECORD_SIZE];
    Data* scanner_data = new Data(PAGE_SIZE);
    Record* scanner_record = new Record(nullptr, scanner_data);
    HeapFileScanner* scanner = new HeapFileScanner(file);

    // Perform getNext() twice on the HeapFileScanner, and check that each 
    // recordId is returned, and that the recordId is correct.
    // Use int free and int full to ensure we don't get duplicate returns from
    // HeapFileScanner->getNext()
    for(int i=0; i<2; i++){
      scanner_id = scanner->getNext(scanner_record);
      CHECK((scanner_id==record_id_free) || (scanner_id==record_id_full));
      if(scanner_id==record_id_free){
        CHECK(free==0);
        free++;
        memset(temp, 1, 1);
        CHECK(!memcmp(scanner_data->getData(), temp, 1));
      }
      else{
        CHECK(full==0);
        full++;
        memset(temp, 2, MAX_RECORD_SIZE);
        CHECK(!memcmp(scanner_data->getData(), temp, MAX_RECORD_SIZE));
      }
    }

    //check that the next returned record id is INVALID_RECORD_ID
    scanner_id = scanner->getNext(scanner_record);
    //NOTE: for some reason, CHECK_EQUAL doesn't work here
    CHECK(scanner_id==INVALID_RECORD_ID);

    //check that the header state is unaffected
    checkHeader(record_id_full.page_num, record_id_free.page_num, 1, 1, 2);
    //check that no page is pinned
    CHECK_EQUAL(0, buf_mgr->getBufferState().pinned);

    //clean up
    delete[] temp;
    delete scanner;
    delete scanner_data;
    delete scanner_record;
  }

  /*
   * Check that scanner returns all RecordIds of records in the HeapFile when
   * there are multiple records in each page and there are multiple pages in
   * free and full list. Inserts records of size ranging from 1 to
   * min(MAX_PAGE_NUM, MAX_RECORD_SIZE). Scans the entire HeapFile and checks
   * if returned RecordId and record data are consistent. Also checks that
   * the scanner returns INVALID_RECORD_ID at the end.
   */
  TEST_FIXTURE(TestFixture, HeapFileScanner2) {
    std::unordered_map<RecordId, std::uint32_t, RecordHash>record_ids;
    HeapFileScanner* scanner;
    RecordId cur_rid;
    std::uint32_t cur_data;
    char* temp = new char[MAX_RECORD_SIZE];
    //min(MAX_RECORD_SIZE, MAX_PAGE_NUM)
    std::uint32_t num = (MAX_RECORD_SIZE < MAX_PAGE_NUM) ? MAX_RECORD_SIZE :
        MAX_PAGE_NUM;

    std::cout << "check HeapFileScanner gets all records in file w/many"
      << "recs/page and many pages on each list\n";
    //insert records of varying size, increasing from 1 to num
    for (std::uint32_t i=1; i < num; i++){
      data->setSize(i);
      memset(data->getData(), i%128, i);
      record_ids[file->insertRecord(record)] = i;
    }

    scanner = new HeapFileScanner(file);
    for (std::uint32_t i=0; i < num-1; i++){
      cur_rid = scanner->getNext(&record);
      cur_data = record_ids[cur_rid];
      memset(temp, cur_data%128, cur_data);
      //check data
      CHECK(!memcmp(temp,data->getData(),cur_data));
      CHECK_EQUAL(cur_data, data->getSize());
      //reset data
      data->setSize(0);
      memset(data->getData(), 0, MAX_RECORD_SIZE);
      //remove scanned record from map
      record_ids.erase(cur_rid);
    }

    //check if the end of the scanner is reached
    CHECK(scanner->getNext(&record) == INVALID_RECORD_ID);
    //check if all records are scanned.
    CHECK(record_ids.empty());
    //check pin count
    CHECK_EQUAL(0, buf_mgr->getBufferState().pinned);
    delete scanner;
    delete[] temp;
  }

  
/*
 * Prints usage
 */
void usage(){
  std::cout << "Usage: ./unittestsf -s <suite_name> -h help\n";
  std::cout << "Available Suites: " 
    << "HeapFileScanner, mytests" << std::endl;
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
