#include <stdlib.h>
#include <stdio.h>
#include <time.h>

void printTime()
{
    time_t t = time(NULL);
    struct tm lcltm = *localtime(&t);
    printf("|--- %d-%02d-%02d %02d:%02d:%02d ---|\n", lcltm.tm_year + 1900, lcltm.tm_mon + 1, lcltm.tm_mday, lcltm.tm_hour, lcltm.tm_min, lcltm.tm_sec);
}

void printMenu()
{
    printf("[1] Main information");
}

int main(int argc, char* argv[])
{
    printTime();
    printf("Welcome to the \"bootable-fleshdrive\" application!\n");
}
