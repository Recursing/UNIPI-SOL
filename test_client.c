#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include "access_library.h"
#include "utils.h"

#define BUFSIZE 100000
#define OBJNUM 20

static char *nomi_convenzionali[20] = {
    "Angela", "Angelo", "Anna", "Antonio", "Carmela", "Caterina",
    "Francesco", "Giovanna", "Giovanni", "Giuseppe", "Giuseppina",
    "Lucia", "Luigi", "Maria", "Mario", "Pietro", "Rosa",
    "Salvatore", "Teresa", "Vincenzo"};
static char buffer[BUFSIZE] = {0};

static char *client_name;

static int read_file_to_buffer(char *filepath, ssize_t fsize)
{
    int fd = open(filepath, O_RDONLY);

    if (fd < 0)
    {
        perror("Cannot open test file");
        return 0;
    }
    int err = read(fd, buffer, fsize);
    if (err < 0)
    {
        perror("Cannot read test file");
        return 0;
    }
}

static void fill_buffer_with_copies(char *fpath, ssize_t fsize)
{
    read_file_to_buffer(fpath, fsize);
    int i = 0;
    for (i = fsize; i + fsize < BUFSIZE; i += fsize)
    {
        memcpy(buffer + i, buffer, fsize);
    }
    memcpy(buffer + i, buffer, BUFSIZE - i);
}

static void test_store()
{
    int fsize = 9909;
    fill_buffer_with_copies("testo_progetto.txt", fsize);
    int min_size = 100;
    int max_size = 100000;
    for (int i = 0; i < OBJNUM; i++)
    {
        char *obj_name = nomi_convenzionali[i];
        printf("Sending %s\n", obj_name);
        int store_size = min_size + ((max_size - min_size) * i / (OBJNUM - 1));
        int res = os_store(obj_name, buffer, store_size);
        if (!res)
        {
            printf("%s cannot store file %s\n", client_name, obj_name);
        }
    }
}

static void test_retrieve()
{
    // Since os_retrive doesn't return the object size
    // I must already know the retrieved object len use it...
    // TODO add `int last_retrieve_len()` to library

    test_store(); // Store the files if they're not there

    int fsize = 9909;
    fill_buffer_with_copies("testo_progetto.txt", fsize);
    int min_size = 100;
    int max_size = 100000;
    for (int i = 0; i < OBJNUM; i++)
    {
        char *obj_name = nomi_convenzionali[i];

        printf("Retrieving %s\n", obj_name);
        char *retrieved_obj = os_retrieve(obj_name);
        if (retrieved_obj == NULL)
        {
            printf("%s cannot retrieve file %s\n", client_name, obj_name);
            continue;
        }

        int obj_size = min_size + ((max_size - min_size) * i / (OBJNUM - 1));
        if (memcmp(buffer, retrieved_obj, obj_size) != 0)
        {
            printf("%s Retrieved %s that does not match\n", client_name, obj_name);
        }
        free(retrieved_obj);
    }
}

static void test_delete()
{
    int fsize = 9909;
    fill_buffer_with_copies("testo_progetto.txt", fsize);
    test_store(); // Store the files if they're not there

    for (int i = 0; i < OBJNUM; i++)
    {
        char *obj_name = nomi_convenzionali[i];
        printf("%s Deleting %s\n", client_name, obj_name);
        int res = os_delete(obj_name);
        if (!res)
        {
            printf("%s Failed to delete %s\n", client_name, obj_name);
            continue;
        }

        char *filepath = malloc(strlen(STORAGEPATH) + strlen(client_name) + strlen(obj_name) + 3);
        strcpy(filepath, STORAGEPATH);
        strcat(filepath, "/");
        strcat(filepath, client_name);
        strcat(filepath, "/");
        strcat(filepath, obj_name);
        if (access(filepath, F_OK) != -1)
        {

            fprintf(stderr, "%s deleted %s, but it's still there\n", client_name, obj_name);
        }

        free(filepath);
    }
}

static void test_leave()
{
    int res = os_disconnect();
    if (!res)
    {
        fprintf(stderr, "%s disconnection failed\n", client_name);
    }
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Client must be called with two arguments: name and test number\n");
        return 1;
    }
    client_name = argv[1];
    printf("starting client %s\n", client_name);
    if (!os_connect(client_name))
    {
        fprintf(stderr, "Connection failed with name %s\n", argv[1]);
        return 1;
    }

    switch (argv[2][0])
    {
    case '1':
        test_store();
        break;
    case '2':
        test_retrieve();
        break;
    case '3':
        test_delete();
        break;
    default:
        fprintf(stderr, "Invalid test number :( send 1, 2 or 3\n");
        test_leave();
        return 1;
    }
    test_leave();
    return 0;
}