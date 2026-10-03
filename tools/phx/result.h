#ifndef PHX_RESULT_H
#define PHX_RESULT_H

#include <types.h>

typedef PHX_Byte PHX_Result;

#define PHX_SUCCESS 0
#define PHX_ERROR_INTERNAL 1
#define PHX_ERROR_MEMORY 2
#define PHX_ERROR_IO 3
#define PHX_ERROR_FORMAT 4
#define PHX_ERROR_SIZE 6
#define PHX_ERROR_PARAMETER 7
#define PHX_ERROR_PERMISSION 8
#define PHX_ERROR_NOT_SUPPORTED 9
#define PHX_ERROR_OUT_OF_SPACE 10
#define PHX_ERROR_NOT_FOUND 11
#define PHX_ERROR_NAME_TOO_LONG 12


typedef struct
{
    PHX_Result code;
    const char* msg;
} PHX_DetailedResult;


#endif
