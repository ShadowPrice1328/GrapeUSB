#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "utils.h"
#include "devices.h"
#include "ui.h"
#include "iso.h"

int main(int argc, char *argv[])
{
    checkRoot();

    if (argc != 3)
    {
        fprintf(stderr,
                "Usage: %s path/to/.iso /dev/sdX (or \"0\" if not known)\n",
                argv[0]);
        return EXIT_FAILURE;
    }

    IsoType isoType = ISO_UNKNOWN;

    if (validateISOArgument(argv[1], &isoType) != 0)
        return EXIT_FAILURE;

    if (!checkDependencies(isoType))
        return EXIT_FAILURE;

    UsbDevice dev_data = {0};

    if (strcmp(argv[2], "0") != 0)
    {
        if (!findUsbByName(argv[2], &dev_data))
        {
            fprintf(stderr, "USB device not found: %s\n", argv[2]);
            return EXIT_FAILURE;
        }
    }

    Screen current = MENU;

    while (current != EXIT)
    {
        switch (current)
        {
            case MENU:
                current = showMenu();
                break;

            case MAIN_INFO:
                current = showMainInfo();
                break;

            case DEVICES:
                current = showDevices(&dev_data);
                break;

            case BEGIN:
                current = showBeginCreation(&dev_data, isoType);
                break;

            case START:
                if (dev_data.name[0] == '\0')
                {
                    current = DEVICES;
                    break;
                }

                current = showStartCreation(&dev_data, argv[1], isoType);
                break;

            default:
                current = EXIT;
                break;
        }
    }

    clearScreen();
    printf("Thank you, goodbye =)\n");

    return EXIT_SUCCESS;
}