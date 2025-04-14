#include <string>
#include <string.h>
#include <iostream>
#include <vector>
#include "swatdb_types.h"
#include "swatdb.h"
#include "catalog.h"
#include "schema.h"
#include "swatdb_exceptions.h"
#include "bufmgr.h"
#include "diskmgr.h"
#include "filemgr.h"
#include "page.h"
#include "record.h"
#include "data.h"
#include "heappagescanner.h" 
#include "heapfile.h"
#include "heapfilescanner.h"
#include "hashindexfile.h"


/* testing: max record size */
const uint32_t SWATDB_TEST_REC_MAX = 128;
/* testing: relation name */
const char *  SWATDB_TEST_REL_NAME = "Rel1";

/* create a new DB 
 * if save_on_exit is true, save its state on exit
 * otherwise don't 
 * infile: name of db file
 * num_recs: number of records to insert
 */
static void create_emptyDB(bool save_on_exit, std::string infile, 
    std::uint32_t num_recs); 

/* create and from a saved DB metadata file
 * and save on exit if save_on_exit is true
 * infile: name of db file
 * num_recs: number new records to insert
 */
static void create_from_savedDB(bool save_on_exit,
    std::string infile, std::string save_file, std::uint32_t num_recs) ;

// helper functions for test code
static FileId createFile(FileManager *f_mgr);  
static void print_fids(Catalog *mycat) ;
static void print_file_stats(SwatDB *db, FileId fid) ;
/* inserts some recs into file (there is no guarentee records are unique) */
static void add_some_records(SwatDB *db, FileId fid,  std::uint32_t num);

/* prints out some number of records in the file
 * pass 0 for num to print all records in the file */
static void scan_and_print_some_records(SwatDB *db, FileId fid, 
    std::uint32_t num);


/**************************************************************
 * control flow for a swateDB test program 
 *   (1) boots the system: creates the main SwatDB object
 *   (2) does some stuff with it: this is where test code or 
 *       a a user interface could sit
 *   (3) shutdown saving state of DB or not to metadata file
 */
int main(int argc, char *argv[]) {

  // test saving metadata on exit
  std::cout << "\nCreate Empty save\n";
  std::cout << "------------------\n";
  create_emptyDB(true, std::string("saved.db"), 10);


  // test creating a DB from saved metadata
  std::cout << "\nCreate From Saved don't save\n";
  std::cout << "-----------------------------\n";
  create_from_savedDB(false, std::string("saved.db"), 
      std::string("shouldnotexist.db"), 10); 

  return 0;
}

/***********************************************
 * create an empty DB, create some files, shutdown
 * with or without saving  based on save_on_exit
 */
static void create_emptyDB(bool save_on_exit, std::string save_file,
    std::uint32_t num_recs) {

   SwatDB *mydb;
   FileId fid;
   Record rec;

   // this shows the general control flow for using
   // and initing swaDB

   // (1) creating an empty DB, and shutdown without saving
   mydb = new SwatDB("./");   // boot SwatDB: create an empty DB

   // (2) create some new relation files
   fid = createFile(mydb->getFileMgr());
   print_fids(mydb->getCatalog());

   // (3) do something with one of the relation files
   //     add some records, scan them, print out file info
   add_some_records(mydb, fid, num_recs);

   scan_and_print_some_records(mydb, fid, 5);
   // pass 0 to scan all records in file:
   // scan_and_print_some_records(mydb, fid, 0);
   
   print_file_stats(mydb, fid);

   // (4) shutdown, with or without saving the db
   std::cout << "relation files before shutdown:\n";
   print_fids(mydb->getCatalog());
   system("pwd");
   system("ls -l *.db *.rel");
   if(save_on_exit) {
     mydb->setSaveDB(std::string(save_file));
   }

   delete mydb;
   mydb = nullptr;
   std::cout << "relation files after shutdown:\n";
   system("pwd");
   system("ls -l *.db *.rel");

}

/***********************************************************
 * create a SwatDB instance from a saved .db file
 *   save_on_exit: if true save db to save_file on exit, o/w delete the DB
 *   infile: the input .db file to init SwatDB
 *   num_records: the number of records to add to the DB
 */
