#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

/* The different requests that can be made to the wallman daemon. */
enum RequestTypes
{
    StartSlideshow,
    StopSlideshow,
    SetWallpaper,
    Refresh,
    IncrementIndex,
    DecrementIndex,
    GetIndex,
    SetWallpaperByIndex
};

struct __attribute__((__packed__)) Indices
{
    uint16_t no; 
};

/* The request object. */ 
struct __attribute__((__packed__)) Request 
{
    uint8_t req_type;
    uint8_t data_len;
};


enum ResponseTypes
{
    SetWallpaperResp,
    StartSlideshowResp,
    StopSlideshowResp
};

enum ResponseCodes
{
    Success,
    Boolean,
    ErrnoReturned,
    FilenameTooLong,
    SlideshowAlready,
    SlideshowNoent,
    DelayError,
    IndexWrapAround,
    RefreshSlideShowActive 
};

static const char *strerrorwallmand[] = {
    "Success.",
    "Boolean sent was incorrect.",
    "A file errno was returned, please use strerror() on it.",
    "Filename was too long",
    "A slideshow is already in progress!",
    "There isn't a slideshow in progress!",
    "Invalid delay sent.",
    "The index was wrapped back around"
};

struct __attribute__((__packed__)) Response
{
    uint8_t resp_type;
    uint8_t err; // an errno
    uint8_t code;
};

#endif