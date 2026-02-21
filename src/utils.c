#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>

#include "usb.h"
#include "utils.h"
#include "exec.h"

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