static void create_from_savedDB(bool save_on_exit,
    std::string infile, std::string save_file, std::uint32_t num_recs) {

  SwatDB *mydb;
  FileId fid;
  Record rec;

  // this shows the general control flow for using
  // and initing swaDB

  // (1) creating an empty DB, and shutdown without saving
  mydb = new SwatDB(infile, "./");   // boot SwatDB: create an empty DB

  // (2) print some info about the DB
  print_fids(mydb->getCatalog());

  fid = mydb->getCatalog()->getFileId(SWATDB_TEST_REL_NAME);
  print_file_stats(mydb, fid);

  // (3) modify the DB: insert a few records
  add_some_records(mydb, fid, num_recs);

  // (4) print out the first few records
  scan_and_print_some_records(mydb, fid, 5);
  print_file_stats(mydb, fid);

  // (4) shutdown
  std::cout << "create_from_saved: relation files before shutdown:\n";
  print_fids(mydb->getCatalog());
  system("pwd");
  system("ls -l *.db *.rel");
  if(save_on_exit) {
    mydb->setSaveDB(save_file);
  }

  delete mydb;
  mydb = nullptr;
  std::cout << "create_from_saved: relation files after shutdown:\n";
  system("pwd");
  system("ls -l *.db *.rel");

}
/***********************************************
 * adds some records to a file (assumes HeapFile) 
 */
static void add_some_records(SwatDB *db, FileId fid,  std::uint32_t num) {

  HeapFile *file;
  Record rec;
  Data *data;
  char *buf;  // this record space is 
  std::uint32_t i;

  file = (HeapFile *)db->getCatalog()->getFile(fid);
  data = new Data(SWATDB_TEST_REC_MAX);
  buf = data->getData();
  rec.setSchema(db->getCatalog()->getSchema(fid));
  rec.setRecordData(data);
  data->setSize(SWATDB_TEST_REC_MAX);

  // insert some records  into the file
  for(i = 0; i < num; i++) {
    // there may be a better C++ way to do this but I don't know
    sprintf(buf, "%s%d", "Rec: ", i);
    file->insertRecord(rec);
  }

  delete data;
}

/***********************************************
 * print out some size information of the file 
 * fid: file's FileId
 */
static void print_file_stats(SwatDB *db, FileId fid) {
    HeapFile *file;

    file = (HeapFile *)db->getCatalog()->getFile(fid);

    printf("---------- file info: --------\n");
    printf("num pages:       %u\n", file->getNumPages());
    printf("num full pages:  %u\n", file->getNumFullPages());
    printf("num records:     %lu\n\n", file->getNumRecords());

}

/***********************************************
 * scans and prints out all up to num records in a file
 * if num is zero prints out all records in the file
 */
static void scan_and_print_some_records(SwatDB *db, FileId fid, 
    std::uint32_t num) 
{

  HeapFile *file;
  HeapFileScanner *scanner;
  Record rec;
  Data *data;
  char *buf;  // this record space is 
  std::uint32_t i;

  file = (HeapFile *)db->getCatalog()->getFile(fid);
  data = new Data(SWATDB_TEST_REC_MAX);
  buf = data->getData();
  rec.setSchema(db->getCatalog()->getSchema(fid));
  rec.setRecordData(data);

  scanner = new HeapFileScanner(file);

  i = 0;
  std::cout << "Scanning file " << fid << " first " << num << "recs\n";
  while(scanner->getNext(&rec) != INVALID_RECORD_ID) {
    std::cout << "record " << i << ": " << buf << std::endl;
    i++;
    if(num && (i > num)) { break; }
  }

  delete data;
  delete scanner;
}
/***********************************************
 * prints out catalog state  for all valid entries
 */
static void print_fids(Catalog *mycat) {
   std::string filename;
   std::size_t i;
   std::vector<FileId> ids;

   // get fileIds
   ids = mycat->getFileIds();
   std::cout << " there are " << ids.size() << " FileIds"  <<  std::endl;
   for(i = 0; i < ids.size(); i++) {
     std::cout << i << "th entry:  file ID is " << ids[i] <<  " "
       << mycat->getFileName(ids[i]) <<  std::endl;
     mycat->getSchema(ids[i])->printFields();

   }
   
}
/**************************************************
 * Helper Function: creates a HeapFile to use for testing 
 * @returns one valid FileID in the system
 */
static FileId createFile(FileManager *file_mgr) {

  FileId file_id;

  try {
    std::vector<FieldEntry> field_list = {{"name", StringFieldT, 256},
                                          {"year", IntFieldT, 4},
                                          {"gpa", FloatFieldT, 4}};
    std::vector<std::string> primary_key;
    Schema *schema = new Schema(field_list, primary_key); 
    file_id = file_mgr->createRelation(std::string(SWATDB_TEST_REL_NAME), 
        schema, HeapFileT, std::string("testrel1.rel"));
  } catch(SwatDBException& e){
    throw std::runtime_error("Something is messed up with createFile\n");
  }
  return file_id;
}

