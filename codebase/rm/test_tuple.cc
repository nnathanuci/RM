#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <fstream>
#include <iostream>
#include <cassert>

#include "rm.h"

using namespace std;

const int success = 0;

#define START_DATA_OFFSET(n_fields) (2*(n_fields) + 2)
#define FIELD_OFFSET(i) (2*(i) + 2)

void prepareTuple(const int name_length, const string name, const int age, const float height, const int salary, void *buffer, int *tuple_size)
{
    int offset = 0;
    
    memcpy((char *)buffer + offset, &name_length, sizeof(int));
    offset += sizeof(int);    
    memcpy((char *)buffer + offset, name.c_str(), name_length);
    offset += name_length;
    
    memcpy((char *)buffer + offset, &age, sizeof(int));
    offset += sizeof(int);
    
    memcpy((char *)buffer + offset, &height, sizeof(float));
    offset += sizeof(float);
    
    memcpy((char *)buffer + offset, &salary, sizeof(int));
    offset += sizeof(int);
    
    *tuple_size = offset;
}

// Function to parse the data in buffer and print each field
void printTuple(const void *buffer, const int tuple_size)
{
    int offset = 0;
    cout << "****Printing Buffer: Start****" << endl;
   
    int name_length = 0;     
    memcpy(&name_length, (char *)buffer+offset, sizeof(int));
    offset += sizeof(int);
    cout << "name_length: " << name_length << endl;
   
    char *name = (char *)malloc(100);
    memcpy(name, (char *)buffer+offset, name_length);
    name[name_length] = '\0';
    offset += name_length;
    cout << "name: " << name << endl;
    
    int age = 0; 
    memcpy(&age, (char *)buffer+offset, sizeof(int));
    offset += sizeof(int);
    cout << "age: " << age << endl;
  
    float height = 0.0; 
    memcpy(&height, (char *)buffer+offset, sizeof(float));
    offset += sizeof(float);
    cout << "height: " << height << endl;
       
    int salary = 0; 
    memcpy(&salary, (char *)buffer+offset, sizeof(int));
    offset += sizeof(int);
    cout << "salary: " << salary << endl;

    cout << "****Printing Buffer: End****" << endl << endl;    
}

void transform_tuple_to_record(void *tuple, char *record, const vector<Attribute> &attrs)
{
   char *tuple_ptr = (char *) tuple;
   char *record_data_ptr = record;

   unsigned short num_fields = attrs.size();

   /* last offset is used as the offset of where to append in the record. Beginning offset after directory.
      data begins after the directory, [2 for num_fields + 2*num_fields]
   */
   unsigned short last_offset = START_DATA_OFFSET(num_fields);

   /* record data pointer points to where data can be appended. */
   record_data_ptr += last_offset;

   /* write the field count. */
   memcpy(record, &num_fields, sizeof(num_fields));

   for(int i = 0; i < attrs.size(); i++)
   {
       /* field offset address for the attribute. */
       unsigned short field_offset = FIELD_OFFSET(i);

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

void transform_record_to_tuple(char *record, void *tuple, const vector<Attribute> &attrs)
{
   char *tuple_ptr = (char *) tuple;
   char *record_data_ptr;

   unsigned short num_fields;

   /* read in the number of fields. */
   memcpy(&num_fields, record, sizeof(num_fields));

   /* find beginning of data. */
   unsigned short last_offset = START_DATA_OFFSET(num_fields);
   record_data_ptr = record + last_offset;

   for(int i = 0; i < num_fields; i++)
   {
       /* field offset address for the attribute. */
       unsigned short field_offset = FIELD_OFFSET(i);

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
           unsigned short length_data;

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
}
int main()
{
    int tuple_size = 0;
    void *tuple = calloc(128,1); // originally 100
    void *data_returned = calloc(128,1); // originally 100

    char record[4096] = {0};

    vector<Attribute> attrs;

    Attribute attr;
    attr.name = "EmpName";
    attr.type = TypeVarChar;
    attr.length = (AttrLength)30;
    attrs.push_back(attr);

    attr.name = "Age";
    attr.type = TypeInt;
    attr.length = (AttrLength)4;
    attrs.push_back(attr);

    attr.name = "Height";
    attr.type = TypeReal;
    attr.length = (AttrLength)4;
    attrs.push_back(attr);

    attr.name = "Salary";
    attr.type = TypeInt;
    attr.length = (AttrLength)4;
    attrs.push_back(attr);

    
    string name1 = "Peters";
    int name_length = 6;
    int age = 6;
    float height = 170.1; 
    int salary = 5000;
    

    // Insert a tuple into a table
    prepareTuple(name_length, name1, age, height, salary, tuple, &tuple_size);
    cout << "Insert Data:" << endl;
    printTuple(tuple, tuple_size);

    /* My insert tuple. */
    transform_tuple_to_record(tuple, record, attrs);
    assert(memcmp(tuple, data_returned, 128) != 0);
    transform_record_to_tuple(record, data_returned, attrs);
    assert(memcmp(tuple, data_returned, 128) == 0);
    cout << "Data Returned:" << endl;
    printTuple(data_returned, tuple_size);

   
    FILE *f = fopen("test_tuple.data", "wb+");
    fwrite(record, PF_PAGE_SIZE, 1, f);
    fwrite(tuple, 128, 1, f);
    fwrite(data_returned, 128, 1, f);
}
