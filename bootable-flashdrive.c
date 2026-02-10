#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <stdbool.h>

void printMenu();

void flushInput() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

void printTime() {
    system("clear");

    time_t t = time(NULL);
    struct tm lcltm = *localtime(&t);
    printf("\n┌───────────────────────────┐\n");
    printf("│  %d-%02d-%02d %02d:%02d:%02d      │\n", 
        lcltm.tm_year + 1900, lcltm.tm_mon + 1, lcltm.tm_mday,
        lcltm.tm_hour, lcltm.tm_min, lcltm.tm_sec);
    printf("└───────────────────────────┘\n");
}

void printMainInfo() {
    while (1)
    {
        system("clear");

        printf("\033[47;30m");
        printf("  ★ MAIN INFO ★  \n");
        printf("\033[0m");

        printf("This utility helps you with creation of a bootable USB flashdrive from a .iso file.\n\n");
        printf(" [Z] Back to Menu\n");
        printf("Enter choice: ");

        int input = getchar();
        flushInput();

        if (input == 'z' || input == 'Z')
            printMenu();
    }
}

void printMenu() {
    while (1) {
        system("clear");
        printf("Choose one from the options below:\n");
        printf(" [1] Main information\n");
        printf(" [0] Exit\n");
        printf("Enter choice: ");

        int input = getchar();
        flushInput();

        if (input == '1') 
        {
            printTime();
            printMainInfo();
        } 
        else if (input == '0') 
        {
            exit(0);
        } else 
        {
            printf("Wrong option, try again.\n\n");
        }
    }
}

int main(int argc, char* argv[]) {
    printTime();

    printf("===================================================\n");
    printf(" Welcome to the \"Bootable Flashdrive\" Utility!\n");
    printf("===================================================\n\n");

    printMenu();

    return 0;
}
