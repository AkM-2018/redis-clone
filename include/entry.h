#ifndef ENTRY_H
#define ENTRY_H

#include <string.h>

#include "hashtable.h"

struct Entry
{
    struct HNode node;
    std::string key;
    std::string val;
};

#endif