#include <stdlib.h>
#include <stdio.h>
#include <time.h>

typedef enum {
    MENU,
    MAIN_INFO,
    EXIT
} Screen;

void printMenu();

void flushInput() 
{
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

void clearScreen()
{
    #ifdef _WIN32
        system("cls");
    #else 
        system("clear");
    #endif
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

Screen showMenu()
{
    clearScreen();

    system("clear");
    printf("Choose one from the options below:\n");
    printf(" [1] Main information\n");
    printf(" [0] Exit\n");
    printf("\nEnter choice: ");

    int input = getchar();
    flushInput();

    if (input == '1') return MAIN_INFO;
    if (input == '0') return EXIT;

    printf("\nWrong option. Press Enter to continue...");
    getchar();

    return MENU;
}

Screen showMainInfo() 
{
    clearScreen();

    printf("\033[47;30m");
    printf("  ★ MAIN INFO ★  \n");
    printf("\033[0m");

    printf("===================================================\n");
    printf(" Welcome to the \"Bootable Flashdrive\" Utility!\n");
    printf("===================================================\n\n");

    printf("This utility helps you with creation of a bootable USB flashdrive from a .iso file.\n\n");
    printf(" [Z] Back to Menu\n");
    printf("Enter choice: ");

    int input = getchar();
    flushInput();

    if (input == 'z' || input == 'Z')
        return MENU;

    return MAIN_INFO;
}

int main(int argc, char* argv[]) 
{
    Screen current = MENU;

    while (current != EXIT)
    {
        switch (current)
        {
            case MENU:
                current = showMenu();
                break;
            case MAIN_INFO:
                current = showMainInfo();
                break;
            default:
                current = EXIT;
        }
    }

    clearScreen();
    printf("Thank you, goodbye\n");

    return 0;
}
