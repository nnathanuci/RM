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
    closeAllTables();
}

RC RM::AllocateControlPage(PF_FileHandle &fileHandle) // {{{
{
    /* buffer to write control page. */
    static uint8_t page[PF_PAGE_SIZE];

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
    static uint8_t page[PF_PAGE_SIZE] = {0};

    uint16_t *slot_page = (uint16_t *) page;

    /* all fields are 0, except: num_slots, for 1 unallocated slot, and slot 0 stores slot queue end marker. */
    slot_page[SLOT_NUM_SLOT_INDEX] = 1;
    slot_page[SLOT_GET_SLOT_INDEX(0)] = SLOT_QUEUE_END;

    return(fileHandle.AppendPage(page));
} // }}}

RC RM::getPageSpace(PF_FileHandle &fileHandle, unsigned int page_id, uint16_t &unused_space) // {{{
{
    /* buffer to read in the control page. */
    uint16_t ctrl_page[CTRL_MAX_PAGES];

    unsigned int ctrl_page_id;
    uint16_t page_id_offset;

    /* get number of allocated pages. */
    unsigned int n_pages = fileHandle.GetNumberOfPages();

    /* make sure the page_id is valid. */
    if (page_id >= n_pages || CTRL_IS_CTRL_PAGE(page_id))
        return -1;

    /* determine the absolute control page id for a given page. */
    ctrl_page_id = CTRL_GET_CTRL_PAGE(page_id);

    /* get the offset in the control page. */
    page_id_offset = CTRL_GET_CTRL_PAGE_OFFSET(page_id);


    /* read in control page. */
    if(fileHandle.ReadPage(ctrl_page_id, (void *) ctrl_page))
        return -1;

    /* return the unused space in the page. */
    unused_space = ctrl_page[page_id_offset];

    return 0;
} // }}}

RC RM::decreasePageSpace(PF_FileHandle &fileHandle, unsigned int page_id, uint16_t space) // {{{
{
    /* buffer to read in the control page. */
    uint16_t ctrl_page[CTRL_MAX_PAGES];

    unsigned int ctrl_page_id;
    uint16_t page_id_offset;

    /* get number of allocated pages. */
    unsigned int n_pages = fileHandle.GetNumberOfPages();

    /* make sure the page_id is valid. */
    if (page_id >= n_pages || CTRL_IS_CTRL_PAGE(page_id))
        return -1;


    /* determine the absolute control page id for a given page. */
    ctrl_page_id = CTRL_GET_CTRL_PAGE(page_id);

    /* get the offset in the control page. */
    page_id_offset = CTRL_GET_CTRL_PAGE_OFFSET(page_id);

    /* read in control page. */
    if(fileHandle.ReadPage(ctrl_page_id, (void *) ctrl_page))
        return -1;

    /* cannot decrease the space if more than what is unused. */
    if(space > ctrl_page[page_id_offset])
        return -1;

    /* decrease the space for page on control page. */
    ctrl_page[page_id_offset] -= space;

    /* write back the control page. */
    if(fileHandle.WritePage(ctrl_page_id, (void *) ctrl_page))
        return -1;

    return 0;
} // }}}

RC RM::increasePageSpace(PF_FileHandle &fileHandle, unsigned int page_id, uint16_t space) // {{{
{
    /* buffer to read in the control page. */
    uint16_t ctrl_page[CTRL_MAX_PAGES];

    unsigned int ctrl_page_id;
    uint16_t page_id_offset;

    /* get number of allocated pages. */
    unsigned int n_pages = fileHandle.GetNumberOfPages();

    /* make sure the page_id is valid. */
    if (page_id >= n_pages || CTRL_IS_CTRL_PAGE(page_id))
        return -1;

    /* determine the absolute control page id for a given page. */
    ctrl_page_id = CTRL_GET_CTRL_PAGE(page_id);

    /* get the offset in the control page. */
    page_id_offset = CTRL_GET_CTRL_PAGE_OFFSET(page_id);

    /* read in control page. */
    if(fileHandle.ReadPage(ctrl_page_id, (void *) ctrl_page))
        return -1;

    /* cannot increase space beyond maximum. */
    if((space + ctrl_page[page_id_offset]) > SLOT_MAX_SPACE)
        return -1;

    /* increase the space for page on control page. */
    ctrl_page[page_id_offset] += space;

    /* write back the control page. */
    if(fileHandle.WritePage(ctrl_page_id, (void *) ctrl_page))
        return -1;

    return 0;
} // }}}

