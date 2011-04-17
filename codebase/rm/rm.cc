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

RC RM::createTable(const string tableName, const vector<Attribute> &attrs)
{
    return 1;
}

RC RM::deleteTable(const string tableName)
{
    return 1;
}

RC RM::getAttributes(const string tableName, vector<Attribute> &attrs)
{
    attrs.push_back((struct Attribute) { "a1", TypeInt, 0 });
    attrs.push_back((struct Attribute) { "a2", TypeReal, 0 });
    attrs.push_back((struct Attribute) { "a3", TypeVarChar, 500 });

    return 1;
}
