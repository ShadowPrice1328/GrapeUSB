#ifndef UTILS_H
#define UTILS_H

#include "iso.h"

int fileExists(const char *path);
void checkRoot(void);
void printTime(void);
void flushInput(void);
int getCharInput(void);
int commandExists(const char *cmd);
int checkDependencies(IsoType iso);
int askUser(const char *message);

#endif