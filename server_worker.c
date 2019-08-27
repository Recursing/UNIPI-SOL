#include <stdlib.h>
#include <stddef.h>
#include <poll.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "utils.h"

static __thread char buffer[1024] = {0};  // Buffer for socket reads
static __thread int buffer_head = 0;      // Current head of buffer
static __thread int store_left = 0;       // Bytes left to store
static __thread int store_fd = -1;        // Fd where to store those bytes
static __thread int socket = -1;          // Socket fd
static __thread char *folder_name = NULL; // Name of client

static void clear_buffer()
{
    memset(buffer, 0, sizeof(buffer));
    buffer_head = 0;
}

static int header_in_buffer()
{
    for (int i = 0; i < 1024; i++)
    {
        switch (buffer[i])
        {
        case '\0':
            return false;
        case '\n':
            return true;
        }
    }
    return false;
}

static void send_ok()
{
    clear_buffer();
    int err = write_all(socket, "OK \n", 5);
    if (err == -1)
    {
        perror("Writing OK to socket");
    }
}

static void send_ko(char *message)
{
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
    int err = mkdir(folder_name, 0740);
    if (err == -1)
    {
        if (errno != EEXIST)
        {
            free(folder_name);
            folder_name = NULL;
            free(name);
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
    err = open(lockfile_name, O_WRONLY | O_CREAT | O_EXCL, 0640);
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
        send_ok();
    }
    free(name);
    free(lockfile_name);
}

static void handle_store()
{
    char *name = get_name("STORE ");
    if (name == NULL)
    {
        return;
    }
    int len_index = strlen("STORE ") + strlen(name) + 1;
    char *lenstr = malloc(MAX_OBJ_LEN_DIGITS + 1);
    int i;
    for (i = 0; i < MAX_OBJ_LEN_DIGITS && buffer[i + len_index] != ' '; i++)
    {
        char c = buffer[i + len_index];
        if (!('0' <= c && c <= '9'))
        {
            send_ko("Object len can only contain digits");
            free(name);
            free(lenstr);
            return;
        }
        lenstr[i] = c;
    }
    lenstr[i] = '\0';
    if (i == 0)
    {
        send_ko("Object len missing, send only one space after name");
        free(name);
        free(lenstr);
        return;
    }
    if (i >= MAX_OBJ_LEN_DIGITS)
    {
        send_ko("File is way too big");
        free(name);
        free(lenstr);
        return;
    }
    long long len = atoll(lenstr);
    int data_start = i + len_index + 3;
    if (buffer[data_start - 2] != '\n' || (buffer_head > data_start - 2 && buffer[data_start - 1] != ' '))
    {
        send_ko("Invalid protocol, len must be followed by newline and space");
        free(name);
        free(lenstr);
        return;
    }

    char *filepath = malloc(strlen(folder_name) + strlen(name) + 2);
    strcpy(filepath, folder_name);
    strcat(filepath, "/");
    strcat(filepath, name);
    int fd = open(filepath, O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd == -1)
    {
        if (errno == EEXIST)
        {
            perror("File with same name already exists");
            send_ko("Another object is stored with the same name");
        }
        else
        {
            perror("Cannot create storage file");
            send_ko("Cannot crate file in storage directory");
        }
        free(lenstr);
        free(name);
        free(filepath);
        return;
    }

    store_left = len;
    store_fd = fd;
    if (buffer_head > data_start && len > 0)
    {
        int to_write = buffer_head - data_start;
        if (to_write > store_left)
            to_write = store_left;
        int err = write_all(fd, buffer + data_start, to_write);
        if (err == -1)
            perror("Writing to storage");
        store_left -= to_write;
        if (store_left < 0)
        {
            fprintf(stderr, "stored too much\n");
            send_ok();
        }
        if (store_left == 0)
            send_ok();
    }
    free(filepath);
    free(lenstr);
    free(name);
    clear_buffer();
}

static void handle_retrieve()
{
    char *name = get_name("RETRIEVE ");
    if (name == NULL)
    {
        return;
    }

    clear_buffer();
}

static void handle_delete()
{
    char *name = get_name("DELETE ");
    if (name == NULL)
    {
        return;
    }
    send_ok();
}

static void handle_leave()
{
    if (!valid_command("LEAVE ", 6))
    {
        return;
    }
    send_ok();
}

static void continue_store()
{
    int to_write = buffer_head;
    if (to_write > store_left)
        to_write = store_left;
    int err = write_all(store_fd, buffer, to_write);
    if (err == -1)
        perror("Writing to storage");
    store_left -= to_write;
    if (store_left < 0)
    {
        fprintf(stderr, "stored too much\n");
        send_ok();
    }
    if (store_left == 0)
    {
        send_ok();
    }
    clear_buffer();
}

/*static void continue_retrieve()
{
}*/

static void handle_socket_message()
{
    int read_len = read(socket, buffer + buffer_head, 1024 - buffer_head);
    if (read_len == -1)
    {
        perror("Reading client socket");
        return;
    }
    if (read_len == 0) // EOF
    {
        return;
    }
    buffer_head += read_len;
    printf("Recieved message! buffer contains %s with len %d\n", buffer, buffer_head);
    if (store_left > 0)
    {
        continue_store();
        return;
    }
    if (!header_in_buffer()) // JUST wait for the rest of the header to be read by the buffer
    {
        return;
    }
    if (buffer[2] != 'G' && folder_name == NULL && buffer[2] != 'L')
    {
        send_ko("You must REGISTER before sending a command");
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
    }
}

// fds[0] is a pipe fd for recieving a termination signal
// fds[1] is the socket fd for handling client communication
static void worker_loop(int *fds)
{
    socket = fds[1];

    int is_done = false;
    int err;
    struct pollfd pfds[2];

    pfds[0].fd = fds[0];
    pfds[0].events = POLLIN;
    pfds[1].fd = fds[1];
    pfds[1].events = POLLIN;

    while (!is_done)
    {
        err = poll(pfds, 2, -1);
        if (err == -1)
        {
            perror("Polling termination pipe and socket");
        }

        if ((pfds[0].revents & POLLIN) ||
            (pfds[0].revents & POLLNVAL) || // POLLNVAL socket has been destroyed TODO
            (pfds[1].revents & POLLHUP))    // POLLHUP --> socket has no writers
        {
            is_done = true;
        }
        else if (pfds[1].revents & POLLIN) // Don't check for messages if it's closing / closed
        {
            handle_socket_message();
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
    int *file_descriptors = (int *)fds;
    worker_loop(file_descriptors);
    cleanup();
    free(fds);
    return NULL;
}