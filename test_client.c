#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "access_library.h"
#include "utils.h"

#define TESTFILE "testo_progetto.txt"
#define MAXSIZE 100000
#define BUFSIZE 100001 // needs to be > MAXSIZE
#define OBJNUM 20

static char *nomi_convenzionali[20] = {
    "Angela", "Angelo", "Anna", "Antonio", "Carmela", "Caterina",
    "Francesco", "Giovanna", "Giovanni", "Giuseppe", "Giuseppina",
    "Lucia", "Luigi", "Maria", "Mario", "Pietro", "Rosa",
    "Salvatore", "Teresa", "Vincenzo"};
static char buffer[BUFSIZE] = {0};

static char *client_name;

// Copy file content to global buffer
static int read_file_to_buffer(int fd, ssize_t fsize)
{
    int err = read(fd, buffer, fsize);
    if (err < 0)
    {
        perror("Cannot read test file");
        return false;
    }
    return true;
}

// Fill buffer with copies of file
static void fill_buffer_with_copies(char *fpath)
{
    int fd = open(fpath, O_RDONLY);
    if (fd < 0)
    {
        perror("Cannot open test file");
        return;
    }
    struct stat s; // Needed to get file size
    int err = fstat(fd, &s);
    if (err < 0)
    {
        perror("Cannot get test file info");
        return;
    }
    ssize_t fsize = s.st_size;
    int res = read_file_to_buffer(fd, fsize);
    if (!res)
        return;
    unsigned long i = 0;
    for (i = fsize; i + fsize < BUFSIZE; i += fsize)
    {
        memcpy(buffer + i, buffer, fsize);
    }
    memcpy(buffer + i, buffer, BUFSIZE - i);
}

// Test storing 20 files with growing size, filling them with the buffer content
// "Il client dovrà generare 20 oggetti, di dimensioni crescenti da 100 byte
//  a 100000 byte, memorizzandoli sull'object store con nomi convenzionali."
// Returns the number of failed stores
static int test_store()
{
    fill_buffer_with_copies(TESTFILE);
    int min_size = 100;
    int max_size = MAXSIZE;
    int errors = 0;
    for (int i = 0; i < OBJNUM; i++)
    {
        char *obj_name = nomi_convenzionali[i];
        // printf("Sending %s\n", obj_name);
        int store_size = min_size + ((max_size - min_size) * i / (OBJNUM - 1));
        int res = os_store(obj_name, buffer, store_size);
        if (!res)
        {
            printf("%s cannot store file %s\n", client_name, obj_name);
            errors += 1;
        }
    }
    return errors;
}

// Retrieves files stored with test_store and checks their size and content
// Returns the number of failed retrieves
static int test_retrieve()
{
    ignore_overwrite_errors = true;
    test_store(); // Store the files if they're not there, ignore errors

    fill_buffer_with_copies(TESTFILE);
    int min_size = 100;
    int max_size = MAXSIZE;

    int errors = 0;
    for (int i = 0; i < OBJNUM; i++)
    {
        char *obj_name = nomi_convenzionali[i];

        // printf("Retrieving %s\n", obj_name);
        char *retrieved_obj = os_retrieve(obj_name);
        int obj_size = get_retrieve_len();
        int known_size = min_size + ((max_size - min_size) * i / (OBJNUM - 1));
        if (known_size != obj_size)
        {
            printf("%s retrieved file %s with wrong size\n", client_name, obj_name);
            errors += 1;
            continue;
        }
        if ((retrieved_obj == NULL && obj_size != 0) || obj_size < 0)
        {
            printf("%s cannot retrieve file %s\n", client_name, obj_name);
            errors += 1;
            continue;
        }
        if (memcmp(buffer, retrieved_obj, obj_size) != 0)
        {
            errors += 1;
            printf("%s Retrieved %s that does not match\n", client_name, obj_name);
        }
        free(retrieved_obj);
    }
    return errors;
}

// Deletes files stored in test_store and checks that they've been deleted
// Returns the number of failed deletes
static int test_delete()
{
    fill_buffer_with_copies(TESTFILE);
    ignore_overwrite_errors = true;
    test_store(); // Store the files if they're not there
    int errors = 0;
    for (int i = 0; i < OBJNUM; i++)
    {
        char *obj_name = nomi_convenzionali[i];
        // printf("%s Deleting %s\n", client_name, obj_name);
        int res = os_delete(obj_name);
        if (!res)
        {
            printf("%s Failed to delete %s\n", client_name, obj_name);
            errors += 1;
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
            errors += 1;
        }

        free(filepath);
    }
    return errors;
}

// Return 1 if disconnect returned an error, 0 otherwise
static int test_leave()
{
    int res = os_disconnect();
    if (!res)
    {
        fprintf(stderr, "%s disconnection failed\n", client_name);
        return 1;
    }
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Client must be called with two arguments: name and test number\n");
        return 1;
    }

    client_name = argv[1];
    // printf("starting client %s\n", client_name);
    if (!os_connect(client_name))
    {
        fprintf(stderr, "Connection failed for name %s\n", argv[1]);
        printf("Run test %c with 1 errors in 0 operations\n", argv[2][0]);
        return 1;
    }

    int errors = 0;
    switch (argv[2][0])
    {
    case '1':
        errors = test_store();
        break;
    case '2':
        errors = test_retrieve();
        break;
    case '3':
        errors = test_delete();
        break;
    default:
        fprintf(stderr, "Invalid test number :( send 1, 2 or 3\n");
        test_leave();
        return 1;
    }
    errors += test_leave();
    printf("Run test %c with %d errors in %d operations\n", argv[2][0], errors, OBJNUM);
    if (errors > 0)
        return 1;
    return 0;
}