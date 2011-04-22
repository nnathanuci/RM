#include "rm.h"
#include <cstdio>
#include <fstream>
#include <iostream>
#include <stdint.h>
#include <stdio.h>
#include <string.h>


RM* RM::_rm = 0;

RM* RM::Instance()
{
    if(!_rm)
        _rm = new RM();
    
    return _rm;
}

RM::RM()
{
    pf = PF_Manager::Instance();

    /* find system catalog file, create an open file handle, and empty map to cache open tables. */
    /* XXX: put code to open system catalog. */
}

RM::~RM()
{
    
}

RC RM::AllocateControlPage(PF_FileHandle &fileHandle) // {{{
{
    /* buffer to write control page. */
    static char page[PF_PAGE_SIZE];

    /* overlay buffer as a uint16_t array. */
    uint16_t *ctrl_page = (uint16_t *) page;

    /* blank the space for all pages. */
    for(unsigned int i = 0; i < CTRL_MAX_PAGES; i++)
        ctrl_page[i] = SLOT_MAX_SPACE;

    return(fileHandle.AppendPage(page));
} // }}}

RC RM::AllocateDataPage(PF_FileHandle &fileHandle) // {{{
{
    /* buffer to write blank page. */
    static char page[PF_PAGE_SIZE] = {0};

    uint16_t *slot_page = (uint16_t *) page;

    /* all fields are 0, except: num_slots, for 1 unallocated slot, and slot 0 stores slot queue end marker. */
    slot_page[NUM_SLOT_INDEX] = 1;
    slot_page[GET_SLOT_INDEX(0)] = SLOT_QUEUE_END;

    return(fileHandle.AppendPage(page));
} // }}}

RC RM::findBlankPage(PF_FileHandle &fileHandle, uint16_t length, unsigned int &page_id, uint16_t &unused_space) // {{{
{
    /* buffer to read in control page. */
    static char read_page[PF_PAGE_SIZE];

    /* read page as an array of uint16_t. */
    uint16_t *ctrl_page = (uint16_t *) read_page;

    /* get number of allocated pages. */
    unsigned int n_pages = fileHandle.GetNumberOfPages();

    /* calculate number of control/data pages. */
    unsigned int n_ctrl_pages = CTRL_NUM_CTRL_PAGES(n_pages);
    unsigned int n_data_pages = CTRL_NUM_DATA_PAGES(n_pages);

    /* There should always be a control page, since the database creates one. */
    if(n_pages == 0)
        return -1;

    /* Layout of Control Structure in Heap:
       [C] [D * CTRL_MAX_PAGES] [C] [D * CTRL_MAX_PAGES] [C] [D] [D] ...
       (C: control page, D: data page)
    */
    for(unsigned int i = 0; i < n_ctrl_pages; i++)
    {
        /* get the page id for i-th control page. */
        unsigned int ctrl_page_id = CTRL_PAGE_ID(i);

        unsigned int n_allocated_pages = CTRL_MAX_PAGES;

        /* if we're on the last control page, only iterate over remaining allocated pages,
           since the control page might not control all possible pages.
        */
        if((n_ctrl_pages - 1) == i)
            n_allocated_pages = n_data_pages % CTRL_MAX_PAGES;

        /* read in first control page. */
        if(fileHandle.ReadPage(ctrl_page_id, (void *) read_page))
            return -1;

        for(unsigned int j = 0; j < n_allocated_pages; j++)
        {
            /* found a free page, consume the space, and return the page id. */
            if(length <= ctrl_page[j])
            {
                page_id = ctrl_page_id + (j + 1); // page index needs to shift by 1.

                /* update free space now, since it's guaranteed to fit? */
                ctrl_page[j] -= length;

                return 0;
            }
        }
    }

    /* if all data pages consume fill all control pages, then allocate a new control page. */
    if((n_data_pages % CTRL_MAX_PAGES) == 2)
    {
        if(AllocateControlPage(fileHandle))
            return -1;

        /* number of pages increase. */
        n_ctrl_pages++;
        n_pages++;
    }

    /* the last page is the index for a newly allocated page. */
    page_id = n_pages;

    /* allocate a new data page. */
    if(AllocateDataPage(fileHandle))
        return -1;

    return 0;    
} // }}}

unsigned int RM::getSchemaSize(const vector<Attribute> &attrs) // {{{
{
    uint16_t size = sizeof(uint16_t); /* field offset marker consumes uint16_t bytes */

    size += attrs.size() * sizeof(uint16_t); /* each field consumes uint16_t bytes. */

    for (unsigned int i = 0; i < attrs.size(); i++)
    {
        switch (attrs[i].type)
        {
            case TypeInt:
                 size += sizeof(int);
                 break;

            case TypeReal:
                 size += sizeof(float);
                 break;

            case TypeVarChar:
                 size += attrs[i].length;
                 break;
        }
    }

    return size;
} // }}}

