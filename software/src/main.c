#include <stdio.h>
#include <stdlib.h>
#include "file_handler.h"
#include "SerialComm.h"
#include "args_parser.h"

// Define true to not include bool.h
#define true 1

// Non-standard SerialComm Signals
#define PORT_SIG     'S'
#define PORT_WRITE   'W'
#define PORT_READ    'R'
#define PORT_P_EN    'E'
#define PORT_P_DIS   'D'
#define PORT_DUMP    'B'

#define eprintf(args...) fprintf(stderr, args)

static char* executable_name = NULL;

/* Update this to be more accurate */
void print_usage()
{
    printf("Usage: %s PORT OPTION\n", executable_name);
    printf("PORT: Serial port file\n");
    printf("OPTIONS:\n");
    printf("\t-r [filename]\t\tRead the contents of the EEPROM, optional write those contents into a file\n");
    printf("\t-w <filename>\t\tWrite an image from a file to the EEPROM\n");
    printf("\t-v <filename>\t\tVerify data on EEPROM against an image\n");
    printf("\t-e <filename>\t\tEnable write protection\n");
    printf("\t-d <filename>\t\tDisable write protection\n");

    exit(EXIT_FAILURE);
}

int get_device_signature(struct SerialComm* device_port)
{
    puts("Awaiting device signature...");

    SerialCommSendByte(device_port, PORT_SIG);  // Request device signature
    SerialCommAwaitStatus(device_port);         // Await for the device to acknowledge

    if(device_port->status == PORT_TIMEOUT)     // Device has not responded
    {
        eprintf("Devices has not responded. Timing out...\n");
        return 0;
    }

    if(device_port->status != PORT_ACK)
    {
        eprintf("Devices has not acknowledged signature request.\n");
        return 0;
    }

    SerialCommAwaitBytes(device_port, 4);
    SerialCommReadBytes(device_port, 4);

    // Print device signature
    printf("Device firmware version: %d.%d.%d\n", device_port->receive_buffer[0], device_port->receive_buffer[1], device_port->receive_buffer[2]);

    // Ensure transmission was ended with a newline
    if(device_port->receive_buffer[3] != 0x0A)
        printf("Warning: Transmission did not end with a newline character\n");

    return 1;
}

// Send the image size to the device and validate correct echo of size
int SendImageSize(struct SerialComm* port, uint32_t size)
{
    SerialCommSendU32(port, size);          // Send the image size
    SerialCommAwaitStatus(port);            // Await for an ACK

    if(port->status != PORT_ACK)
    {
        eprintf("Device did not acknowledge image size receive\n");
        return 0;
    }

    uint32_t r_size = SerialCommReadU32(port);

    if(port->status == PORT_TIMEOUT)
    {
        eprintf("Port timed out awaiting u32 value\n");
        return 0;
    }

    if(r_size != size)
    {
        SerialCommSendByte(port, PORT_NAK);
        eprintf("Image size did not echo correct (0x%08X) [%02X %02X %02X %02X]\n", r_size, port->receive_buffer[3], port->receive_buffer[2], port->receive_buffer[1], port->receive_buffer[0]);
        return 0;
    }

    SerialCommSendByte(port, PORT_ACK);

    return 1;
}

