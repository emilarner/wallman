#include "wallmand.h"

void list_files(struct Wallman *wallman, int fd)
{
    /* The client needs to know the amount of filenames incoming. 2 bytes for this one */
    send(fd, (uint16_t*) &wallman->files->length, 2, 0);


    /* Then, for every file, send its length and then the non-null terminated name for it. */
    for (size_t i = 0; i < wallman->files->length; i++)
    {
        uint8_t length = strlen(wallman->files->strings[i]);
        send(fd, &length, 1, 0);
        send(fd, wallman->files->strings[i], length, 0);
    }
}

void refresh_files(struct Wallman *wallman)
{
    /* If a file list exists, free it. */
    if (wallman->files != NULL)
        sarray_free(wallman->files);
    

    /* Make a new list. */
    wallman->files = sarray_init();

    DIR *dir = opendir(wallman->wallpapers_path);
    
    /* For every file in the pictures directory, add it to the file list. */
    for (struct dirent *entry = readdir(dir); entry != NULL; entry = readdir(dir))
    {
        /* Ignore special files directories, only looking for regular files. */
        if (entry->d_type != DT_REG)
            continue;

        /* This can probably be removed because of the clause above, but still... */
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            continue; 

        sarray_add(wallman->files, entry->d_name);
    }

    closedir(dir);
}

void *slideshow_thread(void *info)
{
    struct Slideshow *s = (struct Slideshow*) info;
    struct Wallman *wallman = s->wallman;

    /* Do this forever until the StopSlideshow call is received. */
    while (true)
    {
        /* Go through each picture in the file list and set it as background */
        /* Then, sleep for the provided delay */
        for (int i = 0; i < s->wallman->files->length; i++)
        {
            /* The StopSlideshow request will set this flag false, ending the loop. */
            if (!s->wallman->slideshow)
                goto cleanup;

            if (s->feh)
                set_background_feh(wallman, sarray_get(s->wallman->files, i));
            
            if (s->wal)
                set_background_wal(wallman, sarray_get(s->wallman->files, i));

            sleep(s->delay);
        }
    }

cleanup:
    free(info);    
}

int file_complications(struct Wallman *wallman, char *path)
{
    char actual[1024];
    snprintf(actual, sizeof(actual), "%s/%s", wallman->wallpapers_path, path);
    
    /* Check if there are file complications by checking if fopen() causes errno */
    FILE *fp = fopen(actual, "rb");

    if (!fp)
        return errno;

    fclose(fp);
    return 0;
}

void set_background_feh(struct Wallman *wallman, char *path)
{
    char command[1024];
    
    snprintf(
        command, 
        sizeof(command), 
        "(feh --bg-scale %s/%s) &> /dev/null", 
        wallman->wallpapers_path,
        path
    );

    system(command);
}

void set_background_wal(struct Wallman *wallman, char *path)
{
    char command[1024];
    
    snprintf(
        command, 
        sizeof(command), 
        "(wal -e -q -n -i %s/%s) &> /dev/null", 
        wallman->wallpapers_path,
        path
    );

    system(command);
}

int set_background(struct Wallman *wallman, bool feh, bool wal, char *path)
{
    int complications = 0;

    if ((complications = file_complications(wallman, path)) != 0)
        return complications;

    if (feh)
        set_background_feh(wallman, path);

    if (wal)
        set_background_wal(wallman, path);

    return 0;
}

