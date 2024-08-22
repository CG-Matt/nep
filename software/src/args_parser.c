#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "args_parser.h"

#define eprintf(args...) fprintf(stderr, args)

struct Arguments ParseArguments(int argc, char** args)
{
    // Setup output struct
    struct Arguments out;
    out.input = NULL;
    out.output = NULL;
    out.size = NULL;
    out.mode = 0;
    out.parsed = 0;

    for(int i = 0; i < argc; i++)
    {
        char* cur_arg = args[i];

        // No need to check for a minimum length of 1 as length of 0 is not possible
        if(*cur_arg == '-')
        {
            if(strlen(cur_arg) < 2)
            {
                eprintf("No argument provided after argument marker.\n");
                return out;
            }

            if(strlen(cur_arg) > 2)
            {
                eprintf("Unrecognised argument '%s'.\n", cur_arg);
                return out;
            }

            char arg = cur_arg[1];

            switch(arg)
            {
                // Modes
                case 'r':
                case 'w':
                case 'e':
                case 'd':
                case 'v':
                    if(out.mode){ eprintf("Mode set more than once.\n"); return out; }

                    out.mode = arg;
                    break;

                // Input file set
                case 'i':
                    if(out.input){ eprintf("Duplicate input file argument provided.\n"); return out; }
                    if(i + 1 >= argc){ eprintf("Expected input file name after '-i' argument\n"); return out; }

                    out.input = args[i + 1];
                    break;

                // Output file set
                case 'o':
                    if(out.output){ eprintf("Duplicate output file argument provided.\n"); return out; }
                    if(i + 1 >= argc){ eprintf("Expected output file name after '-o' argument\n"); return out; }

                    out.output = args[i + 1];
                    break;

                // Size set
                case 's':
                    if(out.size){ eprintf("Duplicate size argument provided.\n"); return out; }
                    if(i + 1 >= argc){ eprintf("Expected size after '-s' argument\n"); return out; }
                    
                    out.size = args[i + 1];
                    break;

                default:
                    eprintf("Unknown argument '%s'\n", cur_arg);
                    return out;
            }
        }
    }

    out.parsed = 1;
    return out;
}

size_t ParseImageSize(const char* const_size_str)
{
    size_t len = strlen(const_size_str);
    size_t multiplier = 0;
    size_t image_size = 0;

    // We need to make a copy of the size string to edit it
    char* size_str = malloc(len);
    memcpy(size_str, const_size_str, len);

    // Make this into a 10 left bit shift

    if(size_str[len - 1] == 'K')
    {
        multiplier = 1024;
        size_str[len - 1] = '\0'; // Mark character as parsed
    }

    image_size = atoi(size_str);

    image_size *= multiplier;

    free(size_str);
    return image_size;
}