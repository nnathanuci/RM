
#ifndef _rm_h_
#define _rm_h_

#include <string>
#include <vector>
#include <ostream>
#include <iostream>
#include <map>
#include <climits>

#include "../pf/pf.h"

using namespace std;

// user-defined:

#define REC_BITS_PER_OFFSET (sizeof(uint16_t)*CHAR_BIT)

/* returns the relative offset where data begins in a record. */
#define REC_START_DATA_OFFSET(n_fields) (sizeof(uint16_t)*(n_fields) + sizeof(uint16_t))

/* returns relative offset for where the end offset for the i-th field is stored. */
#define REC_FIELD_OFFSET(i) (sizeof(uint16_t)*(i) + sizeof(uint16_t))

/* following macro finds the record length given an offset:
   1. (*((short *) start)) - 1 == index offset of last field (2+(number of fields-1))
   2. start + FIELD_OFFSET(index of last field) positions pointer to the end offset of last field.
   3. Read in the value to find the end offset value, which is length of the record.
*/

#define REC_LENGTH(start) (*((uint16_t *) ((char *) (start) + (REC_FIELD_OFFSET((*((uint16_t *) (start)))-1)))))

/* determines number of page offsets stored in a control page. */
#define CTRL_MAX_PAGES ((PF_PAGE_SIZE*CHAR_BIT)/REC_BITS_PER_OFFSET)

/* number of pages under control for a given control page, plus the control page itself. */
#define CTRL_BLOCK_SIZE (1+CTRL_MAX_PAGES)

/* given the total number of pages in a file, determine the number of control and data pages.

   An example layout of control and data pages:

   [C] [D * CTRL_MAX_PAGES] [C] [D * CTRL_MAX_PAGES] [C] [D]
   | CTRL_BLOCK_SIZE    | | CTRL_BLOCK_SIZE    | [C] [D]

   Total number of pages: CTRL_BLOCK_SIZE*2 + 2 pages.
   The number of control pages: 3

   To determine number of control pages: (total_num_pages / CTRL_BLOCK_SIZE) + 1 = 3

   To determine number of data pages: (total_num_pages - num_control_pages) = (CTRL_BLOCK_SIZE*2 - 2) + 1
*/

#define CTRL_NUM_CTRL_PAGES(num_pages) (((num_pages) <= CTRL_BLOCK_SIZE) ? 1 : (((num_pages) / CTRL_BLOCK_SIZE) + 1))

#define CTRL_NUM_DATA_PAGES(num_pages) ((num_pages) - CTRL_NUM_CTRL_PAGES((num_pages)))

/* return the page id associated with the i-th control page. */
#define CTRL_PAGE_ID(i) ((i)*CTRL_BLOCK_SIZE)

/* maximum available space after allocating space for control fields, and allocation of one empty slot. */
#define SLOT_MAX_SPACE (PF_PAGE_SIZE - sizeof(uint16_t)*4)

/* slot stuff. */
#define SLOT_MIN_METADATA_SIZE (sizeof(uint16_t)*4)
#define NEXT_SLOT_INDEX ((PF_PAGE_SIZE/2) - 2)
#define SLOT_QUEUE_HEAD ((PF_PAGE_SIZE/2) - 2)
#define NUM_SLOT_INDEX ((PF_PAGE_SIZE/2) - 3)
#define GET_SLOT_INDEX(i) ((PF_PAGE_SIZE/2) - 4 - (i))
#define SLOT_QUEUE_END (4095)




// Return code
typedef int RC;


// Record ID
typedef struct RID
{
  unsigned pageNum;
  unsigned slotNum;
} RID;

// Attribute
typedef enum { TypeInt = 0, TypeReal, TypeVarChar } AttrType;

typedef unsigned AttrLength;

typedef struct Attribute {
    string   name;     // attribute name
    AttrType type;     // attribute type
    AttrLength length; // attribute length

    int operator==(const Attribute &rhs) const
    {
        return((name == rhs.name) && (type == rhs.type) && (length == rhs.length));
    }
} Attribute;


