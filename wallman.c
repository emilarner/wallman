#include "wallman.h"

int start_slideshow(int fd, bool feh, bool wal, int delay)
{
    struct Request req;
    req.data_len = 0;
    req.req_type = StartSlideshow;

    send(fd, &req, sizeof(req), 0);
    send(fd, &delay, 4, 0);
    send(fd, (uint8_t*) &feh, 1, 0);
    send(fd, (uint8_t*) &wal, 1, 0);

    struct Response resp;
    recv(fd, &resp, sizeof(resp), 0);

    return resp.code;
}

int stop_slideshow(int fd)
{
    struct Request req;
    req.data_len = 0;
    req.req_type = StopSlideshow;

    send(fd, &req, sizeof(req), 0);

    struct Response resp;
    recv(fd, &resp, sizeof(resp), 0);
    return resp.code; 
}

struct Response set_wallpaper(int fd, bool feh, bool wal, char *filename)
{
    struct Request req;
    req.req_type = SetWallpaper;
    
    req.data_len = strlen(filename);

    send(fd, &req, sizeof(req), 0);

    send(fd, (uint8_t*) &feh, 1, 0);
    send(fd, (uint8_t*) &wal, 1, 0);

    send(fd, filename, req.data_len, 0);

    struct Response resp;
    recv(fd, &resp, sizeof(resp), 0);

    return resp;
}

struct Response set_wallpaper_by_index(int fd, bool feh, bool wal)
{
    struct Request req;
    req.req_type = SetWallpaperByIndex;
    send(fd, &req, sizeof(req), 0);

    send(fd, (uint8_t*) &feh, 1, 0);
    send(fd, (uint8_t*) &wal, 1, 0);

    struct Response resp;
    recv(fd, &resp, sizeof(resp), 0);
    return resp; 
}

void default_index_handler(int index, char *name)
{
    printf("[%d]: %s\n", index, name);
}

int get_index(int fd, void (*indexhandler)(int index, char *name))
{
    struct Request req;
    req.req_type = GetIndex;
    send(fd, &req, sizeof(req), 0);

    uint16_t no;
    recv(fd, &no, sizeof(uint16_t), 0);

    for (uint16_t i = 0; i < no; i++)
    {
        char filename[256];
        memset(filename, 0, sizeof(filename));

        uint8_t len = 0;

        recv(fd, &len, 1, 0);
        recv(fd, filename, len, 0);
        filename[len] = '\0';

        indexhandler(i, filename);
    }
}

int increment_index(int fd)
{
    struct Request req;
    req.req_type = IncrementIndex;
    send(fd, &req, sizeof(req), 0);
}

int decrement_index(int fd)
{
    struct Request req;
    req.req_type = DecrementIndex;
    send(fd, &req, sizeof(req), 0);
}

bool is_alphanumeric(const char *str)
{
    for (int i = 0; str[i] != '\0'; i++)
    {
        if (!isdigit(str[i]))
            return false;
    }

    return true;
}

int main(int argc, char **argv, char **envp)
{
    char *wallpaper_path = NULL;

    int delay = DEFAULT_DELAY;

    bool feh = false;
    bool wal = false;

    enum Operations operation = -1;
    bool by_index = false;


    for (int i = 1; i < argc; i++)
    {
        if (argv[i][0] != '-')
            continue; 

        if (!strcmp(argv[i], "-s") || !strcmp(argv[i], "--start-slideshow"))
        {
            if (argv[i + 1] != NULL)
            {
                if (!is_alphanumeric(argv[i + 1]))
                {
                    fprintf(stderr, "-s/--start-slideshow received non-int delay!\n");
                    return -1;
                }

                delay = atoi(argv[i + 1]);
            }

            operation = StartSlideShowOp;
        }

        if (!strcmp(argv[i], "-i") || !strcmp(argv[i], "--increment"))
        {
            operation = IncrementOp;
        }

        if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--decrement"))
        {
            operation = DecrementOp;
        }

        if (!strcmp(argv[i], "-g") || !strcmp(argv[i], "--get-index"))
        {
            operation = ShowIndexOp;
        }

        if (!strcmp(argv[i], "-ss") || !strcmp(argv[i], "--stop-slideshow"))
        {
            operation = StopSlideShowOp;
        }

        if (!strcmp(argv[i], "-wa") || !strcmp(argv[i], "--wal"))
        {
            wal = true;
        }

        if (!strcmp(argv[i], "-f") || !strcmp(argv[i], "--feh"))
        {
            feh = true;
        }

        if (!strcmp(argv[i], "-w") || !strcmp(argv[i], "--wallpaper"))
        {
            operation = SetWallpaperOp;

            if (argv[i + 1] == NULL)
            {
                fprintf(stderr, "-w/--wallpaper requires a wallpaper filename!\n");
                return -2;
            }

            if (argv[i + 1][0] == '?')
                by_index = true;
            else
                wallpaper_path = argv[i + 1];
        }
    }


    if (operation == -1)
    {
        fprintf(stderr, "Operation not specified... do -h/--help? Exiting...\n");
        return -100; 
    }

    if (!wal & !feh)
    {
        fprintf(stderr, "Note: feh nor wal explicitly specified; assuming only feh...\n");
        feh = true; 
    }

    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);

    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, UNIX_SOCK_LOCATION, sizeof(addr.sun_path));

    if ((connect(sockfd, (struct sockaddr*) &addr, (socklen_t) sizeof(addr))) != 0)
    {
        fprintf(stderr, "Connection to wallmand socket (%s) failed: %s\n", 
                UNIX_SOCK_LOCATION, strerror(errno));

        return -3;
    }


    switch (operation)
    {
        case IncrementOp:
        {
            increment_index(sockfd);
            break;
        }

        case DecrementOp:
        {
            decrement_index(sockfd);
            break;
        }

        case ShowIndexOp:
        {
            get_index(sockfd, default_index_handler);
            break;
        }

        case SetWallpaperOp:
        {
            struct Response resp = by_index ? 
                set_wallpaper_by_index(sockfd, feh, wal) : 
                set_wallpaper(sockfd, feh, wal, wallpaper_path);

            if (resp.err != 0)
            {
                fprintf(stderr, "Setting the wallpaper failed: %s\n", strerror(resp.err));
                close(sockfd);
                return -4;
            }

            break;
        }

        case StartSlideShowOp:
        {
            int code = start_slideshow(sockfd, feh, wal, delay);

            if (code != Success)
            {
                fprintf(stderr, "%s\n", strerrorwallmand[code]);
                close(sockfd);
                return -5;
            }

            break;
        }

        case StopSlideShowOp:
        {
            int code = stop_slideshow(sockfd);

            if (code != Success)
            {
                fprintf(stderr, "%s\n", strerrorwallmand[code]);
                close(sockfd);
                return -6;
            }

            break;
        }
    }


    close(sockfd);
}