#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>

#include "ventoy.h"
#include "exec.h"
#include "devices.h"

#define VENTOY_SCRIPT_PATH "vendor/ventoy/Ventoy2Disk.sh"

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

    printf("Installing Ventoy on %s...\n", device_path);

    char *cmd[] = {
        "bash",
        VENTOY_SCRIPT_PATH,
        "-i",
        device_path,
        NULL
    };

    return run_checked(cmd);
}