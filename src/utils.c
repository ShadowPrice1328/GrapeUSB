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
    (void)iso;

    const char *deps[] = {
        "bash",
        "lsblk",
        "udevadm",
        "mount",
        "umount",
        "rsync",
        "sync",
        "findmnt",
        NULL
    };

    for (int i = 0; deps[i] != NULL; i++)
    {
        if (!commandExists(deps[i]))
        {
            fprintf(stderr, "Missing dependency: %s\n", deps[i]);
            return 0;
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