RC RM::openTable(const string tableName, PF_FileHandle &fileHandle) // {{{
{
    if(open_tables.count(tableName))
    {
        /* open_tables stores a pointer to the handle, need to dereference for object. */
        fileHandle = *(open_tables[tableName]);

        return 0;
    }
    else
    {
        /* in order to have a handle persist in the map, it needs to be dynamically allocated. */
        PF_FileHandle *handle = new PF_FileHandle;
    
        /* open file and retrieve handle. */
        if(pf->OpenFile(tableName.c_str(), *handle))
        {
            /* deallocate the handle before returning. */
            delete handle;

            return -1;
        }
    
        /* copy the object. */
        fileHandle = *handle;
    
        /* cache handle for later use. */
        open_tables[tableName] = handle;
    
        return 0;
    }
} // }}}

RC RM::closeTable(const string tableName) // {{{
{
    /* check to make sure the table handle is cached. */
    if(open_tables.count(tableName))
    {
        /* read in the handle. */
        PF_FileHandle *handle = open_tables[tableName];

        /* close the handle. */
        if(handle->CloseFile())
            return -1;

        /* deallocate the handle. */
        delete handle;

        /* delete the entry from the map. */
        open_tables.erase(tableName);

        return 0;
    }

    /* table doesn't exist. */
    return -1;
} // }}}

RC RM::createTable(const string tableName, const vector<Attribute> &attrs) // {{{
{
    /* Handle used to write control page. */
    PF_FileHandle handle;

    /* check table name. */
    if(tableName.find_first_of("/.") != string::npos)
        return -1;

    /* check attribute name. */
    for(unsigned int i = 0; i < attrs.size(); i++)
        if(attrs[i].name.find_first_of("/.") != string::npos)
            return -1;

    /* no empty schema. */
    if (attrs.size() == 0)
        return -1;

    /* no empty varchar attributes. */
    for(unsigned int i = 0; i < attrs.size(); i++)
        if(attrs[i].type == TypeVarChar && attrs[i].length == 0)
            return -1;

    /* no duplicate attribute names. */
    map<string, int> dupes;
    for(unsigned int i = 0; i < attrs.size(); i++)
    {
        /* entry already exists, duplicate found. */
        if(dupes.count(attrs[i].name))
            return -1;

        dupes[attrs[i].name] = 1;
    }

    /* check schema fits in a page. */
    if(getSchemaSize(attrs) > PF_PAGE_SIZE)
        return -1;

    /* table exists. */
    if(catalog.count(tableName))
        return -1;

    /* create table. */
    catalog[tableName] = attrs;

    /* add fields for quick lookup (table.fieldname) */
    for(unsigned int i = 0; i < attrs.size(); i++)
        catalog_fields[tableName+"."+attrs[i].name] = attrs[i];

    /* create file & append a control page. */
    if(pf->CreateFile(tableName.c_str()))
        return -1;

    if(openTable(tableName, handle))
        return -1;

    /* write blank control page to page 0. */
    return(AllocateControlPage(handle));
} // }}}

RC RM::getAttributes(const string tableName, vector<Attribute> &attrs) // {{{
{
    /* table doesnt exists. */
    if(!catalog.count(tableName))
        return -1;

    attrs = catalog[tableName];

    return 0;
} // }}}

RC RM::deleteTable(const string tableName) // {{{
{
    vector<Attribute> attrs;

    /* table doesnt exists. */
    if(!catalog.count(tableName))
        return -1;

    /* retrieve table attributes. */
    if(getAttributes(tableName, attrs))
        return -1;

    /* delete all fields from catalog_fields. */
    for(unsigned int i = 0; i < attrs.size(); i++)
        catalog_fields.erase(tableName+"."+attrs[i].name);

    /* delete table. */
    catalog.erase(tableName);

    /* file is already open, close it, delete it from the open_tables map. */
    if(open_tables.count(tableName))
    {
        pf->CloseFile(*(open_tables[tableName]));
        open_tables.erase(tableName);
    }

    return(pf->DestroyFile(tableName.c_str()));
} // }}}