RC RM::getDataPage(PF_FileHandle &fileHandle, uint16_t length, unsigned int &page_id, uint16_t &unused_space) // {{{
{
    /* read control page as an array of uint16_t. */
    uint16_t ctrl_page[CTRL_MAX_PAGES];

    /* get number of allocated pages. */
    unsigned int n_pages = fileHandle.GetNumberOfPages();

    /* calculate number of control/data pages. */
    unsigned int n_ctrl_pages = CTRL_NUM_CTRL_PAGES(n_pages);
    unsigned int n_data_pages = CTRL_NUM_DATA_PAGES(n_pages);

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

        /* read in control page. */
        if(fileHandle.ReadPage(ctrl_page_id, (void *) ctrl_page))
            return -1;

        for(unsigned int j = 0; j < n_allocated_pages; j++)
        {
            /* found a free page, consume the space, and return the page id. */
            if(length <= ctrl_page[j])
            {
                /* set the page_id with the available space which is returned to the caller. */
                page_id = ctrl_page_id + (j + 1); // page index is offset by 1.

                /* set the unused_space variable which will be returned to the caller. */
                unused_space = ctrl_page[j];

                return 0;
            }
        }
    }

    /* allocate a control page only if there are no pages, or the last control page is completly full and the last page isn't a control page. */
    if((n_pages == 0) || ((n_data_pages % CTRL_MAX_PAGES) == 0) && (CTRL_PAGE_ID(n_ctrl_pages - 1) != (n_pages - 1)))
    {
        if(AllocateControlPage(fileHandle))
            return -1;

        /* number of pages increase. */
        n_ctrl_pages++;
        n_pages++;
    }

    /* allocate a new data page. */
    if(AllocateDataPage(fileHandle))
        return -1;

    /* number of pages increase. */
    n_data_pages++;
    n_pages++;

    /* the last page is the index for a newly allocated page. */
    page_id = n_pages - 1;

    /* unused space is the full page since it is freshly allocated. */
    unused_space = SLOT_MAX_SPACE;

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
    PF_FileHandle handle;

    /* if table is open, retrieve and return handle. */
    if(open_tables.count(tableName))
    {
        fileHandle = open_tables[tableName];

        return 0;
    }

    /* table not open: open table, retrieve and cache handle. */

    /* open file and retrieve handle. */
    if(pf->OpenFile(tableName.c_str(), handle))
        return -1;
 
    /* cache handle for later use. */
    open_tables[tableName] = handle;

    /* return handle. */
    fileHandle = handle;
 
    return 0;
} // }}}

RC RM::closeTable(const string tableName) // {{{
{
    /* check to make sure the table handle is cached. */
    if(open_tables.count(tableName))
    {
        /* read in the handle. */
        PF_FileHandle handle = open_tables[tableName];

        /* close the handle. */
        if(handle.CloseFile())
            return -1;

        /* delete the entry from the map. */
        open_tables.erase(tableName);

        return 0;
    }

    /* table isn't open. */
    return 0;
} // }}}

RC RM::closeAllTables() // {{{
{
    /* need a pass to close handles first, and then delete map entries, since iterating must be non-destructive. */
    for (map<string, PF_FileHandle>::const_iterator it = open_tables.begin(); it != open_tables.end(); ++it)
    {
        PF_FileHandle handle = it->second;

        if(handle.CloseFile())
            return -1;
    }

    /* all handles are now closed, delete them from the map. */
    open_tables.clear();

    return 0;
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

    return 0;
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

    if(closeTable(tableName))
        return -1;

    return(pf->DestroyFile(tableName.c_str()));
} // }}}

