/* Copyright (C) 2025 OrionHEN / LightningMods

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 3, or (at your option) any
later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; see the file COPYING. If not, see
<http://www.gnu.org/licenses/>.  */

#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/sysctl.h>

#include <pthread.h>

#include <stdbool.h>
#include <errno.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/stat.h>
#include "ipc.hpp"
#include <string>

// File helpers used by IPC copy/delete (SELF directory decrypt removed)
long totalSize = 0;
long copiedSoFar = 0;
clock_t lastTime = clock();
long lastBytes = 0;

std::string dump_message;
pthread_t notifyThread;

void OrionHEN_log(const char *fmt, ...);
bool if_exists(const char *path);
void notify(bool show_watermark, const char *text, ...);

bool rmtree(const char *path)
{
    DIR *dir = opendir(path);
    if (dir == NULL)
    {
        OrionHEN_log("Error opening directory %s", path);
        return false;
    }

    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL)
    {
        // Skip "." and ".." entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        char path_1[1000];
        snprintf(path_1, sizeof(path_1), "%s/%s", path, entry->d_name);

        if (entry->d_type == DT_DIR)
        {
            // Recursive call for subdirectories
            rmtree(path_1);
        }
        else
        {
            // Delete files
            if (unlink(path_1) != 0)
            {
                // perror("Error deleting file");
                OrionHEN_log("Error deleting file %s", path);
            }
        }
    }

    closedir(dir);

    // Delete the empty folder
    if (rmdir(path) != 0)
    {
        // perror("Error deleting folder");
        OrionHEN_log("Error deleting folder %s", path);
    }

    return true;
}

uint64_t calculateTotalSize(const char *path)
{
    long totalSize = 0;
    DIR *dir = opendir(path);
    struct dirent *entry;
    char fullPath[1024];

    if (dir == NULL)
    {
        notify(false, "calculateTotalSize failed for %s", path);
        return 0;
    }

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        snprintf(fullPath, sizeof(fullPath), "%s/%s", path, entry->d_name);
        struct stat st;
        if (stat(fullPath, &st) == 0)
        {
            if (S_ISDIR(st.st_mode))
            {
                totalSize += calculateTotalSize(fullPath);
            }
            else if (S_ISREG(st.st_mode))
            {
                totalSize += st.st_size;
            }
        }
    }

    closedir(dir);
    return totalSize;
}

static const char *sizes[] = {"EiB", "PiB", "TiB", "GiB", "MiB", "KiB", "B"};
static const uint64_t exbibytes = 1024ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL;

#define DIM(x) (sizeof(x) / sizeof(x[0]))

void calculateSize(uint64_t size, char *result)
{
    uint64_t multiplier = exbibytes;
    int i;

    for (i = 0; i < DIM(sizes); i++, multiplier /= 1024)
    {
        if (size < multiplier)
        {
            continue;
        }
        if (size % multiplier == 0)
        {
            sprintf(result, "%lu %s", size / multiplier, sizes[i]);
        }
        else
        {
            sprintf(result, "%.1f %s", (float)size / multiplier, sizes[i]);
        }

        return;
    }
    strcpy(result, "0");
}


bool copyFile(const char *source, const char *destination, bool for_dumper)
{

    FILE *src = fopen(source, "rb");
    if (src == NULL)
    {
        notify(false, "copyFile failed for %s", source);
        OrionHEN_log("copyFile failed for %s", source);
        return false;
    }

    FILE *dest = fopen(destination, "wb");
    if (dest == NULL)
    {
        notify(false, "copyFile failed for %s", destination);
        OrionHEN_log("copyFile failed for %s", destination);
        fclose(src);
        return false;
    }

    char buffer[1024];
    size_t bytes = 0;

    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0)
    {
        fwrite(buffer, 1, bytes, dest);
        if(for_dumper)
           copiedSoFar += bytes;
    }

    fclose(src);
    fclose(dest);

    return true;
}

bool copyRecursive(const char *source, const char *destination)
{

    struct dirent *entry;
    char srcPath[1024];
    char destPath[1024];

    DIR *dir = opendir(source);
    if (dir == NULL)
    {
        notify(false, "copyRecursive failed for %s", source);
        return false;
    }

    mkdir(destination, 0777);

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        snprintf(srcPath, sizeof(srcPath), "%s/%s", source, entry->d_name);
        snprintf(destPath, sizeof(destPath), "%s/%s", destination, entry->d_name);

        struct stat st;
        if (stat(srcPath, &st) == 0)
        {
            if (S_ISDIR(st.st_mode))
            {
                copyRecursive(srcPath, destPath);
            }
            else if (S_ISREG(st.st_mode))
            {
                if (!copyFile(srcPath, destPath, true))
                {
                    notify(false, "copyRecursive failed for %s", srcPath);
                    return false;
                }
            }
        }
    }

    closedir(dir);

    return true;
}

