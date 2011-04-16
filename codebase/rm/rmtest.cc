#include <fstream>
#include <iostream>
#include <cassert>

#include "rm.h"

#define FIELD_OFFSET_SIZE 2
#define ATTR_INT_SIZE 4
#define ATTR_REAL_SIZE 4

#define ZERO_ASSERT(x) assert((x) == 0)
#define NONZERO_ASSERT(x) assert((x) != 0)

using namespace std;

unsigned int compute_schema_size(vector<Attribute> &attrs)
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

    for(int i = 0; i < attrs.size(); i++)
    {
        switch(attrs[i].type)
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
}

void rmTest_SystemCatalog() // {{{
{
    const char *t1 = "table1";
    vector<Attribute> t1_attrs;

    const char *t2 = "table2";
    vector<Attribute> t2_attrs;

    const char *t3 = "table3";
    vector<Attribute> t3_attrs;

    const char *t4 = "table4";
    vector<Attribute> t4_attrs;

    t1_attrs.push_back((struct Attribute) { "test", TypeInt, 0 });

    t2_attrs.push_back((struct Attribute) { "test", TypeInt, 0 });
    t2_attrs.push_back((struct Attribute) { "test1", TypeReal, 0 });
    t2_attrs.push_back((struct Attribute) { "test2", TypeVarChar, 500 });
    t2_attrs.push_back((struct Attribute) { "test3", TypeVarChar, 1 });

    cout << "sizeof(t1_attrs) == " << compute_schema_size(t1_attrs) << endl;
    cout << "sizeof(t2_attrs) == " << compute_schema_size(t2_attrs) << endl;

    /* Structure of record: 1 byte for #fields, 2 bytes for field offset. */

    /*
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
    
    
    
} // }}}

void rmTest()
{
    RM *rm = RM::Instance();

    // write your own testing cases here
    rmTest_SystemCatalog();
}

int main() 
{
    cout << "test..." << endl;

    rmTest();
    // other tests go here

    cout << "OK" << endl;
}
