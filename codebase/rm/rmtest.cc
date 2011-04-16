#include <fstream>
#include <iostream>
#include <cassert>
#include <sstream>

#include "rm.h"

#define FIELD_OFFSET_SIZE 2
#define ATTR_INT_SIZE 4
#define ATTR_REAL_SIZE 4

//#define ZERO_ASSERT(x) assert((x) == 0)
//#define NONZERO_ASSERT(x) assert((x) != 0)

#define ZERO_ASSERT(x) ((x) == 0);
#define NONZERO_ASSERT(x) ((x) == 1);

using namespace std;

string output_schema(string table_name, vector<Attribute> &attrs) // {{{
{
    stringstream ss;
    ss << table_name;

    for (unsigned int i = 0; i < attrs.size(); i++)
    {
        ss << " " << attrs[i].name << ":";
        switch (attrs[i].type)
        { 
            case TypeInt:
                 ss << "Int";
                 break;

            case TypeReal:
                 ss << "Real";
                 break;

            case TypeVarChar:
                 ss << "VarChar(" << attrs[i].length << ")";
                 break;
        }
    }

    return ss.str();
} // }}}

unsigned int compute_schema_size(vector<Attribute> &attrs) // {{{
{
    unsigned int size = 1;

    /* typedef unsigned AttrLength;
       typedef enum { TypeInt = 0, TypeReal, TypeVarChar } AttrType;
          struct Attribute {
             string   name;     // attribute name
             AttrType type;     // attribute type
             AttrLength length; // attribute length
         };
    */

    size += attrs.size() * 2; /* field offset is 2 bytes, XXX: magic constant. */

    for (unsigned int i = 0; i < attrs.size(); i++)
    {
        switch (attrs[i].type)
        { 
            case TypeInt:
            case TypeReal:
                 size += 4;
                 break;

            case TypeVarChar:
                 size += attrs[i].length;
                 break;
        }
    }

    return size;
} // }}}

void rmTest_SystemCatalog(RM *rm) // {{{
{
    string t1 = "t1";
    vector<Attribute> t1_attrs;
    t1_attrs.push_back((struct Attribute) { "a1", TypeInt, 0 });

    /*
        [Structure of record: 1 byte for #fields, 2 bytes for field offset]

        Test cases for:
         RC createTable(const string tableName, const vector<Attribute> &attrs);
         RC deleteTable(const string tableName);
         RC getAttributes(const string tableName, vector<Attribute> &attrs);

         // Attribute
         typedef enum { TypeInt = 0, TypeReal, TypeVarChar } AttrType;
         
         typedef unsigned AttrLength;
         
         struct Attribute {
             string   name;     // attribute name
             AttrType type;     // attribute type
             AttrLength length; // attribute length
         };
    */
    
    /* duplicate table test. */
    ZERO_ASSERT(rm->createTable(t1, t1_attrs));
    cout << "PASS: createTable(" << output_schema(t1, t1_attrs) << ")" << endl;
    //NONZERO_ASSERT(rm->createTable(t1, t1_attrs));
    //cout << "PASS: createTable(" << t1; OUTPUT_SCHEMA(t1_attrs); cout << ") [error]" << endl;
    //NONZERO_ASSERT(pf->CreateTable(fn));
    //cout << "PASS: create file (already exists) [expecting error]" << endl;

    /* duplicate table destroy test. */
    /* invalid table name destroy test. */
    /* correct attribute retrieval test. */
    /* empty schema. */
    /* schema size tests */
    
} // }}}

void rmTest()
{
    RM *rm = RM::Instance();

    // write your own testing cases here
    rmTest_SystemCatalog(rm);
}

int main() 
{
    cout << "test..." << endl;

    rmTest();
    // other tests go here

    cout << "OK" << endl;
}
