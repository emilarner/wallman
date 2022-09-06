#ifndef WALLMAND_H
#define WALLMAND_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>


#include <pthread.h>
#include <errno.h>

#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/un.h>

#include "config.h"
#include "protocol.h"
#include "sarray.h"


struct Wallman
{
    sarray *files;
    short index;

    bool slideshow;
    char wallpapers_path[256];

};

struct Client 
{
    int fd;
    struct Wallman *wallman;
};

struct Slideshow
{
    struct Wallman *wallman;
    uint32_t delay;
    uint8_t feh;
    uint8_t wal;
};


void list_files(struct Wallman *wallman, int fd);
void refresh_files(struct Wallman *wallman);
void *slideshow_thread(void *info);

int file_complications(struct Wallman *wallman, char *path);
void set_background_feh(struct Wallman *wallman, char *path);
void set_background_wal(struct Wallman *wallman, char *path);
int set_background(struct Wallman *wallman, bool feh, bool wal, char *path);

void *client_handler(void *client);

#endif