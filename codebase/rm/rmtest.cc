#include <fstream>
#include <iostream>
#include <cassert>
#include <sstream>
#include <vector>

#include "rm.h"

#define FIELD_OFFSET_SIZE 2
#define ATTR_INT_SIZE 4
#define ATTR_REAL_SIZE 4

// XXX: toggle when complete
//#define ZERO_ASSERT(x) assert((x) == 0)
//#define NONZERO_ASSERT(x) assert((x) != 0)

#define ZERO_ASSERT(x) ((x) == 0);
#define NONZERO_ASSERT(x) ((x) == 1);

using namespace std;

bool cmp_attrs(const vector<Attribute> &lhs, const vector<Attribute> &rhs)
{
    if(lhs.size() != rhs.size())
        return false;

    for(unsigned int i = 0; i < lhs.size(); i++)
    {
        if(lhs[i].name != rhs[i].name)
            return false;

        if(lhs[i].type != rhs[i].type)
            return false;

        if(lhs[i].length != rhs[i].length)
            return false;
    }

    return true;
}

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

    string t2 = "t2";
    vector<Attribute> t2_attrs;
    t2_attrs.push_back((struct Attribute) { "a1", TypeInt, 0 });
    t2_attrs.push_back((struct Attribute) { "a2", TypeReal, 0 });
    t2_attrs.push_back((struct Attribute) { "a3", TypeVarChar, 500 });

    /* invalid in table namespace */
    string t_invalid_name1 = "invalid.table/name";
    string t_invalid_name2 = "invalid.table.name";
    string t_invalid_name3 = "invalid/table.name";
    vector<Attribute> t_invalid_attrs;
    t_invalid_attrs.push_back((struct Attribute) { "a1", TypeInt, 0 });

    vector<Attribute> aux_attrs;

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
    
    /* duplicate create table test. */
    cout << "[ duplicate table creation. ]" << endl;
    ZERO_ASSERT(rm->createTable(t1, t1_attrs));
    cout << "PASS: createTable(" << output_schema(t1, t1_attrs) << ")" << endl;

    NONZERO_ASSERT(rm->createTable(t1, t1_attrs));
    cout << "PASS: createTable(" << output_schema(t1, t1_attrs) << ") [duplicate]" << endl;

    /* duplicate table destroy test. */
    cout << "[ duplicate table destroy. ]" << endl;
    ZERO_ASSERT(rm->deleteTable(t1));
    cout << "PASS: deleteTable(" << t1 << ")" << endl;

    NONZERO_ASSERT(rm->deleteTable(t1));
    cout << "PASS: deleteTable(" << t1 << ") [doesnt exist]" << endl;

    /* nonexistent table name destroy test. */
    cout << "[ nonexistent table in namespace. ]" << endl;
    NONZERO_ASSERT(rm->deleteTable(t2));
    cout << "PASS: deleteTable(" << t2 << ") [doesnt exist]" << endl;

    /* invalid table in namespace */
    cout << "[ invalid table in namespace. ]" << endl;
    NONZERO_ASSERT(rm->createTable(t_invalid_name1, t_invalid_attrs));
    cout << "PASS: createTable(" << output_schema(t_invalid_name1, t_invalid_attrs) << ") [invalid name]" << endl;

    NONZERO_ASSERT(rm->createTable(t_invalid_name2, t_invalid_attrs));
    cout << "PASS: createTable(" << output_schema(t_invalid_name2, t_invalid_attrs) << ") [invalid name]" << endl;

    NONZERO_ASSERT(rm->createTable(t_invalid_name3, t_invalid_attrs));
    cout << "PASS: createTable(" << output_schema(t_invalid_name3, t_invalid_attrs) << ") [invalid name]" << endl;

    /* correct attribute retrieval test. */
    cout << "[ correct attribute retrieval test. ]" << endl;
    ZERO_ASSERT(rm->createTable(t2, t2_attrs));
    cout << "PASS: createTable(" << output_schema(t2, t2_attrs) << ")" << endl;
    ZERO_ASSERT(rm->getAttributes(t2, aux_attrs));
    assert(cmp_attrs(aux_attrs, t2_attrs));
    aux_attrs.clear();
    cout << "PASS: getAttributes(" << t2 << ")" << endl;
    ZERO_ASSERT(rm->deleteTable(t2));
    cout << "PASS: deleteTable(" << t2 << ")" << endl;
  
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
