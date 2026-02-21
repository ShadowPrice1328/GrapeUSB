#include <stdio.h>
#include <string.h>

#include "utils.h"
#include "exec.h"
#include "iso.h"

#define MNT_ISO_PATH "/mnt/grapeusb_iso"

#include <sys/stat.h>
#include <unistd.h>

int mountISO(const char *iso)
{
    struct stat st;

    if (stat(MNT_ISO_PATH, &st) != 0)
    {
        if (mkdir(MNT_ISO_PATH, 0755) != 0)
        {
            perror("Failed to create ISO mount directory");
            return -1;
        }
    }

    char *cmd[] = {
        "mount",
        "-o", "loop",
        (char*)iso,
        MNT_ISO_PATH,
        NULL
    };

    return run_checked(cmd);
}

void unmountISO()
{
    char *cmd[] = {"umount", MNT_ISO_PATH, NULL};
    run(cmd);
}

int isValidISO(const char *iso)
{
    char *blkid[] = {"blkid", "-o", "value", "-s", "TYPE", (char*)iso, NULL};
    return run(blkid) == 0;
}

IsoType detectISOType(const char *iso)
{
    if (strstr(iso, "win") || strstr(iso, "Win"))
        return ISO_WINDOWS;
    return ISO_LINUX;
}

int validateISOArgument(const char* iso, IsoType* type)
{
    if (!fileExists(iso)) {
        fprintf(stderr, "ISO file does not exist: %s\n", iso);
        return 1;
    }

    if (!isValidISO(iso))
    {
        fprintf(stderr, "ISO file is not valid: %s\n", iso);
        return 1;
    }

    *type = detectISOType(iso);
    if (*type == ISO_UNKNOWN)
    {
        fprintf(stderr, "ISO file type is unsupported: %s\n", iso);
        return 1;
    }

    if (*type == ISO_WINDOWS)
        printf("Detected Windows ISO\n");

    if (*type == ISO_LINUX)
        printf("Detected Linux ISO\n");

    return 0;
}
