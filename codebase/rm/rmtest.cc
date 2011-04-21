#include <fstream>
#include <iostream>
#include <cassert>
#include <sstream>
#include <vector>
#include <string.h>

#include "rm.h"

#define FIELD_OFFSET_SIZE 2
#define ATTR_INT_SIZE 4
#define ATTR_REAL_SIZE 4

// XXX: toggle when complete
#define ZERO_ASSERT(x) assert((x) == 0)
#define NONZERO_ASSERT(x) assert((x) != 0)

//#define ZERO_ASSERT(x) ((x) == 0);
//#define NONZERO_ASSERT(x) ((x) == 1);

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

    /* no schema. */
    string t_noschema = "t_noschema";
    vector<Attribute> t_noschema_attrs;

    /* duplicate attribute name schema */
    string t_duplicate = "t_duplicate";
    vector<Attribute> t_duplicate_attrs;
    t_duplicate_attrs.push_back((struct Attribute) { "a0", TypeInt, 0 });
    t_duplicate_attrs.push_back((struct Attribute) { "a0", TypeReal, 0 });

    string t_duplicate2 = "t_duplicate2";
    vector<Attribute> t_duplicate2_attrs;
    t_duplicate2_attrs.push_back((struct Attribute) { "a0", TypeInt, 0 });
    t_duplicate2_attrs.push_back((struct Attribute) { "a0", TypeInt, 0 });

    /* empty attribute schema */
    string t_empty = "t_empty";
    vector<Attribute> t_empty_attrs;
    t_empty_attrs.push_back((struct Attribute) { "a0", TypeVarChar, 0 });

    string t_empty2 = "t_empty2";
    vector<Attribute> t_empty2_attrs;
    t_empty2_attrs.push_back((struct Attribute) { "a0", TypeInt, 0 });
    t_empty2_attrs.push_back((struct Attribute) { "a1", TypeVarChar, 0 });

    /* data overflow schema */
    string t_overflow1 = "t_overflow1";
    vector<Attribute> t_overflow1_attrs;
    t_overflow1_attrs.push_back((struct Attribute) { "a1", TypeVarChar, 4093 });

    string t_overflow2 = "t_overflow2";
    vector<Attribute> t_overflow2_attrs;
    /* 2 + 2 + 4 = 8 bytes for field count and first field. */
    t_overflow2_attrs.push_back((struct Attribute) { "a0", TypeVarChar, 4 });

    /* fill attributes for a total of 4089 bytes (excluding 2 byte field count)*/
    for (int i = 1; i <= 1363; i++)
    {
        stringstream ss;
        ss << "a" << i;
        t_overflow2_attrs.push_back((struct Attribute) { ss.str(), TypeVarChar, 1 });
    }

    string t_exact1 = "t_exact1";
    vector<Attribute> t_exact1_attrs;
    /* 2 + 2 + 4092 = 4096 */
    t_exact1_attrs.push_back((struct Attribute) { "a1", TypeVarChar, 4092 });

    string t_exact2 = "t_exact2";
    vector<Attribute> t_exact2_attrs;
    /* 2 + 2 + 3 = 7 bytes for field count and first field. */
    t_exact2_attrs.push_back((struct Attribute) { "a0", TypeVarChar, 3 });

    /* fill attributes for a total of 4089 bytes (excluding 2 byte field count)*/
    for (int i = 1; i <= 1363; i++)
    {
        stringstream ss;
        ss << "a" << i;
        t_exact2_attrs.push_back((struct Attribute) { ss.str(), TypeVarChar, 1 });
    }

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

    /* table creation/deletion test. */ // {{{

    cout << "\n[ table creation/deletion test. ]" << endl;
    ZERO_ASSERT(rm->createTable(t1, t1_attrs));
    cout << "PASS: createTable(" << output_schema(t1, t1_attrs) << ")" << endl;

    ZERO_ASSERT(rm->deleteTable(t1));
    cout << "PASS: deleteTable(" << t1 << ")" << endl;

    // }}}

    /* duplicate create table test. */ // {{{
    cout << "\n[ duplicate table creation. ]" << endl;

    ZERO_ASSERT(rm->createTable(t1, t1_attrs));
    cout << "PASS: createTable(" << output_schema(t1, t1_attrs) << ")" << endl;

    NONZERO_ASSERT(rm->createTable(t1, t1_attrs));
    cout << "PASS: createTable(" << output_schema(t1, t1_attrs) << ") [duplicate]" << endl;
    // }}}

    /* duplicate table destroy test. */ // {{{
    cout << "\n[ duplicate table destroy. ]" << endl;

    ZERO_ASSERT(rm->deleteTable(t1));
    cout << "PASS: deleteTable(" << t1 << ")" << endl;

    NONZERO_ASSERT(rm->deleteTable(t1));
    cout << "PASS: deleteTable(" << t1 << ") [doesnt exist]" << endl;
    // }}}

    /* nonexistent table name destroy test. */ // {{{
    cout << "\n[ nonexistent table in namespace. ]" << endl;

    NONZERO_ASSERT(rm->deleteTable(t2));
    cout << "PASS: deleteTable(" << t2 << ") [doesnt exist]" << endl;
    // }}}

    /* duplicate attributes in schema. */ // {{{
    cout << "\n[ duplicate attribute name in schema. ]" << endl;

    NONZERO_ASSERT(rm->createTable(t_duplicate, t_duplicate_attrs));
    cout << "PASS: createTable(" << output_schema(t_duplicate, t_duplicate_attrs) << ") [duplicate attribute name]" << endl;
    NONZERO_ASSERT(rm->createTable(t_duplicate2, t_duplicate2_attrs));
    cout << "PASS: createTable(" << output_schema(t_duplicate2, t_duplicate2_attrs) << ") [duplicate attributes]" << endl;
    // }}}

    /* empty varchar attributes in schema. */ // {{{
    cout << "\n[ empty varchar attribute name in schema. ]" << endl;

    NONZERO_ASSERT(rm->createTable(t_empty, t_empty_attrs));
    cout << "PASS: createTable(" << output_schema(t_empty, t_empty_attrs) << ") [empty varchar]" << endl;

    NONZERO_ASSERT(rm->createTable(t_empty2, t_empty2_attrs));
    cout << "PASS: createTable(" << output_schema(t_empty2, t_empty2_attrs) << ") [empty varchar]" << endl;
    // }}}

    /* invalid table in namespace */ // {{{
    cout << "\n[ invalid table in namespace. ]" << endl;

    NONZERO_ASSERT(rm->createTable(t_invalid_name1, t_invalid_attrs));
    cout << "PASS: createTable(" << output_schema(t_invalid_name1, t_invalid_attrs) << ") [invalid name]" << endl;

    NONZERO_ASSERT(rm->createTable(t_invalid_name2, t_invalid_attrs));
    cout << "PASS: createTable(" << output_schema(t_invalid_name2, t_invalid_attrs) << ") [invalid name]" << endl;

    NONZERO_ASSERT(rm->createTable(t_invalid_name3, t_invalid_attrs));
    cout << "PASS: createTable(" << output_schema(t_invalid_name3, t_invalid_attrs) << ") [invalid name]" << endl;
    // }}}

    /* empty schema. */ // {{{
    cout << "\n[ no schema test. ]" << endl;

    NONZERO_ASSERT(rm->createTable(t_noschema, t_noschema_attrs));
    cout << "PASS: createTable(" << output_schema(t_noschema, t_noschema_attrs) << ") [no schema]" << endl;
    // }}}

    /* correct attribute retrieval test. */ // {{{
    cout << "\n[ correct attribute retrieval test. ]" << endl;

    ZERO_ASSERT(rm->createTable(t1, t1_attrs));
    cout << "PASS: createTable(" << output_schema(t1, t1_attrs) << ")" << endl;

    ZERO_ASSERT(rm->getAttributes(t1, aux_attrs));
    assert(aux_attrs == t1_attrs);
    aux_attrs.clear();
    cout << "PASS: getAttributes(" << t1 << ")" << endl;

    ZERO_ASSERT(rm->deleteTable(t1));
    cout << "PASS: deleteTable(" << t1 << ")" << endl;

    ZERO_ASSERT(rm->createTable(t2, t2_attrs));
    cout << "PASS: createTable(" << output_schema(t2, t2_attrs) << ")" << endl;

    ZERO_ASSERT(rm->getAttributes(t2, aux_attrs));
    assert(aux_attrs == t2_attrs);
    aux_attrs.clear();
    cout << "PASS: getAttributes(" << t2 << ")" << endl;

    ZERO_ASSERT(rm->deleteTable(t2));
    cout << "PASS: deleteTable(" << t2 << ")" << endl;
    // }}}

    /* non-existent table attribute retrieval test. */ // {{{
    cout << "\n[ non-existent table attribute retrieval test. ]" << endl;

    NONZERO_ASSERT(rm->getAttributes(t1, aux_attrs));
    aux_attrs.clear();
    cout << "PASS: getAttributes(" << t1 << ") [table doesnt exist]" << endl;

    // }}}
  
    /* schema size tests */ // {{{
    cout << "\n[ schema size tests ]" << endl;

    ZERO_ASSERT(rm->createTable(t_exact1, t_exact1_attrs));
    assert(rm->getSchemaSize(t_exact1_attrs) <= PF_PAGE_SIZE);
    cout << "PASS: createTable(" << output_schema(t_exact1, t_exact1_attrs)
         << ") [size=" << rm->getSchemaSize(t_exact1_attrs) << "]" << endl;

    ZERO_ASSERT(rm->createTable(t_exact2, t_exact2_attrs));
    assert(rm->getSchemaSize(t_exact2_attrs) <= PF_PAGE_SIZE);
    cout << "PASS: createTable(" << output_schema(t_exact2, t_exact2_attrs).substr(0, 60)+"..."
         << ") [size=" << rm->getSchemaSize(t_exact2_attrs) << "]" << endl;

    NONZERO_ASSERT(rm->createTable(t_overflow1, t_overflow1_attrs));
    assert(rm->getSchemaSize(t_overflow1_attrs) > PF_PAGE_SIZE);
    cout << "PASS: createTable(" << output_schema(t_overflow1, t_overflow1_attrs)
         << ") [overflow, size=" << rm->getSchemaSize(t_overflow1_attrs) << "]" << endl;

    NONZERO_ASSERT(rm->createTable(t_overflow2, t_overflow2_attrs));
    assert(rm->getSchemaSize(t_overflow2_attrs) > PF_PAGE_SIZE);
    cout << "PASS: createTable(" << output_schema(t_overflow2, t_overflow2_attrs).substr(0,60)+"..."
          << ") [overflow, size=" << rm->getSchemaSize(t_overflow2_attrs) << "]" << endl;

    ZERO_ASSERT(rm->deleteTable(t_exact1));
    cout << "PASS: deleteTable(" << t2 << ")" << endl;

    ZERO_ASSERT(rm->deleteTable(t_exact2));
    cout << "PASS: deleteTable(" << t2 << ")" << endl;
    // }}}

} // }}}

