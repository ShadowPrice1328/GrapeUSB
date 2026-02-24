#include <stdio.h>

#include "utils.h"
#include "devices.h"
#include "ui.h"
#include "iso.h"

int main(int argc, char* argv[]) 
{
    checkRoot();

    if (argc != 3)
    {
        printf("Usage: %s path/to/.iso /dev/sdX (or \"0\" if not known)\n", argv[0]);
        return 1;
    }

    IsoType isoType = ISO_UNKNOWN;

    if (validateISOArgument(argv[1], &isoType) == 1)
        return 1;

    UsbDevice dev_data = {0};
    char *dev = NULL;

    if (strcmp(argv[2], "0") != 0) 
    {
        dev = argv[2];

        if (!findUsbByName(dev, &dev_data))
            dev = NULL;
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
        }
    }

    clearScreen();
    printf("Thank you, goodbye =)\n");
    
    return 0;
}