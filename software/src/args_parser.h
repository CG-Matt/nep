#pragma once

// These are the valid modes that the program can operate in

#define MODE_READ       (char)'r'
#define MODE_WRITE      (char)'w'
#define MODE_PROT_EN    (char)'e'
#define MODE_PROT_DIS   (char)'d'
#define MODE_VERIFY     (char)'v'

struct Arguments
{
    char* input;
    char* output;
    char* size;
    char mode;
    int parsed;
};

struct Arguments ParseArguments(int arg_count, char** args);

size_t ParseImageSize(const char* size_string);