void rmTest_PageMgmt(RM *rm) // {{{
{
    string t1 = "t1";
    vector<Attribute> t1_attrs;
    t1_attrs.push_back((struct Attribute) { "a1", TypeInt, 0 });

    // test 1: create table, expect 1 control page, no data pages.
    /* table creation/deletion test. */ // {{{

    cout << "\n[ table creation ]" << endl;
    ZERO_ASSERT(rm->createTable(t1, t1_attrs));
    cout << "PASS: createTable(" << output_schema(t1, t1_attrs) << ")" << endl;

    ZERO_ASSERT(rm->deleteTable(t1));
    cout << "PASS: deleteTable(" << t1 << ")" << endl;

    // }}}

} // }}}

void rmTest_TableMgmt(RM *rm) // {{{
{
    //RID r1;

    //string t1 = "t1";
    //vector<Attribute> t1_attrs;
    //t1_attrs.push_back((struct Attribute) { "a1", TypeInt, 0 });

    /*
        //  Format of the data passed into the function is the following:
        //  1) data is a concatenation of values of the attributes
        //  2) For int and real: use 4 bytes to store the value;
        //     For varchar: use 4 bytes to store the length of characters, then store the actual characters.
        //  !!!The same format is used for updateTuple(), the returned data of readTuple(), and readAttribute()

        RC insertTuple(const string tableName, const void *data, RID &rid);
    */

} // }}}

void rmTest()
{
    RM *rm = RM::Instance();

    // write your own testing cases here
    cout << "System Catalogue (createTable, deleteTable, getAttributes) tests: " << endl << endl;
    rmTest_SystemCatalog(rm);
    rmTest_PageMgmt(rm);
    rmTest_TableMgmt(rm);
}

int main()
{
    cout << "test..." << endl;

    rmTest();
    // other tests go here

    cout << "OK" << endl;
}
