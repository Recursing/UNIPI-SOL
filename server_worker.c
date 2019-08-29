#include <stdlib.h>
#include <stddef.h>
#include <poll.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include "utils.h"
#include "connection_handler.h"

#define BUFFERSIZE 1024
#define MAX_KOs 10000 // MAX number of errors per client

static __thread int is_done = false;
static __thread char buffer[BUFFERSIZE + 1] = {0}; // Buffer for socket reads
static __thread int buffer_head = 0;               // Current head of buffer
static __thread int store_left = 0;                // Bytes left to store
static __thread int store_fd = -1;                 // Fd where to store those bytes
static __thread char *store_filepath = NULL;       // To unlink in case of termination while storing
static __thread int socket = -1;                   // Socket fd
static __thread char *folder_name = NULL;          // Name of client

static void clear_buffer()
{
    memset(buffer, 0, sizeof(buffer));
    buffer_head = 0;
}

static int header_in_buffer()
{
    for (int i = 4; i < buffer_head; i++)
    {
        switch (buffer[i])
        {
        case '\0':
            return false;
        case '\n':
            if (buffer[i - 1] == ' ')
                return true;
            break;
        }
    }
    return false;
}

static void send_ok()
{
    printf("Sending ok\n");
    clear_buffer();
    int err = write_all(socket, "OK \n", 5);
    if (err == -1)
    {
        perror("Writing OK to socket");
    }
}

static void send_ko(char *message)
{
    printf("Sending ko %s \n", message);
    clear_buffer();
    int msg_len = strlen(message) + strlen(" KO \n") + 1;
    char *full_message = malloc(msg_len);
    strcpy(full_message, "KO ");
    strcat(full_message, message);
    strcat(full_message, " \n");
    int err = write_all(socket, full_message, msg_len);
    if (err == -1)
    {
        perror("Writing KO to socket");
    }
    free(full_message);
}

static int valid_command(char *wanted_command, int command_len)
{
    if (strncmp(buffer, wanted_command, command_len) != 0)
    {
        printf("Invalid command in buffer %s", buffer);
        send_ko("Invalid command in header");
        return false;
    }
    return true;
}

