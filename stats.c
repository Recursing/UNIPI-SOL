#include <sys/stat.h>
#include <ftw.h>
#include <stdio.h>
#include <string.h>
#include "connection_handler.h"
#include "utils.h"

static __thread int stored_bytes = 0;
static __thread int stored_files = 0;
static __thread int stored_folders = 0;

// Called on every file and folder in storage
static int handle_file(const char *fpath, const struct stat *sb, int typeflag)
{
    // Don't count lockfiles
    int lock_file_len = strlen(LOCKFILENAME);
    int fpath_len = strlen(fpath);
    int is_lockfile = (fpath_len > lock_file_len);
    for (int i = 1; i <= lock_file_len && is_lockfile; i++)
    {
        is_lockfile = (fpath[fpath_len - i] == LOCKFILENAME[lock_file_len - i]);
    }
    if (is_lockfile)
        return 0;

    if (typeflag == FTW_F)
    { // Count file number and total file size
        stored_files++;
        stored_bytes += sb->st_size;
    }
    else if (typeflag == FTW_D)
    { // Count folder number
        stored_folders++;
    }
    return 0;
}

// Run ftw on storage calling handle_file
static void compute_stats()
{
    ftw(STORAGEPATH, &handle_file, 4);
}

// Thread entry point, compute and print stats
void *print_stats(void *arg)
{
    stored_bytes = 0;
    stored_files = 0;
    stored_folders = 0;
    compute_stats();
    stored_folders--; // Ignore storage directory itself

    // Could count lockfiles for active users, but I already have active_workers
    // And need to lock stdout in case stats is called concurrenty in different threads
    pthread_mutex_lock(&active_workers_lock);

    printf("Object storage statistics:\n");
    printf(" - %d active clients\n", active_workers);
    printf(" - %d stored objects\n", stored_files);
    printf(" - %d stored bytes\n", stored_bytes);
    printf(" - %d total registered users\n", stored_folders);

    pthread_mutex_unlock(&active_workers_lock);
    return NULL;
}