#include <stdio.h>
#include <unistd.h>

#include "utils.h"
#include "ui.h"
#include "devices.h"

void clearScreen()
{
    printf("\033c");
}

Screen showDevices(UsbDevice *dev_data) 
{
    clearScreen();

    printf("\033[47;30m  ★ DEVICES ★  \n\033[0m\n");

    UsbDevice devs[16];
    int n = getUsbDevices(devs, 16);

    if (n == 0) 
    {
        printf("No USB flash drives detected.\n\n");
        printf(" [Z] Back to Menu\n");
        printf("\nEnter choice: ");

        int input = getCharInput();
        if (input == 'z' || input == 'Z') return MENU;
        return DEVICES;
    }

    printf("Detected USB flash drives:\n\n");

    for (int i = 0; i < n; i++) 
    {
        if (dev_data->name[0] != '\0' && strcmp(devs[i].name, dev_data->name) == 0) 
        {
            printf("\033[47;30m [%d] %-10s %-8s %s\033[0m\n", 
                    i + 1, devs[i].name, devs[i].size, devs[i].model);
        } 
        else 
        {
            printf(" [%d] %-10s %-8s %s\n",
                   i + 1, devs[i].name, devs[i].size, devs[i].model);
        }
    }

    printf("\n [1-%d] Select device\n [Z] Back to Menu\n\nEnter choice: ", n);
    int input = getCharInput();

    if (input >= '1' && input <= '0' + n) 
    {
        int idx = input - '1';
        *dev_data = devs[idx];

        formatPartPath(dev_data);

        return DEVICES;
    }

    if (input == 'z' || input == 'Z') 
        return MENU;

    return DEVICES;
}

Screen showBeginCreation(UsbDevice *dev_data, IsoType isoType) 
{
    clearScreen();

    printf("\033[47;30m  ★ CREATE BOOTABLE USB FLASHDRIVE ★  \n\n\033[0m");

    const char* isoStr = (isoType == ISO_WINDOWS) ? "Windows" : "Linux";
    printf("ISO Type: %s\n", isoStr);

    if (dev_data == NULL || dev_data->name[0] == '\0')
    {
        printf("No USB device selected!\n\n");
        printf("Please select a USB drive first.\n\n");

        printf(" [1] Show available devices\n");
        printf(" [Z] Back to Menu\n");

        printf("\nEnter choice: ");
        int input = getCharInput();

        if (input == '1') return DEVICES;
        if (input == 'z' || input == 'Z') return MENU;

        return BEGIN;
    }

    if (isoType == ISO_UNKNOWN)
    {
        printf("No valid ISO detected!\n");
        printf("Please provide a valid Windows or Linux ISO.\n\n");
        printf(" [Z] Back to Menu\n");

        printf("\nEnter choice: ");
        int input = getCharInput();

        if (input == 'z' || input == 'Z') return MENU;

        return BEGIN;
    }

    printf("Selected flashdrive: %s (%s, %s)\n\n", dev_data->dev_path, dev_data->size, dev_data->model);

    printf("Is it correct? [Y/N]: ");
    int input = getCharInput();

    if (input == 'y' || input == 'Y')
        return START;
    if (input == 'n' || input == 'N')
        return DEVICES;

    return BEGIN;
}

