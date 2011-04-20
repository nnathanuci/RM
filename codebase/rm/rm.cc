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
    unsigned int size = 2; // number of fields marker

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

    getAttributes(tableName, attrs);

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
