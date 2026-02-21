#ifndef UTILS_H
#define UTILS_H

#include "iso.h"

int fileExists(const char *path);
void checkRoot();
void printTime();
void flushInput();
int getCharInput();
int splitWimIfNeeded();
int copyFiles(IsoType type);

#endif