void RM::tuple_to_record(const void *tuple, char *record, const vector<Attribute> &attrs) // {{{
{
   char *tuple_ptr = (char *) tuple;
   char *record_data_ptr = record;

   uint16_t num_fields = attrs.size();

   /* last_offset is the relative offset of where to append data in a record.
      The data begins after the directory, (sizeof(num_fields) + 2*num_fields).
   */
   uint16_t last_offset = REC_START_DATA_OFFSET(num_fields);

   /* record data pointer points to where data can be appended. */
   record_data_ptr += last_offset;

   /* write the field count. */
   memcpy(record, &num_fields, sizeof(num_fields));

   for(unsigned int i = 0; i < attrs.size(); i++)
   {
       /* field offset address for the attribute. */
       uint16_t field_offset = REC_FIELD_OFFSET(i);

       if(attrs[i].type == TypeInt)
       {
           /* write the int starting at the last stored field offset. */
           memcpy(record_data_ptr, tuple_ptr, sizeof(int));

           /* advance pointer in record and tuple. */
           record_data_ptr += sizeof(int);
           tuple_ptr += sizeof(int);

           /* determine the new ending offset. */
           last_offset += sizeof(int);
       }
       else if(attrs[i].type == TypeReal)
       {
           /* write the float starting at the last stored field offset. */
           memcpy(record_data_ptr, tuple_ptr, sizeof(float));

           /* advance pointer in record and tuple. */
           record_data_ptr += sizeof(float);
           tuple_ptr += sizeof(float);

           /* determine the new ending offset. */
           last_offset += sizeof(float);
       }
       else if(attrs[i].type == TypeVarChar)
       {
           int length;

           /* read in the length as int. */
           memcpy(&length, tuple_ptr, sizeof(length));

           /* advance pointer past the length field in tuple. */
           tuple_ptr += sizeof(length);

           /* append varchar data to record. */
           memcpy(record_data_ptr, tuple_ptr, length);

           /* advance pointer in record and tuple. */
           tuple_ptr += length;
           record_data_ptr += length;

           /* determine the new ending offset. */
           last_offset += length;
       }

       /* write the end address for the field offset. */
       memcpy(record + field_offset, &last_offset, sizeof(last_offset));
   }
} // }}}

void RM::record_to_tuple(char *record, const void *tuple, const vector<Attribute> &attrs) // {{{
{
   char *tuple_ptr = (char *) tuple;
   char *record_data_ptr;

   uint16_t num_fields;

   /* read in the number of fields. */
   memcpy(&num_fields, record, sizeof(num_fields));

   /* find beginning of data. */
   uint16_t last_offset = REC_START_DATA_OFFSET(num_fields);
   record_data_ptr = record + last_offset;

   for(int i = 0; i < num_fields; i++)
   {
       /* field offset address for the attribute. */
       uint16_t field_offset = REC_FIELD_OFFSET(i);

       if(attrs[i].type == TypeInt)
       {
           /* copy the int to the tuple. */
           memcpy(tuple_ptr, record_data_ptr, sizeof(int));

           /* advance pointer in record and tuple. */
           record_data_ptr += sizeof(int);
           tuple_ptr += sizeof(int);

           /* determine the new ending offset. */
           last_offset += sizeof(int);
       }
       else if(attrs[i].type == TypeReal)
       {
           /* copy the float to the tuple. */
           memcpy(tuple_ptr, record_data_ptr, sizeof(float));

           /* advance pofloater in record and tuple. */
           record_data_ptr += sizeof(float);
           tuple_ptr += sizeof(float);

           /* determine the new ending offset. */
           last_offset += sizeof(float);
       }
       else if(attrs[i].type == TypeVarChar)
       {
           int length;
           uint16_t length_data;

           /* determine length from reading in the field offset, and subtract the last_offset. */
           memcpy(&length_data, record + field_offset, sizeof(length_data));
           length_data -= last_offset;

           /* copy the length data to an int representation. */
           length = length_data;

           /* write length data to tuple, and advance to where data should be appended. */
           memcpy(tuple_ptr, &length, sizeof(length));
           tuple_ptr += sizeof(length);

           /* append varchar data to tuple from last_offset to last_offset+length. */
           memcpy(tuple_ptr, record_data_ptr, length);

           /* advance pointer in record and tuple. */
           tuple_ptr += length;
           record_data_ptr += length;

           /* determine the new ending offset. */
           last_offset += length;
       }
   }
} // }}}

RC RM::insertTuple(const string tableName, const void *data, RID &rid) // {{{
{
    uint16_t record_length;
    unsigned int page_num;
    uint16_t space;

    /* buffer to store record. */
    static char record[PF_PAGE_SIZE];

    /* attributes to determine data packing format. */
    vector<Attribute> attrs;

    /* Handle for database. */
    PF_FileHandle handle;

    /* retrieve table attributes. */
    if(getAttributes(tableName, attrs))
        return -1;

   /* unpack data and convert into record format. Assumed to be a safe operation. */
   tuple_to_record(data, record, attrs);

   /* determine the size of the record. */
   record_length = REC_LENGTH(record);

   /* find usable data page lareg enough to store record, returns page_id. */
   //page_num = findBlankPage(handle, record_length);

   /* open table for insertion. */
   if(openTable(tableName, handle))
       return -1;

   /* find blank page for insertion. */
   if(findBlankPage(handle, record_length, page_num, space))
       return -1;
 

   /* update free space on page (can determine which control page by rid). */
   // updatePageSpace(handle, rid);

   return 0;
} // }}}
