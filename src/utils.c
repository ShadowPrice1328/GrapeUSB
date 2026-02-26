#include <ctype.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "utils.h"
#include "exec.h"
#include "iso.h"

#define MNT_USB_PATH "/mnt/grapeusb_usb"
#define MNT_ISO_PATH "/mnt/grapeusb_iso"

int fileExists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

void checkRoot()
{
    if (geteuid() != 0) {
        fprintf(stderr, "Run as root!\n");
        exit(EXIT_FAILURE);
    }
}

int commandExists(const char *cmd)
{
    char *path = getenv("PATH");
    if (!path) return 0;

    char *path_copy = strdup(path);
    char *dir = strtok(path_copy, ":");

    while (dir)
    {
        char full[512];
        snprintf(full, sizeof(full), "%s/%s", dir, cmd);

        if (access(full, X_OK) == 0)
        {
            free(path_copy);
            return 1;
        }

        dir = strtok(NULL, ":");
    }

    free(path_copy);
    return 0;
}

int checkDependencies(IsoType iso)
{
    const char *common_deps[] = {
        "lsblk",
        "mkfs.fat",
        "mount",
        "umount",
        "rsync",
        "sync",
        NULL
    };

    const char *windows_deps[] = {
        "wimlib-imagex",
        NULL
    };

    for (int i = 0; common_deps[i] != NULL; i++)
    {
        if (!commandExists(common_deps[i]))
        {
            fprintf(stderr, "Missing dependency: %s\n", common_deps[i]);
            return 0;
        }
    }

    if (iso == ISO_WINDOWS)
    {
        for (int i = 0; windows_deps[i] != NULL; i++)
        {
            if (!commandExists(windows_deps[i]))
            {
                fprintf(stderr, "Missing dependency (Windows .iso only): %s\n", windows_deps[i]);
                return 0;
            }
        }
    }

    return 1;
}

void printTime() 
{
    time_t t = time(NULL);
    struct tm lcltm = *localtime(&t);
    
    printf("\n┌───────────────────────────┐\n");
    printf("│  %d-%02d-%02d %02d:%02d:%02d      │\n", 
        lcltm.tm_year + 1900, lcltm.tm_mon + 1, lcltm.tm_mday,
        lcltm.tm_hour, lcltm.tm_min, lcltm.tm_sec);
    printf("└───────────────────────────┘\n");
}

void flushInput() 
{
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

int getCharInput()
{
    int c = getchar();
    flushInput();
    return c;
}

int splitWimIfNeeded() 
{
    char *split[] = 
    {
        "wimlib-imagex", "split",
        MNT_ISO_PATH "/sources/install.wim",
        MNT_USB_PATH "/sources/install.swm",
        "3800", NULL
    };

    return run_checked(split);
}

int copyFiles(IsoType type) 
{
    if (access(MNT_ISO_PATH, R_OK) != 0)
    {
        fprintf(stderr, "ISO not mounted or not readable\n");
        return -1;
    }

    if (access(MNT_USB_PATH, R_OK) != 0)
    {
        fprintf(stderr, "USB not mounted or not readable\n");
        return -1;
    }

    if (type == ISO_WINDOWS)
    {
        char *copy_base[] = {
            "rsync", "-ah", "--progress", 
            "--no-perms", "--no-owner", "--no-group",
            "--exclude", "sources/install.wim", 
            "--exclude", "sources/install.esd",
            MNT_ISO_PATH "/", MNT_USB_PATH "/", NULL
        };

        if (run_checked(copy_base) != 0)
            return -1;

        char wim_path[256];
        snprintf(wim_path, sizeof(wim_path), "%s/sources/install.wim", MNT_ISO_PATH);

        if (access(wim_path, F_OK) == 0)
        {
            struct stat st;
            stat(wim_path, &st);
            if (st.st_size > 4294967295LL)
            {
                if (splitWimIfNeeded() != 0)
                    return -1;
            }
            else
            {
                char *copy_wim[] = {"cp", wim_path, MNT_USB_PATH "/sources/", NULL};

                if (run_checked(copy_wim) != 0)
                    return -1;
            }
        }
    }
    else
    {
        char *copy_linux[] = {"rsync", "-ah", "--progress", MNT_ISO_PATH "/", MNT_USB_PATH "/", NULL};
        
        if (run_checked(copy_linux) != 0)
            return -1;

        return 0;
    }

    char *sync_cmd[] = {"sync", NULL};
    
    if (run_checked(sync_cmd) != 0)
        return -1;

    return 0;
}

void formatPartPath(UsbDevice *dev)
{
    snprintf(dev->dev_path, sizeof(dev->dev_path), "/dev/%s", dev->name);

    size_t len = strlen(dev->name);

    if (len + 2 >= sizeof(dev->part_path))
    {
        fprintf(stderr, "Path too long for partition\n");
        dev->part_path[0] = '\0';
        return;
    }

    memcpy(dev->part_path, dev->dev_path, len);

    if (isdigit(dev->name[strlen(dev->name) - 1]))
    {
        dev->part_path[len] = 'p';
        dev->part_path[len + 1] = '1';
        dev->part_path[len + 2] = '\0';
    }
    else
    {
        dev->part_path[len] = '1';
        dev->part_path[len + 1] = '\0';
    }
}