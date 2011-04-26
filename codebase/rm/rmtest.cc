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

#define CTRL_GET_CTRL_PAGEID(pageid) ((pageid) / (CTRL_BLOCK_SIZE))
#define CTRL_GET_CTRL_PAGEOFFSET(pageid) ((pageid) % (CTRL_BLOCK_SIZE))

using namespace std;

bool verbose = false;

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

void rmTest_CleanUp()
{
    const char *files[9] = { "t1", "t2", "t_noschema", "t_duplicate", "t_duplicate2", "t_empty", "t_empty2", "t_overflow1", "t_overflow2" };

    for(int i = 0; i < 9; i++)
        remove(files[i]);
}

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
    /* used when getting free pages. */
    unsigned int page_id, n_pages;
    uint16_t unused_space, aux_space;
    uint16_t request;

    /* create a scratch control page for comparison purposes only */
    uint16_t scratch_ctrl_page[CTRL_MAX_PAGES];

    /* buffer to read in page. */
    char read_page_buf[PF_PAGE_SIZE];

    /* void pointer for passing to functions. */
    void *read_page = read_page_buf;

    PF_FileHandle handle;

    string t1 = "t1";
    vector<Attribute> t1_attrs;
    t1_attrs.push_back((struct Attribute) { "a1", TypeInt, 0 });

    // test 1: create table, expect 1 control page, no data pages.

    cout << "\n[ control page creation/verification test ]" << endl; // {{{
    ZERO_ASSERT(rm->createTable(t1, t1_attrs));
    cout << "PASS: createTable(" << output_schema(t1, t1_attrs) << ") [create table, no pages]" << endl;
    ZERO_ASSERT(rm->openTable(t1, handle));
    cout << "PASS: openTable(" << t1 << ") [get handle to table]" << endl;

    assert(handle.GetNumberOfPages() == 0);

    cout << "PASS: getNumberOfPages(" << t1 << "_handle) == 0" << endl;

    ZERO_ASSERT(rm->deleteTable(t1));
    cout << "PASS: deleteTable(" << t1 << ")" << endl;

    cout << "\n[ allocate blank page test (request 500 bytes) ]" << endl;
    request = 500;

    ZERO_ASSERT(rm->createTable(t1, t1_attrs));
    cout << "PASS: createTable(" << output_schema(t1, t1_attrs) << ") [create table & control page]" << endl;

    ZERO_ASSERT(rm->openTable(t1, handle));
    cout << "PASS: openTable(" << t1 << ") [get handle to table]" << endl;

    ZERO_ASSERT(rm->getDataPage(handle, request, page_id, unused_space));
    cout << "PASS: getDataPage(" << request << ") [allocates a new page to fit 500 bytes]" << endl;

    /* no changes made to control page, yet. */
    for(int i=0; i < CTRL_MAX_PAGES; i++) scratch_ctrl_page[i] = SLOT_MAX_SPACE;
    ZERO_ASSERT(handle.ReadPage(0, (void *) read_page));
    ZERO_ASSERT(memcmp(read_page, scratch_ctrl_page, PF_PAGE_SIZE));
    cout << "PASS: read/verify control page 0" << endl;

    /* aux_space == unused space of page. */
    ZERO_ASSERT(rm->getPageSpace(handle, page_id, aux_space));
    assert(aux_space == unused_space);
    cout << "PASS: getPageSpace(" << page_id << ") == " << unused_space << endl;

    ZERO_ASSERT(rm->decreasePageSpace(handle, page_id, request));
    unused_space -= 500;
    cout << "PASS: decreasePageSpace(" << page_id << ", " << request << ")" << endl;

    /* no changes made to control page, yet. */
    for(int i=0; i < CTRL_MAX_PAGES; i++) scratch_ctrl_page[i] = SLOT_MAX_SPACE;
    scratch_ctrl_page[0] -= 500;
    ZERO_ASSERT(handle.ReadPage(0, (void *) read_page));
    ZERO_ASSERT(memcmp(read_page, scratch_ctrl_page, PF_PAGE_SIZE));
    cout << "PASS: read/verify control page 0 [reflect new changes to page 0]" << endl;

    ZERO_ASSERT(rm->getPageSpace(handle, page_id, aux_space));
    assert(aux_space == unused_space);
    cout << "PASS: getPageSpace(" << page_id << ") == " << unused_space << endl;

    assert(handle.GetNumberOfPages() == 2);
    cout << "PASS: getNumberOfPages(" << t1 << "_handle) == 2 [ctrl + data page]" << endl;

    ZERO_ASSERT(rm->deleteTable(t1));
    cout << "PASS: deleteTable(" << t1 << ")" << endl;
    // }}} 

    cout << "\n[ allocate and consume 2048 data page test (1 control pages) ]" << endl; // {{{
    request = SLOT_MAX_SPACE;
    ZERO_ASSERT(rm->createTable(t1, t1_attrs));
    n_pages = 1; // start out with 1 control page.

    cout << "PASS: createTable(" << output_schema(t1, t1_attrs) << ") [create table & control page]" << endl;
    ZERO_ASSERT(rm->openTable(t1, handle));
    cout << "PASS: openTable(" << t1 << ") [get handle to table]" << endl;
   
    for(int i = 0; i < CTRL_MAX_PAGES; i++)
    {
        request = SLOT_MAX_SPACE;

        ZERO_ASSERT(rm->getDataPage(handle, request, page_id, unused_space));
        if (verbose) { cout << "PASS: getDataPage(" << request << ") [page_id: " << page_id << "]" << endl; }
        /* since we're using up the entire page, the unused space and request should be equal. */
        assert(request == unused_space);

        /* getDataPage allocates one whole page for the record. */
        n_pages++;

        ZERO_ASSERT(rm->decreasePageSpace(handle, page_id, request));
        unused_space -= SLOT_MAX_SPACE;
        if (verbose) { cout << "PASS: decreasePageSpace(" << page_id << ", " << request << ")" << endl; }

        ZERO_ASSERT(rm->getPageSpace(handle, page_id, aux_space));
        assert(aux_space == unused_space);
        if (verbose) { cout << "PASS: getPageSpace(" << page_id << ") == " << unused_space << endl; }
        assert(handle.GetNumberOfPages() == n_pages);
        if (verbose) { cout << "PASS: getNumberOfPages(" << t1 << "_handle) == " << n_pages << endl; }
    }

    cout << "PASS: created and fully consumed " << CTRL_MAX_PAGES << endl;

    /* verify the total number of pages is 2049. */
    assert(handle.GetNumberOfPages() == CTRL_BLOCK_SIZE);
    cout << "PASS: getNumberOfPages(" << t1 << "_handle) == " << CTRL_BLOCK_SIZE << " [1 ctrl + " << CTRL_MAX_PAGES << " data pages]" << endl;

    ZERO_ASSERT(rm->deleteTable(t1));
    cout << "PASS: deleteTable(" << t1 << ")" << endl;
    // }}}

    cout << "\n[ allocate and consume 2050 data page test (2 control pages) ]" << endl; // {{{
    request = SLOT_MAX_SPACE;
    ZERO_ASSERT(rm->createTable(t1, t1_attrs));
    n_pages = 1; // start out with 1 control page.

    cout << "PASS: createTable(" << output_schema(t1, t1_attrs) << ") [create table & control page]" << endl;
    ZERO_ASSERT(rm->openTable(t1, handle));
    cout << "PASS: openTable(" << t1 << ") [get handle to table]" << endl;
   
    for(int i = 0; i < (CTRL_MAX_PAGES+2); i++)
    {
        request = SLOT_MAX_SPACE;

        ZERO_ASSERT(rm->getDataPage(handle, request, page_id, unused_space));
        if (verbose) { cout << "PASS: getDataPage(" << request << ") [page_id: " << page_id << "]" << endl; }
        /* since we're using up the entire page, the unused space and request should be equal. */
        assert(request == unused_space);

        /* getDataPage allocates one whole page for the record. */
        n_pages++;

        /* a new control page is created every 2048 records. */
        if(i == CTRL_MAX_PAGES)
            n_pages++;

        ZERO_ASSERT(rm->decreasePageSpace(handle, page_id, request));
        unused_space -= SLOT_MAX_SPACE;
        if (verbose) { cout << "PASS: decreasePageSpace(" << page_id << ", " << request << ")" << endl; }

        ZERO_ASSERT(rm->getPageSpace(handle, page_id, aux_space));
        assert(aux_space == unused_space);
        if (verbose) { cout << "PASS: getPageSpace(" << page_id << ") == " << unused_space << endl; }

        
        assert(handle.GetNumberOfPages() == n_pages);
        if (verbose) { cout << "PASS: getNumberOfPages(" << t1 << "_handle) == " << n_pages << endl; }
    }

    cout << "PASS: created and fully consumed " << (2+CTRL_MAX_PAGES) << " data pages." << endl;

    /* verify the total number of pages is 2052. */
    assert(handle.GetNumberOfPages() == (CTRL_BLOCK_SIZE+3));
    cout << "PASS: getNumberOfPages(" << t1 << "_handle) == " << (CTRL_BLOCK_SIZE+3) << " [2 ctrl + " << (CTRL_MAX_PAGES+2) << " data pages]" << endl;

    ZERO_ASSERT(rm->deleteTable(t1));
    cout << "PASS: deleteTable(" << t1 << ")" << endl;
    // }}}

    cout << "\n[ allocate 4 data pages, change space attributes. ]" << endl; // {{{
    request = SLOT_MAX_SPACE;
    ZERO_ASSERT(rm->createTable(t1, t1_attrs));
    n_pages = 1; // start out with 1 control page.

    cout << "PASS: createTable(" << output_schema(t1, t1_attrs) << ") [create table & control page]" << endl;
    ZERO_ASSERT(rm->openTable(t1, handle));
    cout << "PASS: openTable(" << t1 << ") [get handle to table]" << endl;
   
    for(int i = 0; i < 4; i++)
    {
        request = SLOT_MAX_SPACE;

        ZERO_ASSERT(rm->getDataPage(handle, request, page_id, unused_space));
        if (verbose) { cout << "PASS: getDataPage(" << request << ") [page_id: " << page_id << "]" << endl; }
        /* since we're using up the entire page, the unused space and request should be equal. */
        assert(request == unused_space);

        /* getDataPage allocates one whole page for the record. */
        n_pages++;

        ZERO_ASSERT(rm->decreasePageSpace(handle, page_id, request));
        unused_space -= SLOT_MAX_SPACE;
        if (verbose) { cout << "PASS: decreasePageSpace(" << page_id << ", " << request << ")" << endl; }

        ZERO_ASSERT(rm->getPageSpace(handle, page_id, aux_space));
        assert(aux_space == unused_space);
        if (verbose) { cout << "PASS: getPageSpace(" << page_id << ") == " << unused_space << endl; }
        assert(handle.GetNumberOfPages() == n_pages);
        if (verbose) { cout << "PASS: getNumberOfPages(" << t1 << "_handle) == " << n_pages << endl; }
    }

    cout << "PASS: created and fully consumed " << 4 << " pages" << endl;

    /* verify the total number of pages is 5. */
    assert(handle.GetNumberOfPages() == 5);
    cout << "PASS: getNumberOfPages(" << t1 << "_handle) == " << 5 << " [1 ctrl + " << 4 << " data pages]" << endl;

    /* increase page space for 2 and 3 to 500 bytes. */
    page_id = 3, request = 500, unused_space = 500;
    ZERO_ASSERT(rm->increasePageSpace(handle, page_id, request));
    cout << "PASS: increasePageSpace(" << page_id << ", " << request << ")" << endl;
    ZERO_ASSERT(rm->getPageSpace(handle, page_id, aux_space));
    assert(aux_space == unused_space);
    cout << "PASS: getPageSpace(" << page_id << ") == " << unused_space << endl;

    page_id = 4, request = 500, unused_space = 500;
    ZERO_ASSERT(rm->increasePageSpace(handle, page_id, request));
    cout << "PASS: increasePageSpace(" << page_id << ", " << request << ")" << endl;
    ZERO_ASSERT(rm->getPageSpace(handle, page_id, aux_space));
    assert(aux_space == unused_space);
    cout << "PASS: getPageSpace(" << page_id << ") == " << unused_space << endl;

    /* 4 requests of 100 bytes (page 3), 2 requests of 220 bytes (page 4), 1 request of 60 bytes (page 3), 1 request of 50 bytes (page 4) */
    for(int i = 0; i < 4; i++)
    {
        request = 100;

        ZERO_ASSERT(rm->getDataPage(handle, request, page_id, unused_space));
        cout << "PASS: getDataPage(" << request << ") [page_id: " << page_id << "]" << endl;

        ZERO_ASSERT(rm->decreasePageSpace(handle, page_id, request));
        cout << "PASS: decreasePageSpace(" << page_id << ", " << request << ")" << endl;
    }

    /* page 3 should have 100 bytes left. */
    page_id = 3, unused_space = 100;
    ZERO_ASSERT(rm->getPageSpace(handle, page_id, aux_space));
    assert(aux_space == unused_space);
    cout << "PASS: getPageSpace(" << page_id << ") == " << unused_space << endl;

    for(int i = 0; i < 2; i++)
    {
        request = 220;

        ZERO_ASSERT(rm->getDataPage(handle, request, page_id, unused_space));
        cout << "PASS: getDataPage(" << request << ") [page_id: " << page_id << "]" << endl;

        ZERO_ASSERT(rm->decreasePageSpace(handle, page_id, request));
        cout << "PASS: decreasePageSpace(" << page_id << ", " << request << ")" << endl;
    }

    /* page 4 should have 60 bytes left. */
    page_id = 4, unused_space = 60;
    ZERO_ASSERT(rm->getPageSpace(handle, page_id, aux_space));
    assert(aux_space == unused_space);
    cout << "PASS: getPageSpace(" << page_id << ") == " << unused_space << endl;

    for(int i = 0; i < 1; i++)
    {
        request = 60;

        ZERO_ASSERT(rm->getDataPage(handle, request, page_id, unused_space));
        cout << "PASS: getDataPage(" << request << ") [page_id: " << page_id << "]" << endl;

        ZERO_ASSERT(rm->decreasePageSpace(handle, page_id, request));
        cout << "PASS: decreasePageSpace(" << page_id << ", " << request << ")" << endl;
    }

    /* page 3 should now have a total of 40 bytes. */
    page_id = 3, unused_space = 40;
    ZERO_ASSERT(rm->getPageSpace(handle, page_id, aux_space));
    assert(aux_space == unused_space);
    cout << "PASS: getPageSpace(" << page_id << ") == " << unused_space << endl;

    for(int i = 0; i < 1; i++)
    {
        request = 50;

        ZERO_ASSERT(rm->getDataPage(handle, request, page_id, unused_space));
        cout << "PASS: getDataPage(" << request << ") [page_id: " << page_id << "]" << endl;

        ZERO_ASSERT(rm->decreasePageSpace(handle, page_id, request));
        cout << "PASS: decreasePageSpace(" << page_id << ", " << request << ")" << endl;
    }

    /* page 4 should now have a total of 10 bytes. */
    page_id = 4, unused_space = 10;
    ZERO_ASSERT(rm->getPageSpace(handle, page_id, aux_space));
    assert(aux_space == unused_space);
    cout << "PASS: getPageSpace(" << page_id << ") == " << unused_space << endl;

    ZERO_ASSERT(rm->deleteTable(t1));
    cout << "PASS: deleteTable(" << t1 << ")" << endl;
    // }}}

    cout << "\n[ allocate 2048+4 data pages, change space attributes (over 2 control pages). ]" << endl; // {{{
    request = SLOT_MAX_SPACE;
    ZERO_ASSERT(rm->createTable(t1, t1_attrs));
    n_pages = 1; // start out with 1 control page.

    cout << "PASS: createTable(" << output_schema(t1, t1_attrs) << ") [create table & control page]" << endl;
    ZERO_ASSERT(rm->openTable(t1, handle));
    cout << "PASS: openTable(" << t1 << ") [get handle to table]" << endl;
   
    for(int i = 0; i < (CTRL_MAX_PAGES+4); i++)
    {
        request = SLOT_MAX_SPACE;

        ZERO_ASSERT(rm->getDataPage(handle, request, page_id, unused_space));
        if (verbose) { cout << "PASS: getDataPage(" << request << ") [page_id: " << page_id << "]" << endl; }
        /* since we're using up the entire page, the unused space and request should be equal. */
        assert(request == unused_space);

        /* getDataPage allocates one whole page for the record. */
        n_pages++;

        /* a new control page is created every 2048 records. */
        if(i == CTRL_MAX_PAGES)
            n_pages++;

        ZERO_ASSERT(rm->decreasePageSpace(handle, page_id, request));
        unused_space -= SLOT_MAX_SPACE;
        if (verbose) { cout << "PASS: decreasePageSpace(" << page_id << ", " << request << ")" << endl; }

        ZERO_ASSERT(rm->getPageSpace(handle, page_id, aux_space));
        assert(aux_space == unused_space);
        if (verbose) { cout << "PASS: getPageSpace(" << page_id << ") == " << unused_space << endl; }
        assert(handle.GetNumberOfPages() == n_pages);
        if (verbose) { cout << "PASS: getNumberOfPages(" << t1 << "_handle) == " << n_pages << endl; }
    }

    cout << "PASS: created and fully consumed " << 4 << " pages" << endl;

    /* verify the total number of pages is 2048+4+2. */
    assert(handle.GetNumberOfPages() == CTRL_MAX_PAGES+4+2);
    cout << "PASS: getNumberOfPages(" << t1 << "_handle) == " << CTRL_MAX_PAGES+2+4 << " [2 ctrl + " << 2052 << " data pages]" << endl;

    /* increase page space for 3 and 2051 to 500 bytes. */
    page_id = 3, request = 500, unused_space = 500;
    ZERO_ASSERT(rm->increasePageSpace(handle, page_id, request));
    cout << "PASS: increasePageSpace(" << page_id << ", " << request << ")" << endl;
    ZERO_ASSERT(rm->getPageSpace(handle, page_id, aux_space));
    assert(aux_space == unused_space);
    cout << "PASS: getPageSpace(" << page_id << ") == " << unused_space << endl;

    page_id = 2051, request = 500, unused_space = 500;
    ZERO_ASSERT(rm->increasePageSpace(handle, page_id, request));
    cout << "PASS: increasePageSpace(" << page_id << ", " << request << ")" << endl;
    ZERO_ASSERT(rm->getPageSpace(handle, page_id, aux_space));
    assert(aux_space == unused_space);
    cout << "PASS: getPageSpace(" << page_id << ") == " << unused_space << endl;

    /* 4 requests of 100 bytes (page 3), 2 requests of 220 bytes (page 4), 1 request of 60 bytes (page 3), 1 request of 50 bytes (page 4) */
    for(int i = 0; i < 4; i++)
    {
        request = 100;

        ZERO_ASSERT(rm->getDataPage(handle, request, page_id, unused_space));
        cout << "PASS: getDataPage(" << request << ") [page_id: " << page_id << "]" << endl;

        ZERO_ASSERT(rm->decreasePageSpace(handle, page_id, request));
        cout << "PASS: decreasePageSpace(" << page_id << ", " << request << ")" << endl;
    }

    /* page 3 should have 100 bytes left. */
    page_id = 3, unused_space = 100;
    ZERO_ASSERT(rm->getPageSpace(handle, page_id, aux_space));
    assert(aux_space == unused_space);
    cout << "PASS: getPageSpace(" << page_id << ") == " << unused_space << endl;

    for(int i = 0; i < 2; i++)
    {
        request = 220;

        ZERO_ASSERT(rm->getDataPage(handle, request, page_id, unused_space));
        cout << "PASS: getDataPage(" << request << ") [page_id: " << page_id << "]" << endl;

        ZERO_ASSERT(rm->decreasePageSpace(handle, page_id, request));
        cout << "PASS: decreasePageSpace(" << page_id << ", " << request << ")" << endl;
    }

    /* page 4 should have 60 bytes left. */
    page_id = 2051, unused_space = 60;
    ZERO_ASSERT(rm->getPageSpace(handle, page_id, aux_space));
    assert(aux_space == unused_space);
    cout << "PASS: getPageSpace(" << page_id << ") == " << unused_space << endl;

    for(int i = 0; i < 1; i++)
    {
        request = 60;

        ZERO_ASSERT(rm->getDataPage(handle, request, page_id, unused_space));
        cout << "PASS: getDataPage(" << request << ") [page_id: " << page_id << "]" << endl;

        ZERO_ASSERT(rm->decreasePageSpace(handle, page_id, request));
        cout << "PASS: decreasePageSpace(" << page_id << ", " << request << ")" << endl;
    }

    /* page 3 should now have a total of 40 bytes. */
    page_id = 3, unused_space = 40;
    ZERO_ASSERT(rm->getPageSpace(handle, page_id, aux_space));
    assert(aux_space == unused_space);
    cout << "PASS: getPageSpace(" << page_id << ") == " << unused_space << endl;

    for(int i = 0; i < 1; i++)
    {
        request = 50;

        ZERO_ASSERT(rm->getDataPage(handle, request, page_id, unused_space));
        cout << "PASS: getDataPage(" << request << ") [page_id: " << page_id << "]" << endl;

        ZERO_ASSERT(rm->decreasePageSpace(handle, page_id, request));
        cout << "PASS: decreasePageSpace(" << page_id << ", " << request << ")" << endl;
    }

    /* page 4 should now have a total of 10 bytes. */
    page_id = 2051, unused_space = 10;
    ZERO_ASSERT(rm->getPageSpace(handle, page_id, aux_space));
    assert(aux_space == unused_space);
    cout << "PASS: getPageSpace(" << page_id << ") == " << unused_space << endl;

    ZERO_ASSERT(rm->deleteTable(t1));
    cout << "PASS: deleteTable(" << t1 << ")" << endl;
    // }}}

    cout << "\n[ allocate 2048+4 data pages, change space attributes (over 2 control pages). ]" << endl; // {{{
    request = SLOT_MAX_SPACE;
    ZERO_ASSERT(rm->createTable(t1, t1_attrs));
    n_pages = 1; // start out with 1 control page.

    cout << "PASS: createTable(" << output_schema(t1, t1_attrs) << ") [create table & control page]" << endl;
    ZERO_ASSERT(rm->openTable(t1, handle));
    cout << "PASS: openTable(" << t1 << ") [get handle to table]" << endl;
   
    for(int i = 0; i < (CTRL_MAX_PAGES+4); i++)
    {
        request = SLOT_MAX_SPACE;

        ZERO_ASSERT(rm->getDataPage(handle, request, page_id, unused_space));
        if (verbose) { cout << "PASS: getDataPage(" << request << ") [page_id: " << page_id << "]" << endl; }
        /* since we're using up the entire page, the unused space and request should be equal. */
        assert(request == unused_space);

        /* getDataPage allocates one whole page for the record. */
        n_pages++;

        /* a new control page is created every 2048 records. */
        if(i == CTRL_MAX_PAGES)
            n_pages++;

        ZERO_ASSERT(rm->decreasePageSpace(handle, page_id, request));
        unused_space -= SLOT_MAX_SPACE;
        if (verbose) { cout << "PASS: decreasePageSpace(" << page_id << ", " << request << ")" << endl; }

        ZERO_ASSERT(rm->getPageSpace(handle, page_id, aux_space));
        assert(aux_space == unused_space);
        if (verbose) { cout << "PASS: getPageSpace(" << page_id << ") == " << unused_space << endl; }
        assert(handle.GetNumberOfPages() == n_pages);
        if (verbose) { cout << "PASS: getNumberOfPages(" << t1 << "_handle) == " << n_pages << endl; }
    }

    cout << "PASS: created and fully consumed " << 4 << " pages" << endl;

    /* verify the total number of pages is 2048+4+2. */
    assert(handle.GetNumberOfPages() == CTRL_MAX_PAGES+4+2);
    cout << "PASS: getNumberOfPages(" << t1 << "_handle) == " << CTRL_MAX_PAGES+2+4 << " [2 ctrl + " << 2052 << " data pages]" << endl;

    /* increase page space for 3 and 2051 to 500 bytes. */
    page_id = 3, request = 500, unused_space = 500;
    ZERO_ASSERT(rm->increasePageSpace(handle, page_id, request));
    cout << "PASS: increasePageSpace(" << page_id << ", " << request << ")" << endl;
    ZERO_ASSERT(rm->getPageSpace(handle, page_id, aux_space));
    assert(aux_space == unused_space);
    cout << "PASS: getPageSpace(" << page_id << ") == " << unused_space << endl;

    page_id = 2050, request = 500, unused_space = 500;
    ZERO_ASSERT(rm->increasePageSpace(handle, page_id, request));
    cout << "PASS: increasePageSpace(" << page_id << ", " << request << ")" << endl;
    ZERO_ASSERT(rm->getPageSpace(handle, page_id, aux_space));
    assert(aux_space == unused_space);
    cout << "PASS: getPageSpace(" << page_id << ") == " << unused_space << endl;

    /* 4 requests of 100 bytes (page 3), 2 requests of 220 bytes (page 4), 1 request of 60 bytes (page 3), 1 request of 50 bytes (page 4) */
    for(int i = 0; i < 4; i++)
    {
        request = 100;

        ZERO_ASSERT(rm->getDataPage(handle, request, page_id, unused_space));
        cout << "PASS: getDataPage(" << request << ") [page_id: " << page_id << "]" << endl;

        ZERO_ASSERT(rm->decreasePageSpace(handle, page_id, request));
        cout << "PASS: decreasePageSpace(" << page_id << ", " << request << ")" << endl;
    }

    /* page 3 should have 100 bytes left. */
    page_id = 3, unused_space = 100;
    ZERO_ASSERT(rm->getPageSpace(handle, page_id, aux_space));
    assert(aux_space == unused_space);
    cout << "PASS: getPageSpace(" << page_id << ") == " << unused_space << endl;

    for(int i = 0; i < 2; i++)
    {
        request = 220;

        ZERO_ASSERT(rm->getDataPage(handle, request, page_id, unused_space));
        cout << "PASS: getDataPage(" << request << ") [page_id: " << page_id << "]" << endl;

        ZERO_ASSERT(rm->decreasePageSpace(handle, page_id, request));
        cout << "PASS: decreasePageSpace(" << page_id << ", " << request << ")" << endl;
    }

    /* page 4 should have 60 bytes left. */
    page_id = 2050, unused_space = 60;
    ZERO_ASSERT(rm->getPageSpace(handle, page_id, aux_space));
    assert(aux_space == unused_space);
    cout << "PASS: getPageSpace(" << page_id << ") == " << unused_space << endl;

    for(int i = 0; i < 1; i++)
    {
        request = 60;

        ZERO_ASSERT(rm->getDataPage(handle, request, page_id, unused_space));
        cout << "PASS: getDataPage(" << request << ") [page_id: " << page_id << "]" << endl;

        ZERO_ASSERT(rm->decreasePageSpace(handle, page_id, request));
        cout << "PASS: decreasePageSpace(" << page_id << ", " << request << ")" << endl;
    }

    /* page 3 should now have a total of 40 bytes. */
    page_id = 3, unused_space = 40;
    ZERO_ASSERT(rm->getPageSpace(handle, page_id, aux_space));
    assert(aux_space == unused_space);
    cout << "PASS: getPageSpace(" << page_id << ") == " << unused_space << endl;

    for(int i = 0; i < 1; i++)
    {
        request = 50;

        ZERO_ASSERT(rm->getDataPage(handle, request, page_id, unused_space));
        cout << "PASS: getDataPage(" << request << ") [page_id: " << page_id << "]" << endl;

        ZERO_ASSERT(rm->decreasePageSpace(handle, page_id, request));
        cout << "PASS: decreasePageSpace(" << page_id << ", " << request << ")" << endl;
    }

    /* page 4 should now have a total of 10 bytes. */
    page_id = 2050, unused_space = 10;
    ZERO_ASSERT(rm->getPageSpace(handle, page_id, aux_space));
    assert(aux_space == unused_space);
    cout << "PASS: getPageSpace(" << page_id << ") == " << unused_space << endl;

    ZERO_ASSERT(rm->deleteTable(t1));
    cout << "PASS: deleteTable(" << t1 << ")" << endl;
    // }}}

    cout << "\n[ allocate 1 data pages, check decrease/increasePageSpace bounds. ]" << endl; // {{{
    request = SLOT_MAX_SPACE;
    ZERO_ASSERT(rm->createTable(t1, t1_attrs));
    n_pages = 1; // start out with 1 control page.

    cout << "PASS: createTable(" << output_schema(t1, t1_attrs) << ") [create table & control page]" << endl;
    ZERO_ASSERT(rm->openTable(t1, handle));
    cout << "PASS: openTable(" << t1 << ") [get handle to table]" << endl;
   
    for(int i = 0; i < 1; i++)
    {
        request = SLOT_MAX_SPACE;

        ZERO_ASSERT(rm->getDataPage(handle, request, page_id, unused_space));
        cout << "PASS: getDataPage(" << request << ") [page_id: " << page_id << "]" << endl;
        /* since we're using up the entire page, the unused space and request should be equal. */
        assert(request == unused_space);

        /* getDataPage allocates one whole page for the record. */
        n_pages++;

        ZERO_ASSERT(rm->decreasePageSpace(handle, page_id, request));
        unused_space -= SLOT_MAX_SPACE;
        cout << "PASS: decreasePageSpace(" << page_id << ", " << request << ")" << endl;

        ZERO_ASSERT(rm->getPageSpace(handle, page_id, aux_space));
        assert(aux_space == unused_space);
        cout << "PASS: getPageSpace(" << page_id << ") == " << unused_space << endl;
        assert(handle.GetNumberOfPages() == n_pages);
        cout << "PASS: getNumberOfPages(" << t1 << "_handle) == " << n_pages << endl;
    }

    cout << "PASS: created and fully consumed " << 1 << " pages" << endl;

    /* verify the total number of pages is 2. */
    assert(handle.GetNumberOfPages() == 2);
    cout << "PASS: getNumberOfPages(" << t1 << "_handle) == " << 2 << " [1 ctrl + " << 1 << " data pages]" << endl;

    /* increase page space of page 1 to SLOT_MAX_SPACE-1 bytes. */
    page_id = 1, request = SLOT_MAX_SPACE-1, unused_space = SLOT_MAX_SPACE-1;
    ZERO_ASSERT(rm->increasePageSpace(handle, page_id, request));
    cout << "PASS: increasePageSpace(" << page_id << ", " << request << ")" << endl;
    ZERO_ASSERT(rm->getPageSpace(handle, page_id, aux_space));
    assert(aux_space == unused_space);
    cout << "PASS: getPageSpace(" << page_id << ") == " << unused_space << endl;

    /* increase page space of page 1, 2 bytes. [overflow] */
    page_id = 1, request = 2, unused_space = SLOT_MAX_SPACE-1;
    NONZERO_ASSERT(rm->increasePageSpace(handle, page_id, request));
    cout << "PASS: increasePageSpace(" << page_id << ", " << request << ") [fails since +2 is overflow]" << endl;
    ZERO_ASSERT(rm->getPageSpace(handle, page_id, aux_space));
    assert(aux_space == unused_space);
    cout << "PASS: getPageSpace(" << page_id << ") == " << unused_space << endl;

    /* decrease page space of page 1, SLOT_MAX_SPACE bytes. [underflow] */
    page_id = 1, request = SLOT_MAX_SPACE, unused_space = SLOT_MAX_SPACE-1;
    NONZERO_ASSERT(rm->decreasePageSpace(handle, page_id, request));
    cout << "PASS: decreasePageSpace(" << page_id << ", " << request << ") [fails since -2088 is underflow]" << endl;
    ZERO_ASSERT(rm->getPageSpace(handle, page_id, aux_space));
    assert(aux_space == unused_space);
    cout << "PASS: getPageSpace(" << page_id << ") == " << unused_space << endl;

    ZERO_ASSERT(rm->deleteTable(t1));
    cout << "PASS: deleteTable(" << t1 << ")" << endl;
    // }}}

    cout << "\n[ invalid decrease/increasePageSpace on non-existent/control pages. ]" << endl; // {{{
    request = SLOT_MAX_SPACE;
    ZERO_ASSERT(rm->createTable(t1, t1_attrs));
    n_pages = 1; // start out with 1 control page.

    cout << "PASS: createTable(" << output_schema(t1, t1_attrs) << ") [create table & control page]" << endl;
    ZERO_ASSERT(rm->openTable(t1, handle));
    cout << "PASS: openTable(" << t1 << ") [get handle to table]" << endl;
   
    for(int i = 0; i < 1; i++)
    {
        request = SLOT_MAX_SPACE;

        ZERO_ASSERT(rm->getDataPage(handle, request, page_id, unused_space));
        cout << "PASS: getDataPage(" << request << ") [page_id: " << page_id << "]" << endl;
        /* since we're using up the entire page, the unused space and request should be equal. */
        assert(request == unused_space);

        /* getDataPage allocates one whole page for the record. */
        n_pages++;

        ZERO_ASSERT(rm->decreasePageSpace(handle, page_id, request));
        unused_space -= SLOT_MAX_SPACE;
        cout << "PASS: decreasePageSpace(" << page_id << ", " << request << ")" << endl;

        ZERO_ASSERT(rm->getPageSpace(handle, page_id, aux_space));
        assert(aux_space == unused_space);
        cout << "PASS: getPageSpace(" << page_id << ") == " << unused_space << endl;
        assert(handle.GetNumberOfPages() == n_pages);
        cout << "PASS: getNumberOfPages(" << t1 << "_handle) == " << n_pages << endl;
    }

    cout << "PASS: created and fully consumed " << 1 << " pages" << endl;

    /* verify the total number of pages is 2. */
    assert(handle.GetNumberOfPages() == 2);
    cout << "PASS: getNumberOfPages(" << t1 << "_handle) == " << 2 << " [1 ctrl + " << 1 << " data pages]" << endl;

    /* increase page space of page 2 to SLOT_MAX_SPACE-1 bytes. */
    page_id = 0, request = 100, unused_space = SLOT_MAX_SPACE-100;
    NONZERO_ASSERT(rm->increasePageSpace(handle, page_id, request));
    cout << "PASS: increasePageSpace(" << page_id << ", " << request << ") [page is control page]" << endl;
    NONZERO_ASSERT(rm->decreasePageSpace(handle, page_id, request));
    cout << "PASS: decreasePageSpace(" << page_id << ", " << request << ") [page is control page]" << endl;
    NONZERO_ASSERT(rm->getPageSpace(handle, page_id, aux_space));
    cout << "PASS: getPageSpace(" << page_id << ") [page is control page]" << endl;

    /* increase page space of page 2 to SLOT_MAX_SPACE-1 bytes. */
    page_id = 2, request = 100, unused_space = SLOT_MAX_SPACE-100;
    NONZERO_ASSERT(rm->increasePageSpace(handle, page_id, request));
    cout << "PASS: increasePageSpace(" << page_id << ", " << request << ") [page doesnt exist]" << endl;
    NONZERO_ASSERT(rm->decreasePageSpace(handle, page_id, request));
    cout << "PASS: decreasePageSpace(" << page_id << ", " << request << ") [page doesnt exist]" << endl;
    NONZERO_ASSERT(rm->getPageSpace(handle, page_id, aux_space));
    cout << "PASS: getPageSpace(" << page_id << ") [page doesnt exist]" << endl;

    ZERO_ASSERT(rm->deleteTable(t1));
    cout << "PASS: deleteTable(" << t1 << ")" << endl;
    // }}}

} // }}}