// Comparison Operator
typedef enum { EQ_OP = 0,  // =
           LT_OP,      // <
           GT_OP,      // >
           LE_OP,      // <=
           GE_OP,      // >=
           NE_OP,      // !=
           NO_OP       // no condition
} CompOp;


# define RM_EOF (-1)  // end of a scan operator

// RM_ScanIterator is an iterator to go through records
// The way to use it is like the following:
//  RM_ScanIterator rmScanIterator;
//  rm.open(..., rmScanIterator);
//  while (rmScanIterator(rid, data) != RM_EOF) {
//    process the data;
//  }
//  rmScanIterator.close();

class RM_ScanIterator {
public:
  RM_ScanIterator() {};
  ~RM_ScanIterator() {};

  // "data" follows the same format as RM::insertTuple()
  RC getNextTuple(RID &rid, void *data) { return RM_EOF; };
  RC close() { return -1; };
};


// Record Manager
class RM
{
public:
  static RM* Instance();

  RC createTable(const string tableName, const vector<Attribute> &attrs);

  RC deleteTable(const string tableName);

  RC getAttributes(const string tableName, vector<Attribute> &attrs);

  //  Format of the data passed into the function is the following:
  //  1) data is a concatenation of values of the attributes
  //  2) For int and real: use 4 bytes to store the value;
  //     For varchar: use 4 bytes to store the length of characters, then store the actual characters.
  //  !!!The same format is used for updateTuple(), the returned data of readTuple(), and readAttribute()
  RC insertTuple(const string tableName, const void *data, RID &rid);

  RC deleteTuples(const string tableName);

  RC deleteTuple(const string tableName, const RID &rid);

  // Assume the rid does not change after update
  RC updateTuple(const string tableName, const void *data, const RID &rid);

  RC readTuple(const string tableName, const RID &rid, void *data);

  RC readAttribute(const string tableName, const RID &rid, const string attributeName, void *data);

  RC reorganizePage(const string tableName, const unsigned pageNumber);

  // scan returns an iterator to allow the caller to go through the results one by one.
  RC scan(const string tableName, 
      const vector<string> &attributeNames, // a list of projected attributes
      RM_ScanIterator &rm_ScanIterator);


// Extra credit
public:
  RC dropAttribute(const string tableName, const string attributeName);

  RC addAttribute(const string tableName, const Attribute attr);

  RC reorganizeTable(const string tableName);

  // scan returns an iterator to allow the caller to go through the results one by one. 
  RC scan(const string tableName,
      const string conditionAttribute,
      const CompOp compOp,                  // comparision type such as "<" and "="
      const void *value,                    // used in the comparison
      const vector<string> &attributeNames, // a list of projected attributes
      RM_ScanIterator &rm_ScanIterator);

protected:
  RM();
  ~RM();

private:
  static RM *_rm;

// user-defined:

public:
  RC produceHeader(const vector<Attribute> &attrs, char*);

  unsigned getSchemaSize(const vector<Attribute> &attrs);

  /* find page with available space given the requested length. (public for performing tests.) */
  RC getFreePage(PF_FileHandle &fileHandle, uint16_t length, unsigned int &page_id, uint16_t &unused_space);

  /* interface to open_tables map. */
  RC openTable(const string tableName, PF_FileHandle &fileHandle);

  /* interface to open_tables map to close tables; used by deleteTable(). */
  RC closeTable(const string tableName);

  /* close all tables in open_tables. */
  RC closeAllTables();

private:

  /* allocate & append control page to a given database file. */
  RC AllocateControlPage(PF_FileHandle &fileHandle);

  /* allocate & append a blank page to a given database file. */
  RC AllocateDataPage(PF_FileHandle &fileHandle);

  /* auxillary functions for insertTuple and readTuple. */
  void tuple_to_record(const void *tuple, char *record, const vector<Attribute> &attrs);
  void record_to_tuple(char *record, const void *tuple, const vector<Attribute> &attrs);

  PF_Manager *pf;
  map<string, vector<Attribute> > catalog;
  map<string, Attribute> catalog_fields;

  /* once a table is open, the file should persist. */
  map<string, PF_FileHandle> open_tables;
};

#endif
