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
    ZERO_ASSERT(rm->createTable(t1, t1_attrs));
    cout << "PASS: createTable(" << output_schema(t2, t2_attrs) << ")" << endl;
    ZERO_ASSERT(rm->deleteTable(t2));
    cout << "PASS: deleteTable(" << t2 << ")" << endl;

    /* empty schema. */
    /* schema size tests */

} // }}}

unsigned int read_first_x_bytes_as_int32(const unsigned int x,const char* s)
{
	unsigned int int32 = 0;
	for (unsigned int i = (x-1); i > 0; i--)
		int32 = ( (int32 | (s[i]&0xFF) ) << bitsInByte );
	return int32 = (int32 | ((s[0]) & 0xFF));
}

void next_field(char *&iter)
{
	iter += bytesPerOffset;
}
void print_record( char* record )
{
	char* iter = record;
	unsigned int count = read_first_x_bytes_as_int32(bytesPerOffset, iter);
	cout << "Field Count: " << count << endl;
	next_field(iter);
	for (unsigned int i = 0; i < count; i++)
	{
		int field_start = read_first_x_bytes_as_int32(bytesPerOffset, iter);
		int field_end   = read_first_x_bytes_as_int32(bytesPerOffset, iter+1);
		cout << "FO_" << i << ": " << field_start << endl;
		cout << "F_" << i << ": " << read_first_x_bytes_as_int32(field_end - field_start, record+field_start) << endl;
		next_field(iter);
	}
	cout << "END_AT: " << read_first_x_bytes_as_int32(bytesPerOffset, iter) << endl;
	next_field(iter);
}
void testRecWrite()
{
	RM *rm = RM::Instance();
	vector<Attribute> attrs;
	vector<Attribute> attrs2;
	attrs.push_back((struct Attribute) { "a1", TypeInt, 4 });
	attrs.push_back((struct Attribute) { "a2", TypeReal, 4 });
	attrs.push_back((struct Attribute) { "a3", TypeVarChar, 10 });

	char test_chars[28];
	memset( test_chars, 0, 28 * sizeof(char) );
	rm->produceHeader(attrs, test_chars);
	print_record( test_chars );

	rm->createTable("tableTest", attrs);
	rm->getAttributes("tableTest", attrs2);

	cout << "ATRR2 " << attrs2.size() << endl ;
}

void rmTest()
{
    RM *rm = RM::Instance();
	testRecWrite();

    // write your own testing cases here
    //rmTest_SystemCatalog(rm);
}

int main()
{
    cout << "test..." << endl;

    rmTest();
    // other tests go here

    cout << "OK" << endl;
}
