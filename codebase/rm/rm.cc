#include "rm.h"
#include <cstdio>
#include <fstream>
#include <iostream>
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

//Write the first x bytes of the value val into the first x bytes starting from s.
void write_first_x_bytes_as_int(const unsigned int x, char* s, const unsigned int val)
{
	for (unsigned int i = 0; i < x; i++)
		s[i] = (val >> (bitsInByte*i) ) & 0xFF ;
}

//Produce the header for the attributes and write to string.
/* Writes the record into header.
 * Record Format: count|FO_0|...|FO_n|FO_E|F_0|...|F_n|END
 * Count contains n, the number of fields in the record.
 * There are n + 1 field offsets (FO) in the record, the first n point to the first byte of the
 * respective field. The last points to the last byte of the record. Each offset is bytesPerOffset bytes long.
 * Ints occupy bytesPerInt bytes.
 * Reals occupy bytesPerReal bytes.
 * Currently VarChars occupy the number of bytes
 */
RC RM::produceHeader(const vector<Attribute> &attrs, char* header)
{
	unsigned int a_size = attrs.size();
	unsigned int total_header_bytes = ( a_size + 2 ) * bytesPerOffset;//A slot for the last offset, and another for the initial count.
	memset( header, 0, total_header_bytes * sizeof(char) ); //Clear the header
	write_first_x_bytes_as_int(bytesPerOffset, header, a_size); //The first bytesPerOffset of the record contain num of records.
	unsigned int current = total_header_bytes; //Starts at end of header, advanced until end of record.
	char* current_ptr = header + bytesPerOffset; //Starts past the count, advanced until end of header.
	//For each attribute record the offset of the attribute.
	for (unsigned int i = 0; i < a_size; i++)
	{
		if ( attrs[i].type == TypeInt)
		{
			write_first_x_bytes_as_int(bytesPerOffset, current_ptr, current);
			current += bytesPerInt;
		}
		else if ( attrs[i].type == TypeReal)
		{
			write_first_x_bytes_as_int(bytesPerOffset, current_ptr, current);
			current += bytesPerReal;
		}
		else //varchar.
		{
			write_first_x_bytes_as_int(bytesPerOffset, current_ptr, current);
			current += attrs[i].length;
		}
		current_ptr += bytesPerOffset; //Advance to position for next offset.
	}
	write_first_x_bytes_as_int(bytesPerOffset, current_ptr, current); //Fills in the offset indicating the record end.
	return 0;
}

/* generates a control page format provided a page buffer. */
void blank_control_page(char *page)
{
   /* all bits are set to indicate each page (12 bits per page) has full free space. */
   memset(page, 0xFF, PF_PAGE_SIZE-1);
   page[PF_PAGE_SIZE-1] = 0;
}

unsigned int RM::getSchemaSize(const vector<Attribute> &attrs)
{
    rec_offset_t size = sizeof(rec_offset_t); /* field offset marker consumes rec_offset_t bytes */

    size += attrs.size() * sizeof(rec_offset_t); /* each field consumes rec_offset_t bytes. */

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
}

RC RM::openTable(const string tableName, PF_FileHandle &fileHandle)
{
    /* check if already open, if so, return file handle. */
    if(open_tables.count(tableName))
    {
        fileHandle = open_tables[tableName];
        return 0;
    }

    /* Open file. */
    if(pf->OpenFile(tableName.c_str(), fileHandle))
        return -1;

    /* Keep handle for later use. */
    open_tables[tableName] = fileHandle;

    return 0;
}

RC RM::createTable(const string tableName, const vector<Attribute> &attrs)
{
    /* A blank control page will be written as the first page. */
    char ctrl_page[PF_PAGE_SIZE];

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
    pf->CreateFile(tableName.c_str());

    if(openTable(tableName, handle))
        return -1;

    /* write blank control page to page 0. */
    blank_control_page(ctrl_page);
    return (handle.AppendPage((void *) ctrl_page));
}

RC RM::getAttributes(const string tableName, vector<Attribute> &attrs)
{
    /* table doesnt exists. */
    if(!catalog.count(tableName))
        return -1;

    attrs = catalog[tableName];

    return 0;
}

RC RM::deleteTable(const string tableName)
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
        pf->CloseFile(open_tables[tableName]);
        open_tables.erase(tableName);
    }

    return(pf->DestroyFile(tableName.c_str()));
}

void tuple_to_record(const void *tuple, char *record, const vector<Attribute> &attrs)
{
   char *tuple_ptr = (char *) tuple;
   char *record_data_ptr = record;

   rec_offset_t num_fields = attrs.size();

   /* last_offset is the relative offset of where to append data in a record.
      The data begins after the directory, (sizeof(num_fields) + 2*num_fields).
   */
   rec_offset_t last_offset = START_DATA_OFFSET(num_fields);

   /* record data pointer points to where data can be appended. */
   record_data_ptr += last_offset;

   /* write the field count. */
   memcpy(record, &num_fields, sizeof(num_fields));

   for(unsigned int i = 0; i < attrs.size(); i++)
   {
       /* field offset address for the attribute. */
       rec_offset_t field_offset = FIELD_OFFSET(i);

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
}

RC RM::insertTuple(const string tableName, const void *data, RID &rid)
{
    rec_offset_t record_length;
    unsigned page_num;

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
   record_length = RECORD_LENGTH(record);

   /* find usable data page lareg enough to store record, returns page_id. */
   //page_num = findBlankPage(handle, record_length);

   /* open table for insertion. */
   if(openTable(tableName, handle))
      return -1;

   /* insert record */

   /* update free space on page (can determine which control page by rid). */
   // updatePageSpace(handle, rid);
}