int main(int argc, char** argv)
{
    executable_name = argv[0];  // First argument is the name of the file being executed

    // Check if at the minimum a serial port file name is provided
    if(argc < 2) print_usage();

    // We remove the executable name and serial port file name from the args
    struct Arguments args = ParseArguments(argc - 2, argv + 2);

    // Exit if there has been an error processing the arguments
    if(!args.parsed) print_usage();

    // Make an alias for the serial ports file name
    char* serial_port_name = argv[1];

    // Exit program if no mode argument was provided
    if(!args.mode) print_usage();

    struct SerialComm port;

    /* Open the serial port */
    SerialCommOpenPort(&port, serial_port_name, 0x200);
    if(port.port_fd < 0){ perror("Unable to open serial port"); return -1; }

    /* Set up serial port */
    SerialCommSetBaudrate(&port, B115200);
    SerialCommSetTimeout(&port, 5);
    SerialCommSetLSBFirst(&port, true);

    /* Apply settings to serial port */
    SerialCommApplyOptions(&port);

    /* Port is now ready for serial communication */

    // Obtain device signature to ensure we are communicating with the correct device
    if(!get_device_signature(&port))
    {
        SerialCommClosePort(&port);
        return -1;
    }

    switch(args.mode)
    {
        // Request device to print EEPROM contents
        case MODE_READ:
        {
            // If the read command was follow by a file name it mean we want to dump the contents
            if(!args.output)
            {
                SerialCommSendByte(&port, PORT_READ);
                while(1)
                {
                    while(!SerialCommDataAvailable(&port)) continue;
                    size_t bytes_received = SerialCommReadPortAll(&port);   // Read all data from the serial port into the buffer
                    port.receive_buffer[bytes_received] = '\0';
                    printf("%s", port.receive_buffer); // Print buffer contents

                    if(port.receive_buffer[bytes_received - 1] == 0) // Check if last transmitted byte was a null byte
                        break; // If so transmission ended
                }
                break;
            }

            if(!args.size)
            {
                eprintf("No file size was provided for the dump\n");
                return 1;
            }

            // Improve image size parser
            uint32_t image_size = ParseImageSize(args.size);

            SerialCommSendByte(&port, PORT_DUMP);   // Request a dump of the EEPROM
            if(!SendImageSize(&port, image_size))   // Error message will be already printed by SendImageSize
                return -1;

            // Open dump file for writing

            FILE* dump = fopen(args.output, "wb");
            if(!dump){ perror("Unable to open dump file for writing"); SerialCommClosePort(&port); return -1; }

            size_t bytes_received = 0;
            size_t bytes_received_wrap = 0;
            size_t kb_received = 0;

            printf("Dumping:");
            fflush(stdout);

            // Ready to receive data
            SerialCommSendByte(&port, PORT_RDY);

            while(bytes_received < image_size)
            {
                size_t bytes_read = 0;
                while(!SerialCommDataAvailable(&port)) continue;

                bytes_read = SerialCommReadPortAll(&port);
                fwrite(port.receive_buffer, 1, bytes_read, dump);
                bytes_received += bytes_read;
                bytes_received_wrap += bytes_read;

                if(bytes_received_wrap >= 1024)
                {
                    kb_received++;
                    printf(" %zuK", kb_received);
                    fflush(stdout);
                    bytes_received_wrap -= 1024;
                }
            }

            printf("\n");

            /* Close the dump file */
            fclose(dump);

        } break;

        case MODE_VERIFY:
        {
            if(!args.input)
            {
                fprintf(stderr, "No image was provided to verify the EEPROM's data against\n");
                return -1;
            }

            FILE* out_file = NULL;

            /* If an output file is provided */
            if(args.output)
            {
                out_file = fopen(args.output, "w");
                if(!out_file){ perror("Unable to open output file"); return 1; }
            }

            /* Open file to compare EEPROM data against */
            FILE* image = fopen(args.input, "rb");
            if(!image){ perror("Unable to open image file"); return 1; }

            uint32_t image_size = FileSize(image);

            SerialCommSendByte(&port, PORT_DUMP);   // Request a dump of the EEPROM
            if(!SendImageSize(&port, image_size))   // Error message will be already printed by SendImageSize
                return -1;

            // Open file for writing

            FILE* eeprom_data = tmpfile();
            if(!eeprom_data){ perror("Unable to open dump file for writing"); SerialCommClosePort(&port); return -1; }

            size_t bytes_received = 0;
            size_t bytes_received_wrap = 0;
            size_t kb_received = 0;

            printf("Dumping:");
            fflush(stdout);

            // Ready to receive data
            SerialCommSendByte(&port, PORT_RDY);

            while(bytes_received < image_size)
            {
                size_t bytes_read = 0;
                while(!SerialCommDataAvailable(&port)) continue;

                bytes_read = SerialCommReadPortAll(&port);
                fwrite(port.receive_buffer, 1, bytes_read, eeprom_data);
                bytes_received += bytes_read;
                bytes_received_wrap += bytes_read;

                if(bytes_received_wrap >= 1024)
                {
                    kb_received++;
                    printf(" %zuK", kb_received);
                    fflush(stdout);
                    bytes_received_wrap -= 1024;
                }
            }

            rewind(eeprom_data);

            printf("\nVerifying: ");
            fflush(stdout);

            int ok = 1;

            for(size_t i = 0; i < image_size; i++)
            {
                uint8_t image_byte;
                uint8_t eeprom_byte;

                fread(&image_byte, 1, 1, image);
                fread(&eeprom_byte, 1, 1, eeprom_data);

                if(image_byte != eeprom_byte)
                {
                    if(ok)
                    {
                        printf("BAD\n");
                        ok = 0;
                    }

                    printf("Invalid byte at address 0x%04zX, Expected: %02hhX, Read: %02hhX\n", i, image_byte, eeprom_byte);

                    if(out_file)
                    {
                        fprintf(out_file, "%04zX: %02X, %02X\n", i, image_byte, eeprom_byte);
                    }
                }
            }

            if(ok) printf("OK\n");

            /* Close the dump file */
            if(out_file) fclose(out_file);
            fclose(eeprom_data);
            fclose(image);
        } break;

        // Enable software protection on the EEPROM
        case MODE_PROT_EN:
            SerialCommSendByte(&port, PORT_P_EN);
            puts("EEPROM write protection enabled.");
            break;

        // Disable software protection on the EEPROM
        case MODE_PROT_DIS:
            SerialCommSendByte(&port, PORT_P_DIS);
            puts("EEPROM write protection disabled.");
            break;
        
        case MODE_WRITE:
        {
            if(!args.input){ eprintf("No image filename provided\n"); print_usage(); }

            FILE* image_file = fopen(args.input, "rb");
            if(!image_file){ perror("Unable to open image file"); SerialCommClosePort(&port); return -1; }

            uint32_t image_size = FileSize(image_file);
            uint8_t* image_data = malloc(image_size);

            fread(image_data, 1, image_size, image_file);
            fclose(image_file);

            printf("Image size is 0x%08X\n", image_size);
            puts("Requesting to write to EEPROM");

            SerialCommSendByte(&port, PORT_WRITE);  // Request to write to EEPROM
            if(!SendImageSize(&port, image_size))   // Error message will be already printed by SendImageSize
                return -1;

            size_t pages_sent = 0;

            printf("Writing:");
            fflush(stdout);

            while((pages_sent * 256) < image_size)
            {
                SerialCommAwaitStatus(&port); // Await ready signal

                if(port.status == PORT_TIMEOUT)
                {
                    puts("\nPort timed out exiting...");
                    return 1;
                }

                // Fix this
                if(port.status == PORT_ERR)
                {
                    SerialCommReadBytes(&port, 3);
                    if(port.status == PORT_TIMEOUT)
                    {
                        eprintf("The port timed out while reading device error\n");
                        return 1;
                    }

                    printf("The serial port has sent an error status, Addr: 0x%02lX%02hhX E: 0x%02hhX R: 0x%02hhX\n", pages_sent - 1, port.receive_buffer[0], port.receive_buffer[1], port.receive_buffer[2]);
                    SerialCommAwaitStatus(&port);
                }

                // This should error or block
                if(port.status != PORT_RDY){ printf("Device sent unexpected signal [%2hhX] (Awaiting ready)\n", port.status); return 1; }

                for(size_t i = 0; i < 256; i++)
                {
                    SerialCommSendByte(&port, image_data[(pages_sent * 256) + i]);
                }

                SerialCommAwaitStatus(&port); // Await acknowledge (resent page if not acknowledged)

                if(port.status != PORT_ACK){ puts("Device did not acknowledge, resending page..."); /* Finish this */ }
                // else puts("ACK");

                pages_sent++;
                if((pages_sent * 256) % 1024 == 0)
                {
                    printf(" %luK", (pages_sent * 256) / 1024);
                    fflush(stdout);
                }
            }

            free(image_data);
            puts("");
        } break;
    }

    SerialCommClosePort(&port);
    return 0;
}