void rmTest_TableMgmt(RM *rm) // {{{
{
    RID r1, r2;

    /* used when getting free pages. */
    unsigned int page_id, n_pages;
    uint16_t unused_space, aux_space;
    uint16_t request;

    /* create a scratch control page for comparison purposes only */
    uint16_t scratch_ctrl_page[CTRL_MAX_PAGES];

    /* buffer to read in page. */
    char read_page_buf[PF_PAGE_SIZE];

    /* void pointer for passing to functions. */
    void *read_page = read_page_buf;

    /* place to store data. */
    int data_size;
    char data[PF_PAGE_SIZE];
    char data_read[PF_PAGE_SIZE];


    /* clear the data/data_read to ensure the data matches. */
    memset(data, 0xF8, PF_PAGE_SIZE);
    memset(data_read, 0xF8, PF_PAGE_SIZE);

    PF_FileHandle handle;

    string t1 = "t1";
    vector<Attribute> t1_attrs;
    /* 512 byte max records. */
    t1_attrs.push_back((struct Attribute) { "a1", TypeVarChar, 508 });

    
    cout << "\n[ insert and read a record. ]" << endl; // {{{
    ZERO_ASSERT(rm->createTable(t1, t1_attrs));
    cout << "PASS: createTable(" << output_schema(t1, t1_attrs) << ")" << endl;
    
    /* create a record. */
    data_size = 1;
    memcpy(data, &data_size, sizeof(data_size));
    memcpy(data+sizeof(data_size), &("T"), 1);
    ZERO_ASSERT(rm->insertTuple(t1, data, r1));
    cout << "rm->insertTuple(" << t1 << ", r1)" << endl;
   
    /* read back record. */
    ZERO_ASSERT(rm->readTuple(t1, r1, data_read));   
    ZERO_ASSERT(memcmp(data, data_read, PF_PAGE_SIZE));
    cout << "rm->readTuple(" << t1 << ", r1)" << endl;

    /* wipe out the table. */
    ZERO_ASSERT(rm->deleteTable(t1));
    cout << "PASS: deleteTable(" << t1 << ")" << endl;
    // }}} 

    cout << "\n[ insert 2 records and delete the last record. ]" << endl; // {{{
    ZERO_ASSERT(rm->createTable(t1, t1_attrs));
    cout << "PASS: createTable(" << output_schema(t1, t1_attrs) << ")" << endl;
    
    /* create a record. */
    data_size = 1;
    memcpy(data, &data_size, sizeof(data_size));
    memcpy(data+sizeof(data_size), &("T"), 1);
    ZERO_ASSERT(rm->insertTuple(t1, data, r1));
    cout << "rm->insertTuple(" << t1 << ", r1)" << endl;
   
    /* read back record. */
    ZERO_ASSERT(rm->readTuple(t1, r1, data_read));   
    ZERO_ASSERT(memcmp(data, data_read, PF_PAGE_SIZE));
    cout << "rm->readTuple(" << t1 << ", r1)" << endl;

    /* create second record. */
    data_size = 1;
    memcpy(data, &data_size, sizeof(data_size));
    memcpy(data+sizeof(data_size), &("T"), 1);
    ZERO_ASSERT(rm->insertTuple(t1, data, r2));
    cout << "rm->insertTuple(" << t1 << ", r2)" << endl;
   
    /* read back record. */
    ZERO_ASSERT(rm->readTuple(t1, r2, data_read));   
    ZERO_ASSERT(memcmp(data, data_read, PF_PAGE_SIZE));
    cout << "rm->readTuple(" << t1 << ", r2)" << endl;

    /* delete last record. */
    ZERO_ASSERT(rm->deleteTuple(t1, r2));

    /* wipe out the table. */
    ZERO_ASSERT(rm->deleteTable(t1));
    cout << "PASS: deleteTable(" << t1 << ")" << endl;
    // }}} 

    cout << "\n[ insert 2 records and delete the first record. ]" << endl; // {{{
    ZERO_ASSERT(rm->createTable(t1, t1_attrs));
    cout << "PASS: createTable(" << output_schema(t1, t1_attrs) << ")" << endl;
    
    /* create a record. */
    data_size = 1;
    memcpy(data, &data_size, sizeof(data_size));
    memcpy(data+sizeof(data_size), &("T"), 1);
    ZERO_ASSERT(rm->insertTuple(t1, data, r1));
    cout << "rm->insertTuple(" << t1 << ", r1)" << endl;
   
    /* read back record. */
    ZERO_ASSERT(rm->readTuple(t1, r1, data_read));   
    ZERO_ASSERT(memcmp(data, data_read, PF_PAGE_SIZE));
    cout << "rm->readTuple(" << t1 << ", r1)" << endl;

    /* wipe out the table. */
    ZERO_ASSERT(rm->deleteTable(t1));
    cout << "PASS: deleteTable(" << t1 << ")" << endl;
    // }}} 

} // }}}

void rmTest()
{
    RM *rm = RM::Instance();

    // delete all test files
    rmTest_CleanUp();

    // write your own testing cases here
    cout << "System Catalogue (createTable, deleteTable, getAttributes) tests: " << endl << endl;
    //rmTest_SystemCatalog(rm);
    //rmTest_PageMgmt(rm);
    rmTest_TableMgmt(rm);
}

int main(int argc, char **argv)
{
    cout << "test..." << endl;

    if(argc > 1 && string(argv[1]) == "-v")
       verbose = true;

    rmTest();
    // other tests go here

    cout << "OK" << endl;
}
