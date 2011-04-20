
#ifndef _rm_h_
#define _rm_h_

#include <string>
#include <vector>
#include <ostream>
#include <iostream>
#include <map>

#include "../pf/pf.h"

using namespace std;


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

// Defined by group 14

public:
  RC produceHeader(const vector<Attribute> &attrs, char*);

  unsigned getSchemaSize(const vector<Attribute> &attrs);

private:
  // interface to open_tables map.
  RC openTable(const string tableName, PF_FileHandle &fileHandle);

  PF_Manager *pf;
  map<string, vector<Attribute> > catalog;
  map<string, Attribute> catalog_fields;

  /* once a table is open, the file should persist. */
  map<string, PF_FileHandle> open_tables;
};

// Defined by group 14

/* returns the relative offset where data begins in a record. */
#define START_DATA_OFFSET(n_fields) (sizeof(unsigned short)*(n_fields) + sizeof(unsigned short))

/* returns relative offset for where the end offset for the i-th field is stored. */
#define FIELD_OFFSET(i) (sizeof(unsigned short)*(i) + sizeof(unsigned short))

/* following macro finds the record length given an offset:
   (*((short *) rec_offset)) - 1 == index offset of last field (2+(number of fields-1))
   record_offset + FIELD_OFFSET(index of last field) positions pointer to the end offset.
   The end offset value is the size of the record.
*/

typedef unsigned short rec_offset_t;

#define RECORD_LENGTH(rec_offset) (*((rec_offset_t *) ((char *) (rec_offset) + (FIELD_OFFSET((*((rec_offset_t *) (rec_offset)))-1)))))

static const unsigned int bytesPerInt = 4;
static const unsigned int bytesPerReal = 4;
static const unsigned int bytesPerOffset = 2;
static const unsigned int bitsInByte = 8;

#endif