void *client_handler(void *client)
{
    struct Client *c = (struct Client*) client;

    int fd = c->fd;
    struct Wallman *wallman = c->wallman;

    struct Request req;
    recv(fd, &req, sizeof(req), 0);

    switch (req.req_type)
    {
        /* List all the files and send it to the client's file descriptor. */
        case GetIndex:
        {
            list_files(wallman, fd);

            struct Response resp;
            resp.resp_type = SetWallpaperResp;
            resp.err = 0;
            resp.code = Success;

            send(fd, &resp, sizeof(resp), 0); 
            break;
        }

        /* Set the wallpaper. */
        case SetWallpaperByIndex:
        case SetWallpaper:
        {
            uint8_t feh = false;
            uint8_t wal = false;

            recv(fd, &feh, 1, 0);
            recv(fd, &wal, 1, 0);

            char *filename = NULL;
            char wallpaper_filename[256];

            /* Normal wallpaper setting is done by receiving the desired filename. */
            /* Indexed wallpaper setting uses an internal index to go through each background. */
            if (req.req_type == SetWallpaper)
            {
                /* Modulus the filename length so that it does not go over 256. */
                /* Also explicitly add a null terminator at the end. */
                /* It's better to be safe than sorry, but there is a guarantee for a NUL... */ 
                recv(fd, wallpaper_filename, (req.data_len % sizeof(wallpaper_filename)) + 1, 0);
                wallpaper_filename[req.data_len + 1] = '\0';
                filename = wallpaper_filename;
            }
            else
            {
                /* If setting wallpaper by index, get the filename from the list of names */
                filename = wallman->files->strings[wallman->index];
            }
            
            int code = set_background(wallman, feh, wal, filename);

            struct Response resp;
            resp.resp_type = SetWallpaperResp;
            resp.err = code;

            send(fd, &resp, sizeof(resp), 0);
            break;
        }

        /* Begin a slideshow. */
        case StartSlideshow:
        {
            struct Response resp;
            resp.resp_type = StartSlideshowResp;
            resp.err = 0;

            struct Slideshow *s = (struct Slideshow*) malloc(sizeof(*s));

            s->wallman = wallman;

            /* Receive parameters for the slideshow: delay (4 bytes), feh (1 byte), wal (1 byte) */
            recv(fd, &s->delay, 4, 0);
            recv(fd, &s->feh, 1, 0);
            recv(fd, &s->wal, 1, 0);

            /* If the delay is invalid, return an error. */
            if (s->delay == 0)
            {
                resp.code = DelayError;
                send(fd, &resp, sizeof(resp), 0);
                break;
            }

            /* If not proper boolean, return an error. */
            if (s->feh != 0 && s->feh != 1)
            {
                resp.code = Boolean; 
                send(fd, &resp, sizeof(resp), 0);
                break;
            }

            /* If not proper boolean, return an error. */
            if (s->wal != 0 && s->wal != 1)
            {
                resp.code = Boolean; 
                send(fd, &resp, sizeof(resp), 0);
                break;
            }

            /* If there is a slideshow already active, return an error */
            if (wallman->slideshow)
            {
                resp.code = SlideshowAlready;

                send(fd, &resp, sizeof(resp), 0);
                break;
            }
            

            /* This will control the slideshow thread, as well as prevent certain other commands. */
            wallman->slideshow = true;

            /* Create the slideshow thread which runs independently. */
            pthread_t slideshowid;
            pthread_create(&slideshowid, NULL, slideshow_thread, (void*) s);

            resp.code = Success;
            send(fd, &resp, sizeof(resp), 0);

            break;
        }

        /* Stop the current slideshow--set wallman->slideshow = false. */
        case StopSlideshow:
        {
            struct Response resp;
            resp.resp_type = StopSlideshowResp;

            /* There's no slideshow to stop! */
            if (!wallman->slideshow)
            {
                resp.code = SlideshowNoent;
                send(fd, &resp, sizeof(resp), 0);
                break;
            }

            /* Tell the slideshow thread to exit its loop. */ 
            wallman->slideshow = false;

            resp.code = Success;
            send(fd, &resp, sizeof(resp), 0);

            break;
        }
        
        /* Refresh the internal index of files. Possibly the user has added wallpapers? */
        case Refresh:
        {
            /* Only refresh if a slideshow ISN'T active. */
            /* This can cause a race condition if we don't! */ 
            if (!wallman->slideshow)
                refresh_files(wallman);

            struct Response resp;

            resp.resp_type = Refresh;
            resp.err = 0;
            resp.code = wallman->slideshow ? RefreshSlideShowActive : Success;

            send(fd, &resp, sizeof(resp), 0);
            break; 
        }

        /* Increment the internal index, preventing overflow. */
        case IncrementIndex:
        {
            wallman->index = (wallman->index + 1) % wallman->files->length; 
            break;
        }

        /* Decrement the internal index, preventing underflow. */
        case DecrementIndex:
        {
            wallman->index = (wallman->index - 1) % wallman->files->length;
            break;
        }

        default:
        {
            break;
        }
    }

    close(fd);
    free(c);
}

