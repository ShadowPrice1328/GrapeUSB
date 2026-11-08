#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <ctype.h>

#include "ventoy.h"
#include "exec.h"
#include "devices.h"

#define VENTOY_SCRIPT_PATH "vendor/ventoy/Ventoy2Disk.sh"


static int confirmVentoyInstallation(const UsbDevice *dev)
{
    char confirmation[128];

    printf("\n");
    printf("========================================\n");
    printf("       WARNING: DESTRUCTIVE ACTION\n");
    printf("========================================\n\n");

    printf("Installing Ventoy will erase ALL existing data\n");
    printf("and recreate the partition table on this device.\n\n");

    printf("Device: %s\n", dev->dev_path);
    printf("Model:  %s\n", dev->model);
    printf("Size:   %s\n\n", dev->size);

    printf("To continue, type exactly: %s\n> ", dev->dev_path);
    fflush(stdout);

    if (fgets(confirmation, sizeof(confirmation), stdin) == NULL)
    {
        fprintf(stderr, "Cannot read confirmation\n");
        return 0;
    }

    if (strchr(confirmation, '\n') == NULL &&
        !feof(stdin))
    {
        int c;
        while ((c = getchar()) != '\n' && c != EOF)
            ;

        fprintf(stderr, "Confirmation too long\n");
        return 0;
    }

    confirmation[strcspn(confirmation, "\r\n")] = '\0';

    if (strcmp(confirmation, dev->dev_path) != 0)
    {
        printf("\nInstallation cancelled.\n");
        return 0;
    }

    printf("\nConfirmation accepted.\n");
    return 1;
}

static int isValidDeviceName(const char *name)
{
    if (name == NULL || name[0] == '\0')
        return 0;

    for (int i = 0; name[i] != '\0'; i++)
    {
        if (!isalnum((unsigned char)name[i]))
            return 0;
    }

    return 1;
}

int installVentoy(UsbDevice *dev)
{
    if (dev == NULL)
    {
        fprintf(stderr, "USB device is NULL\n");
        return -1;
    }

    if (!isValidDeviceName(dev->name))
    {
        fprintf(stderr, "Invalid USB device name\n");
        return -1;
    }

    char device_path[128];

    snprintf(device_path, sizeof(device_path), "/dev/%s", dev->name);

    if (strcmp(dev->dev_path, device_path) != 0)
    {
        fprintf(stderr, "USB device path mismatch\n");
        return -1;
    }

    struct stat st;

    if (stat(device_path, &st) != 0)
    {
        perror("Cannot access USB device");
        return -1;
    }

    if (!S_ISBLK(st.st_mode))
    {
        fprintf(stderr, "%s is not a block device\n", device_path);
        return -1;
    }

    if (access(VENTOY_SCRIPT_PATH, R_OK) != 0)
    {
        fprintf(stderr, "Ventoy installer not found: %s\n",
                VENTOY_SCRIPT_PATH);
        return -1;
    }

    if (validateUsbDeviceForVentoy(dev) != 0)
    {
        fprintf(stderr, "USB device validation failed\n");
        return -1;
    }

    UsbDevice current_dev = {0};

    if (!findUsbByName(dev->name, &current_dev))
    {
        fprintf(stderr, "Selected USB device is no longer available\n");
        return -1;
    }

    if (strcmp(current_dev.model, dev->model) != 0 ||
        strcmp(current_dev.size, dev->size) != 0)
    {
        fprintf(stderr, "USB device identity has changed!\n");
        return -1;
    }

    if (validateUsbDeviceForVentoy(&current_dev) != 0)
    {
        fprintf(stderr, "USB device validation failed\n");
        return -1;
    }

    if (!confirmVentoyInstallation(&current_dev))
        return -1;

    printf("Installing Ventoy on %s...\n", current_dev.dev_path);

    char *cmd[] = {
        "bash",
        VENTOY_SCRIPT_PATH,
        "-i",
        "-S",
        current_dev.dev_path,
        NULL
    };

    return run_checked(cmd);
}