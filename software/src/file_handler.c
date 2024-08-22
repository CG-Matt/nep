#include "file_handler.h"
#include <sys/stat.h>

size_t FileSize(FILE* stream)
{
    struct stat f_info;
    fstat(fileno(stream), &f_info);
    return f_info.st_size;
}