static char *get_name(char *wanted_command)
{
    int command_len = strlen(wanted_command);
    if (!valid_command(wanted_command, command_len))
    {
        return NULL;
    }
    char *name = malloc(MAX_FILENAME_LEN);
    int i;
    for (i = 0; i < MAX_FILENAME_LEN && (buffer[command_len + i] != ' '); i++)
    {
        char c = buffer[command_len + i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-'))
        {
            send_ko("Name contains non POSIX filename characters");
            free(name);
            return NULL;
        }
        name[i] = c;
    }
    if (i == 0)
    {
        send_ko("Missing name, send only one space after command");
        free(name);
        return NULL;
    }
    if (i >= MAX_FILENAME_LEN)
    {
        send_ko("Name is too long");
        free(name);
        return NULL;
    }
    name[i] = '\0';
    if ((strcmp(name, ".") == 0) || (strcmp(name, "..") == 0) || (strcmp(name, ".lock") == 0))
    {
        send_ko("., .. and .lock are reserved names");
        free(name);
        return NULL;
    }
    return name;
}

static void handle_register()
{
    char *name = get_name("REGISTER ");
    if (name == NULL)
    {
        return;
    }

    printf("Registering %s\n", name);
    folder_name = malloc(strlen(name) + strlen(STORAGEPATH) + 1);
    strcpy(folder_name, STORAGEPATH);
    strcat(folder_name, name);
    free(name);
    int err = mkdir(folder_name, 0740);
    if (err == -1)
    {
        if (errno != EEXIST)
        {
            free(folder_name);
            folder_name = NULL;
            perror("Creating directory");
            send_ko("Error creating directory");
            return;
        }
    }

    char *lockfile_name = malloc(strlen(folder_name) + strlen("/.lock") + 1);
    strcpy(lockfile_name, folder_name);
    strcat(lockfile_name, "/.lock");

    // If O_CREAT and O_EXCL are set, open() shall fail if the file exists.
    // The check for the existence of the file and the creation of the file if it does not exist
    // shall be atomic with respect to other threads executing open() naming the same filename
    // in the same directory with O_EXCL and O_CREAT set.
    err = open(lockfile_name, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (err == -1)
    {
        if (errno == EEXIST)
        {
            perror("Lock file already exists");
            send_ko("Another client is connected with the same name");
        }
        else
        {
            perror("Cannot create lock file");
            send_ko("Cannot write in storage directory");
        }
        free(folder_name);
        folder_name = NULL;
    }
    else
    {
        close(err);
        send_ok();
    }
    free(lockfile_name);
}

static void close_store_file()
{
    if (store_fd > 0)
    {
        close(store_fd);
        store_fd = -1;
    }
    if (store_filepath != NULL)
    {
        free(store_filepath);
        store_filepath = NULL;
    }
}

static void handle_store()
{
    char *name = get_name("STORE ");
    if (name == NULL)
    {
        return;
    }
    int len_index = strlen("STORE ") + strlen(name) + 1;
    char lenstr[MAX_OBJ_LEN_DIGITS + 1];
    int i;
    for (i = 0; i < MAX_OBJ_LEN_DIGITS && buffer[i + len_index] != ' '; i++)
    {
        char c = buffer[i + len_index];
        if (!('0' <= c && c <= '9'))
        {
            send_ko("Object len can only contain digits");
            free(name);
            return;
        }
        lenstr[i] = c;
    }
    lenstr[i] = '\0';
    if (i == 0)
    {
        send_ko("Object len missing, send only one space after name");
        free(name);
        return;
    }
    if (i >= MAX_OBJ_LEN_DIGITS)
    {
        send_ko("File is way too big");
        free(name);
        return;
    }
    int len = atoi(lenstr);
    int data_start = i + len_index + 3;
    if (buffer[data_start - 2] != '\n' || (buffer_head > data_start - 2 && buffer[data_start - 1] != ' '))
    {
        send_ko("Invalid protocol, len must be followed by newline and space");
        free(name);
        return;
    }

    char *filepath = malloc(strlen(folder_name) + strlen(name) + 2);
    strcpy(filepath, folder_name);
    strcat(filepath, "/");
    strcat(filepath, name);
    free(name);
    int fd = open(filepath, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd == -1)
    {
        store_left = len - (buffer_head - data_start); // Need to read bytes anyway, to clear socket
        if (store_left < 0)
            store_left = 0;
        if (errno == EEXIST) // TODO should I allow overwrites?
        {
            perror("File with same name already exists");
            if (store_left == 0)
            {
                send_ko("Another object is stored with the same name");
            }
            store_fd = -2;
        }
        else
        {
            printf("OH NOES!\n");
            perror("Cannot create storage file");
            if (store_left == 0)
            {
                send_ko("Cannot create file in storage directory");
            }
            store_fd = -1;
        }
        clear_buffer();
        free(filepath);
        return;
    }

    store_left = len;
    if (len == 0)
    {
        send_ok();
        close(fd);
    }
    else
    {
        store_fd = fd;
        store_filepath = malloc(strlen(filepath) + 1);
        strcpy(store_filepath, filepath);
    }
    if (buffer_head > data_start && len > 0)
    {
        int to_write = buffer_head - data_start;
        if (to_write > store_left)
            to_write = store_left;
        int err = write_all(fd, buffer + data_start, to_write);
        if (err == -1)
        {
            perror("Writing to storage");
            close_store_file();
            fprintf(stderr, "writing start to storage %s %s\n", folder_name, filepath);
            send_ko("Error writing file to disk");
        }
        store_left -= to_write;
        if (store_left < 0)
        {
            fprintf(stderr, "stored too much\n");
        }
        if (store_left <= 0)
        {
            close_store_file();
            send_ok();
        }
    }
    free(filepath);
    clear_buffer();
}

static void send_retrieve(char *data, ssize_t len)
{
    printf("Sending back %lu bytes \n", len);
    char len_string[MAX_OBJ_LEN_DIGITS];
    sprintf(len_string, "%lu", len);
    clear_buffer();
    int header_len = strlen("DATA  \n ") + strlen(len_string);
    char *header_message = malloc(header_len + 1);
    strcpy(header_message, "DATA ");
    strcat(header_message, len_string);
    strcat(header_message, " \n ");
    int err = write_all(socket, header_message, header_len);
    if (err == -1)
    {
        perror("Writing retrieve header to socket");
        free(header_message);
        send_ko("Error sending header back");
    }
    free(header_message);
    err = write_all(socket, data, len);
    if (err == -1)
    {
        perror("Writing retrieved data to socket");
        send_ko("Error sending data back");
    }
    printf("I think I sent %lu bytes back\n", len);
}

static void handle_retrieve()
{
    char *name = get_name("RETRIEVE ");
    if (name == NULL)
    {
        return;
    }
    char *filepath = malloc(strlen(folder_name) + strlen(name) + 2);
    strcpy(filepath, folder_name);
    strcat(filepath, "/");
    strcat(filepath, name);
    free(name);

    int fd = open(filepath, O_RDONLY);
    if (fd < 0)
    {
        perror("Deleting file");
        if (errno == ENOENT)
        {
            send_ko("Object does not exist");
        }
        else
        {
            send_ko("Error retrieving object");
        }
        free(filepath);
        return;
    }

    struct stat s; // Needed to get file size
    int err = fstat(fd, &s);
    if (err == -1)
    {
        perror("Getting file infos");
        send_ko("Error getting object size");
        free(filepath);
        close(fd);
        return;
    }
    // TODO handle empty files
    char *temp_buffer = malloc(s.st_size); // TODO don't store eveything in memory
    err = read(fd, temp_buffer, s.st_size);
    if (err < 0)
    {
        perror("Copying file to memory");
        free(filepath);
        free(temp_buffer);
        close(fd);
        send_ko("Error reading object to memory");
        return;
    }
    send_retrieve(temp_buffer, s.st_size);
    free(temp_buffer);
    free(filepath);
    close(fd);
    clear_buffer();
}

static void handle_delete()
{
    printf("Handling DeLete\n");
    char *name = get_name("DELETE ");
    if (name == NULL)
    {
        return;
    }
    char *filepath = malloc(strlen(folder_name) + strlen(name) + 2);
    strcpy(filepath, folder_name);
    strcat(filepath, "/");
    strcat(filepath, name);
    free(name);
    int err = unlink(filepath);

    if (err == -1)
    {
        perror("Deleting file");
        free(filepath);
        send_ko("Error deleting file");
        return;
    }
    free(filepath);
    send_ok();
}

static void handle_leave()
{
    if (!valid_command("LEAVE ", 6))
    {
        return;
    }
    send_ok();
    is_done = true;
}

static void continue_store()
{
    // printf("Storing %d bytes...\n", buffer_head);
    int to_write = buffer_head;
    if (to_write > store_left)
        to_write = store_left;
    if (store_fd < 0) // Invalid store, ignoring bytes, just freeing socket
    {
        store_left -= to_write;
        if (store_left <= 0)
        {
            fprintf(stderr, "Dumped invalid store...\n");
            if (store_fd == -1)
                send_ko("Cannot create file in storage directory");
            else if (store_fd == -2)
                send_ko("Another object is stored with the same name");
        }
        clear_buffer();
        return;
    }
    int err = write_all(store_fd, buffer, to_write);
    if (err == -1)
    {
        fprintf(stderr, "Err writing to %s %s\n", folder_name, store_filepath);
        close_store_file();
        perror("Continuing writing file to disk");
        send_ko("Error storing file");
    }
    store_left -= to_write;
    if (store_left < 0)
    {
        fprintf(stderr, "stored too much\n");
    }
    if (store_left <= 0)
    {
        close_store_file();
        send_ok();
    }
    clear_buffer();
}

/*static void continue_retrieve()
{
}*/

static void handle_socket_message()
{
    printf("Buffer head at: %d\n", buffer_head);
    int read_len = read(socket, buffer + buffer_head, BUFFERSIZE - buffer_head);
    // printf("||%d %d||%s", buffer_head, read_len, buffer);

    if (read_len == -1)
    {
        perror("Reading client socket");
        return;
    }
    if (read_len == 0) // EOF
    {
        if (folder_name != NULL)
            printf("Recieved socket EOF for %s\n", folder_name);
        printf("EOF, Buffer head at %d\n", buffer_head);
        is_done = true;
        return;
    }
    buffer_head += read_len;
    if (store_left > 0) // Dump everything that can be read to filesystem
    {
        continue_store();
        return;
    }
    if (!header_in_buffer()) // Just wait for the rest of the header to be read by the buffer
    {
        if (buffer_head == BUFFERSIZE) // buffer full with no header and not storing, garbage data
            // send_ko("Invalid message"); // can choke socket if client does not read
            clear_buffer();
        return;
    }
    if (buffer[2] != 'G' && folder_name == NULL && buffer[2] != 'L')
    {
        // printf("Buffer contains %s with len %d\n", buffer, buffer_head);
        send_ko("You must REGISTER before sending a command"); // can choke socket if client does not read
        return;
    }
    switch (buffer[2])
    {
    case '\0':
        break;
    case 'G': // reGister
        handle_register();
        break;
    case 'O': // stOre
        handle_store();
        break;
    case 'T': // reTrieve
        handle_retrieve();
        break;
    case 'L': // deLete
        handle_delete();
        break;
    case 'A': // leAve
        handle_leave();
        break;
    default: // rubbish
        send_ko("Invalid header");
        break;
    }
}

// fds[0] is a pipe fd for recieving a termination signal
// fds[1] is the socket fd for handling client communication
static void worker_loop(int *fds)
{
    socket = fds[1];

    is_done = false;
    int err;
    struct pollfd pfds[2];

    pfds[0].fd = fds[0];
    pfds[0].events = POLLIN;
    pfds[1].fd = fds[1];
    pfds[1].events = POLLIN;

    while (!is_done)
    {
        printf(".");
        err = poll(pfds, 2, -1);
        if (err == -1)
        {
            perror("Polling termination pipe and socket");
        }

        if (pfds[0].revents & POLLIN) // Recieved termination signal
        {
            printf("Recieved TERM\n");
            is_done = true; // Don't read so everybody can recieve it
        }
        else if (pfds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) // Errors with the termination signal
        {
            fprintf(stderr, "Error %d polling signal pipe from worker", pfds[0].revents);
            is_done = true;
        }
        else if (pfds[1].revents & POLLIN) // Don't check for messages if it's closing / closed
        {
            handle_socket_message();
        }
        else if (pfds[1].revents & POLLHUP) // socket has no writers and there's nothing more to read
        {
            printf("Worker terminating for socket POLLHUP\n");
            is_done = true;
        }
        else
        {
            printf("Unexpected events %d %d\n", pfds[0].revents, pfds[1].revents);
            is_done = true;
        }
    }
}

void cleanup()
{
    if (folder_name == NULL)
    {
        return;
    }
    if (store_left > 0 && store_fd > 0 && store_filepath != NULL)
    {
        printf("Deleting partially stored file :( %s\n", store_filepath);
        unlink(store_filepath);
    }
    close_store_file();
    char *lockfile_name = malloc(strlen(folder_name) + strlen("/.lock") + 1);
    strcpy(lockfile_name, folder_name);
    strcat(lockfile_name, "/.lock");
    int err = unlink(lockfile_name);
    if (err == -1)
    {
        perror("Unlinking lock file");
    }
    free(lockfile_name);
    free(folder_name);
}

void *start_worker(void *fds)
{
    pthread_mutex_lock(&active_workers_lock);
    active_workers++;
    pthread_mutex_unlock(&active_workers_lock);
    printf("Starting worker\n");
    int *file_descriptors = (int *)fds;
    worker_loop(file_descriptors);
    if (folder_name != NULL)
        printf("Ending worker %s\n", folder_name);
    else
        printf("Ending unregistered worker\n");

    cleanup();
    free(fds);
    pthread_mutex_lock(&active_workers_lock);
    active_workers--;
    if (active_workers <= 0)
    {
        pthread_cond_signal(&active_workers_CV);
    }
    pthread_mutex_unlock(&active_workers_lock);
    return NULL;
}