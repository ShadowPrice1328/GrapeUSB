#ifndef ISO_H
#define ISO_H

typedef enum {
    ISO_UNKNOWN,
    ISO_WINDOWS,
    ISO_LINUX
} IsoType;

int mountISO(const char *iso);
void unmountISO();
IsoType detectISOType(const char *iso);
int validateISOArgument(const char* iso, IsoType* type);
int isValidISO(const char *iso);

#endif