void RM::tuple_to_record(const void *tuple, uint8_t *record, const vector<Attribute> &attrs) // {{{
{
   uint8_t *tuple_ptr = (uint8_t *) tuple;
   uint8_t *record_data_ptr = record;

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

void RM::record_to_tuple(uint8_t *record, const void *tuple, const vector<Attribute> &attrs) // {{{
{
   uint8_t *tuple_ptr = (uint8_t *) tuple;
   uint8_t *record_data_ptr;

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

uint16_t RM::activateSlot(uint16_t *slot_page, uint16_t activate_slot_id, uint16_t record_offset) // {{{
{
    /* update slot queue head. */
    if(SLOT_GET_SLOT(slot_page, activate_slot_id) == SLOT_QUEUE_END)
    {
        /* reached last slot in the queue, which means we need to allocate a new slot. */

        /* check to see if enough space is available to allocate a new slot. */
        if(SLOT_GET_FREE_SPACE(slot_page) < sizeof(uint16_t))
            return 0;

        /* assign new slot index which happens to be the current number of slots. */
        slot_page[SLOT_NEXT_SLOT_INDEX] = SLOT_GET_NUM_SLOTS(slot_page);

        /* update the number of slots in the directory. */
        slot_page[SLOT_NUM_SLOT_INDEX]++;

        /* deactivate slot and set it to end of queue marker. */
        slot_page[SLOT_GET_LAST_SLOT_INDEX(slot_page)] = SLOT_QUEUE_END;

        /* activate slot and update offset. */
        slot_page[SLOT_GET_SLOT_INDEX(activate_slot_id)] = record_offset;

        /* return 1 to indicate a new slot is created. */
        return 1;
    }
    else
    {
        /* the queue has more than 1 element, so we just copy the next element from the last allocated slot. */
        slot_page[SLOT_NEXT_SLOT_INDEX] = SLOT_GET_INACTIVE_SLOT(slot_page, activate_slot_id);

        /* activate slot and update offset. */
        slot_page[SLOT_GET_SLOT_INDEX(activate_slot_id)] = record_offset;

        /* no new slots allocated. */
        return 0;
    }
} // }}}

RC RM::insertTuple(const string tableName, const void *data, RID &rid) // {{{
{
    uint16_t record_length;
    unsigned int page_id;

    uint8_t raw_page[PF_PAGE_SIZE];
    uint16_t *slot_page = (uint16_t *) raw_page;

    /* free space management. */
    uint16_t free_space_offset;
    uint16_t free_space_avail;

    /* available space of a page. */
    uint16_t avail_space;

    /* slot used for insert. */
    uint16_t insert_slot;

    /* set to 1 if the slot directory is expanded. */
    int new_slot_flag = 0;

    /* buffer to store record. */
    static uint8_t record[PF_PAGE_SIZE];

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

   /* open table for insertion. */
   if(openTable(tableName, handle))
       return -1;

   /* find data page for insertion, (guaranteed to fit record). */
   if(getDataPage(handle, record_length, page_id, avail_space))
       return -1;

   /* update page id to return. */ 
   rid.pageNum = page_id;

   if(handle.ReadPage(page_id, raw_page))
       return -1;

   debug_data_page(raw_page);

   /* get the free space offset, and determine available space. */
   free_space_avail = SLOT_GET_FREE_SPACE(slot_page);
   free_space_offset = SLOT_GET_FREE_SPACE_OFFSET(slot_page);

   /* It can fit in the free space. Append the record at the free space offset. */
   if(record_length <= free_space_avail)
   {
       uint16_t new_offset;

       /* free space offset must always align on an even boundary (debug check). */
       if(!(IS_EVEN(free_space_offset)))
           return -1;

       /* append the record to the page beginning at the free space offset. */
       memcpy(raw_page + free_space_offset, record, record_length);

       /* calculate new offset, which is the offset after the record is appended at the free space offset. */
       new_offset = free_space_offset + record_length;

       /* align it to even boundary (ignore the one-byte fragment). */
       if(IS_ODD(new_offset))
           new_offset++;

       /* update free space offset (aligned to an even boundary). */
       slot_page[SLOT_FREE_SPACE_INDEX] = new_offset;

       /* assign slot from the head of the slot queue. */
       insert_slot = slot_page[SLOT_NEXT_SLOT_INDEX];

       /* update slot number to return. */
       rid.slotNum = insert_slot;

       /* activate slot setting it to the old free space offset, where the record was inserted. */
       new_slot_flag = activateSlot(slot_page, insert_slot, free_space_offset);
   }
   else
   {
       /* XXX: need to compact, and then insert. */
   }

   /* update space in the control page. */
   decreasePageSpace(handle, page_id, record_length + new_slot_flag*sizeof(uint16_t));

   debug_data_page(raw_page);

   return 0;
} // }}}

void RM::debug_data_page(uint8_t *raw_page) // {{{
{
    uint16_t begin_fragment_offset = SLOT_INVALID_ADDR; /* points to invalid address. */
    uint16_t record_slot = SLOT_INVALID_ADDR; /* points to invalid address. */

    uint16_t offset_to_slot_map[SLOT_HASH_SIZE];

    uint16_t *slot_page = (uint16_t *) raw_page;

    uint16_t num_slots = SLOT_GET_NUM_SLOTS(slot_page);
    uint16_t free_space_offset = SLOT_GET_FREE_SPACE_OFFSET(slot_page);
    uint16_t free_space_avail = SLOT_GET_FREE_SPACE(slot_page);

    cout << "[BEGIN PAGE DUMP]" << endl;
    cout << "free space: " << free_space_avail << endl;
    cout << "free space offset: " << free_space_offset << endl;
    cout << "num slots:  " << num_slots << endl;

    /* read in the slot directory, create a map. */

    /* invalidate offset map. */ 
    memset(offset_to_slot_map, 0xFF, PF_PAGE_SIZE);

    for(uint16_t i = 0; i < SLOT_GET_NUM_SLOTS(slot_page); i++)
    {
        /* slot_index points to the data position in the slot directory relative to the page. */
        uint16_t slot_index = SLOT_GET_SLOT_INDEX(i);

        /* slot_directory[i] offset. */
        uint16_t offset = slot_page[slot_index];

        if(SLOT_IS_ACTIVE(offset))
        {
            cout << "slot " << i << ": active" << endl;
            offset_to_slot_map[SLOT_HASH_FUNC(offset)] = i;
        }
        else
        {
            cout << "slot " << i << ": inactive" << endl;
        }
    }

    /* scan through finding all fragments and records until free space is reached. */
    for(uint16_t i = 0; i < free_space_offset; i++)
    {
        /* reached a fragment word, determine if it's the start of a fragment. */
        if (slot_page[i] == SLOT_FRAGMENT_WORD)
        {
            /* if not set, set the offset to the beginning of the fragment. */
            if(begin_fragment_offset == SLOT_INVALID_ADDR)
                begin_fragment_offset = i;
        }
        else
        {
            /* reached end of fragment, output it. */
            if(i - begin_fragment_offset > 1)
            {
                cout << "Fragment [start: " << begin_fragment_offset << "]: length=" << (i - begin_fragment_offset) << endl;

                /* reset the fragment pointer. */
                begin_fragment_offset = SLOT_INVALID_ADDR;
            }

            record_slot = offset_to_slot_map[SLOT_HASH_FUNC(i)];
            cout << "Record [slot: " << record_slot << " start: " << i << "]: length=" << REC_LENGTH(raw_page + i) << endl;

            /* skip to end of record aligned on even byte, minus one for the post increment. */
            i += IS_EVEN(REC_LENGTH(raw_page + i)) ? REC_LENGTH(raw_page + i) - 1 : REC_LENGTH(raw_page + i);
        }
    }
} // }}}

// functions undefined {{{
RC RM::deleteTuples(const string tableName) { };

RC RM::deleteTuple(const string tableName, const RID &rid) { return -1; }

// Assume the rid does not change after update
RC RM::updateTuple(const string tableName, const void *data, const RID &rid) { return -1; }

RC RM::readTuple(const string tableName, const RID &rid, void *data) { return -1; }

RC RM::readAttribute(const string tableName, const RID &rid, const string attributeName, void *data) { return -1; }

RC RM::reorganizePage(const string tableName, const unsigned pageNumber) { return -1; }

// scan returns an iterator to allow the caller to go through the results one by one.
RC RM::scan(const string tableName, 
      const vector<string> &attributeNames, // a list of projected attributes
      RM_ScanIterator &rm_ScanIterator) { return -1; }

void RM::debug_data_page(unsigned int page_id) { return; }

// }}}