Screen showStartCreation(UsbDevice *dev_data, const char* iso, IsoType isoType)
{
    if (!hasEnoughSpace(iso, dev_data)) 
    {
        printf("\033[1;31mError: Not enough space on %s!\033[0m\n", dev_data->dev_path);
        printf("Press Enter to go back...");
        getchar();

        return DEVICES;
    }

    clearScreen();
    printf("\033[1;31m!!! WARNING: ALL DATA ON %s WILL BE ERASED !!!\033[0m\n\n", dev_data->dev_path);

    const char* isoStr = (isoType == ISO_WINDOWS) ? "Windows" : "Linux";

    printf("Selected ISO: %s\n", iso);
    printf("ISO type: %s\n", isoStr);
    printf("Selected flashdrive: %s (%s, %s)\n\n", dev_data->dev_path, dev_data->size, dev_data->model);

    printf("Are you absolutely sure? [Y/N]: ");
    
    int input = getCharInput();
    
    if (input == 'y' || input == 'Y') 
    {
        printTime();
        printf("\n>>> Starting the process. This may take a while...\n");

        if (create_bootable(iso, dev_data, isoType) != 0)
        {
            printf("\n\033[1;31mError occurred during creation!\033[0m\n");
            printf("Press Enter to return...");
            flushInput();
            return MENU;
        }

        printTime();
        printf("\n\033[1;32m★ Success! Bootable USB created. ★\033[0m\n");
        printf("Press Enter to return to menu...");
        getchar();

        return MENU;
    } 
    else 
    {
        printf("Operation cancelled.\n");
        sleep(1);
        return MENU;
    }
}

Screen showMainInfo() 
{
    clearScreen();

    printf("\033[47;30m");
    printf("  ★ ABOUT ★  \n");
    printf("\033[0m\n");

    printf("  █████████                                          █████  █████  █████████  ███████████\n"); 
    printf(" ███░░░░░███                                        ░░███  ░░███  ███░░░░░███░░███░░░░░███\n");
    printf("███     ░░░  ████████   ██████   ████████   ██████   ░███   ░███ ░███    ░░░  ░███    ░███\n");
    printf("░███         ░░███░░███ ░░░░░███ ░░███░░███ ███░░███ ░███   ░███ ░░█████████  ░██████████ \n");
    printf("░███    █████ ░███ ░░░   ███████  ░███ ░███░███████  ░███   ░███  ░░░░░░░░███ ░███░░░░░███\n");
    printf("░░███  ░░███  ░███      ███░░███  ░███ ░███░███░░░   ░███   ░███  ███    ░███ ░███    ░███\n");
    printf(" ░░█████████  █████    ░░████████ ░███████ ░░██████  ░░████████  ░░█████████  ███████████ \n");
    printf("  ░░░░░░░░░  ░░░░░      ░░░░░░░░  ░███░░░   ░░░░░░    ░░░░░░░░    ░░░░░░░░░  ░░░░░░░░░░░  \n");
    printf("                                  ░███                                                    \n");
    printf("                                  █████                                                   \n");
    printf("                                 ░░░░░                                                    \n\n");
    printf("===================================================\n");
    printf(" Welcome to the \"GrapeUSB\" Utility!\n");
    printf("===================================================\n\n");

    printf("This utility helps you with creation of a bootable USB flashdrive from a .iso file.\n");
    printf("Plug your USB flashdrive in -> Select it in DEVICES -> Proceed to BEGIN\n");
    printf("Do not unplug it during the process, for your own sake! Even though I've done everything to prevent an unpleasant outcomes =]\n");
    printf("That's it.\n");

    printf("\n──────────────────────────────\n");
    printf("  With love,\n");
    printf("  \tShadowPrice\n");
    printf("──────────────────────────────\n");

    printf("\n [Z] Back to Menu\n");
    printf("\nEnter choice: ");

    int input = getCharInput();

    if (input == 'z' || input == 'Z')
        return MENU;

    return MAIN_INFO;
}

Screen showMenu()
{
    clearScreen();

    printf("Choose one from the options below:\n\n");
    printf(" [1] About\n");
    printf(" [2] Show available devices\n");
    printf(" [3] Begin\n");
    printf(" [Z] Exit\n");
    printf("\nEnter choice: ");

    int input = getCharInput();

    if (input == '1') return MAIN_INFO;
    if (input == 'z' || input == 'Z') return EXIT;
    if (input == '2') return DEVICES;
    if (input == '3') return BEGIN;

    printf("\nWrong option. Press Enter to continue...");
    flushInput();

    return MENU;
}