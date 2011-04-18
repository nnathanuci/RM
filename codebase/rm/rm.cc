
#include "rm.h"
#include <stdio.h>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string.h>

RM* RM::_rm = 0;
PF_Manager* RM::_pm;

map<string, vector<Attribute> > RM::catalog;

static const char* attributeLoc = "./attributes.catalog";

RM* RM::Instance()
{
    if(!_pm)
    	_pm = PF_Manager::Instance();

    if(!_rm)
        _rm = new RM();
    
    //If .catalog file exists this will open and close it, otherwise will create it.
    fclose(fopen(attributeLoc, "ab+"));

    return _rm;
}

RM::RM()
{
}

RM::~RM()
{
    if(!_rm)
    	delete _rm;
    //delete RM::catalog;
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

//Max table name     = 128 bytes. SQL
//Max attribute name = 128 bytes. SQL
//Int size = 2 bytes
//nullable = 1 byte

//Catalog representation:
//table	attribute  position type max_size nullable
//vchar	vchar	   int 	    int	 int	  char


RC RM::createTable(const string tableName, const vector<Attribute> &attrs)
{
	PF_FileHandle handle;
	//table_name attribute_name position type max_size nullable

	unsigned int a_size = attrs.size();
	catalog[tableName] = attrs;
	for (unsigned int i = 0; i < a_size; i++)
	{
		string key = tableName + "." + attrs[i].name;
		catalog[key] = attrs;
		cout << key << endl;
	}

	for (unsigned int i = 0; i < catalog[tableName + "." + attrs[0].name].size(); i++)
	{
		cout << " VALUE " << catalog[tableName + "." + attrs[0].name][i].name << endl;
	}

	//RM::catalog[tableName] = attrs;

	//_pm->OpenFile(attributeLoc , handle);
	//_pm->CloseFile(handle);

	//A table maps to a single file, and a single file contains only one table.
	return _pm->CreateFile(tableName.c_str());
}

RC RM::getAttributes(const string tableName, vector<Attribute> &attrs)
{
	attrs = catalog[tableName];
	return 0;
}

RC RM::deleteTable(const string tableName)
{
    return 1;
}
