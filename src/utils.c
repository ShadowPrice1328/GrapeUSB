#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "utils.h"

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