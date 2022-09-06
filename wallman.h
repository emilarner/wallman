#ifndef WALLMAN_H
#define WALLMAN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#include <errno.h>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include "protocol.h"
#include "config.h"

enum Operations
{
    StartSlideShowOp,
    StopSlideShowOp,
    SetWallpaperOp,
    ShowIndexOp,
    IncrementOp,
    DecrementOp
};

bool is_alphanumeric(const char *str);
void default_index_handler(int index, char *name);
int get_index(int fd, void (*indexhandler)(int index, char *name));
int start_slideshow(int fd, bool feh, bool wal, int delay);
int stop_slideshow(int fd);
struct Response set_wallpaper(int fd, bool feh, bool wal, char *filename);
struct Response set_wallpaper_by_index(int fd, bool feh, bool wal);
int increment_index(int fd);
int decrement_index(int fd);

#endif