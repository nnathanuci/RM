
#include "rm.h"

RM* RM::_rm = 0;

RM* RM::Instance()
{
    if(!_rm)
        _rm = new RM();
    
    return _rm;
}

RM::RM()
{
}

RM::~RM()
{
}

RC createTable(const string tableName, const vector<Attribute> &attrs)
{
    return 1;
}

RC deleteTable(const string tableName)
{
    return 1;
}

RC getAttributes(const string tableName, vector<Attribute> &attrs)
{
    return 1;
}

RC foo(const string &t)
{
  cout << t;
  return 0;
}