int main(int argc, char **argv, char **envp)
{
    char *socket_location = UNIX_SOCK_LOCATION;

    struct Wallman wallman;
    memset(&wallman, 0, sizeof(wallman));

    /* Custom arg parsing... */
    for (int i = 1; i < argc; i++)
    {
        /* Get path for wallpapers. Complain if there are none. */
        if (!strcmp(argv[i], "-p") || !strcmp(argv[i], "--path"))
        {
            if (argv[i + 1] == NULL)
            {
                fprintf(stderr, "-p/--path requires a path for images!\n");
                return -1;
            }

            /* We could have used a pointer to the argv index, but whatever! */
            strncpy(wallman.wallpapers_path, argv[i + 1], sizeof(wallman.wallpapers_path));
        }

        if (!strcmp(argv[i], "-s") || !strcmp(argv[i], "--sock"))
        {
            if (argv[i + 1] == NULL)
            {
                fprintf(stderr, "-s/--sock requires a path for the UNIX Domain Socket!\n");
                return -1;
            }

            socket_location = argv[i + 1];
        }

        //if (!strcmp(argv[i], "--"))
    }

    /* If no bytes have been set, we have no default path! Error! */
    if (wallman.wallpapers_path[0] == '\0')
    {
        fprintf(stderr, "Wallmand requires a path for images... exiting with error!\n");
        return -2;
    }

    /* We need an internal index of files. */
    refresh_files(&wallman);


    /* Set up the UNIX Domain Socket that we use for communication. */
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    
    struct sockaddr_un unixaddr;
    unixaddr.sun_family = AF_UNIX;
    strncpy(unixaddr.sun_path, socket_location, sizeof(unixaddr.sun_path));

    /* If there is an existing UDS, delete it. No exceptions in C, so who cares if it fails? */
    unlink(socket_location);

    /* Bind to /tmp/wallmand.sock, checking errno if it failed. */ 
    if (bind(sockfd, (struct sockaddr*) &unixaddr, (socklen_t) sizeof(unixaddr)) != 0)
    {
        fprintf(stderr, "Error binding UNIX socket: %s\n", strerror(errno));
        return -1;
    }

    listen(sockfd, 64);

    while (true)
    {
        /* A structure to store each client. */
        /* For UNIX sockets, this is not too useful. */  
        struct sockaddr_un client;
        socklen_t client_len = sizeof(client);
        /* ~~~~~~~~~~~~~~~~~~~~^ if you forget this, you will get many accept() errors. */

        int fd = 0;

        /* Accept the client; check if accept() failed; finally, check errno. */ 
        if ((fd = accept(sockfd, (struct sockaddr*) &client, &client_len)) < 0)
        {
            fprintf(stderr, "Error accepting client: %s\nContinuing...\n", strerror(errno));
            continue;
        }

        /* We must allocate on the heap, as we are entering a new thread. */
        /* Additionally, there could *theoretically* be many clients. But, this is overkill. */
        struct Client *uclient = (struct Client*) malloc(sizeof(*uclient));
        uclient->fd = fd;
        uclient->wallman = &wallman;

        /* Create the thread--with pthreads! */
        pthread_t tmp;
        pthread_create(&tmp, NULL, &client_handler, (void*) uclient);
    }


    return 0;
}