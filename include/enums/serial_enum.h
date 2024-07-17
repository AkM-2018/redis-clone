#ifndef SERIAL_H
#define SERIAL_H

enum
{
    SER_NIL = 0, // Like `NULL`
    SER_ERR = 1, // An error code and message
    SER_STR = 2, // A string
    SER_INT = 3, // A int64
    SER_ARR = 4, // Array
};

#endif