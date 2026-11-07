#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "iso.h"

IsoType detectISOType(const char *iso)
{
    if (strstr(iso, "win") || strstr(iso, "Win"))
        return ISO_WINDOWS;
    return ISO_LINUX;
}

int validateISOArgument(const char *iso, IsoType *type)
{
    if (iso == NULL || type == NULL || iso[0] == '\0')
    {
        fprintf(stderr, "Invalid ISO argument\n");
        return 1;
    }

    struct stat st;

    if (stat(iso, &st) != 0 || !S_ISREG(st.st_mode))
    {
        fprintf(stderr, "ISO file does not exist or is not regular\n");
        return 1;
    }

    const char *extension = strrchr(iso, '.');

    if (extension == NULL || strcasecmp(extension, ".iso") != 0)
    {
        fprintf(stderr, "File must have .iso extension\n");
        return 1;
    }

    *type = detectISOType(iso);

    printf("ISO file validated: %s\n", iso);

    return 0;
}