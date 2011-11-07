/*
 * makesparse.c
 *
 * This is a simple tool that uses the new fallocate flags to
 * punch holes in files that should be sparse (on file systems
 * that support it).
 *
 * Copyright (C) 2011  Adin Scannell <adin@scannell.ca>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

/* Ensure 64bit file support. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

/* For punchhole. */
#include <linux/falloc.h>
#ifndef FALLOC_FL_PUNCH_HOLE
#error "Sorry, looks like this system doesn't have FALLOC_FL_PUNCH_HOLE."
#endif

static char* real_zeros = NULL;
static int   real_zeros_len = 0;

int
is_zeros(char* data, unsigned int len)
{
    if( len > real_zeros_len )
    {
        real_zeros = realloc(real_zeros, len);
        memset(real_zeros, 0, len);
    }

    return !memcmp(data, real_zeros, len);
}

int
scanfile(int fd, off_t filesize, unsigned int blksize)
{
    off_t current = 0;
    char buffer[blksize];

    /*
     * NOTE: We don't do a mmap() of the file here, because
     * this utility will likely be used with very large files
     * (> 2GB) possibly on 32bit systems.  Since the mmap() 
     * would file in this case, and supporting mapping chunks
     * is probably not really worth it -- we just use standard
     * read() to grab the file contents.
     */
    while( current < filesize )
    {
        int toread = (filesize - current > blksize ?
                      blksize : filesize - current);
        int nread = read(fd, buffer, toread);
        if( nread < toread )
        {
            fprintf(stderr, "error: short read @ %lld: %s\n",
                    (unsigned long long)current, strerror(errno));
            return -1;
        }
        if( is_zeros(buffer, nread) )
        {
            if( fallocate(fd, FALLOC_FL_PUNCH_HOLE,
                          current, nread) < 0 )
            {
                fprintf(stderr, "error: hole punch @ %lld failed: %s\n",
                        (unsigned long long)current, strerror(errno));
                return -1;
            }
        }
        current += nread;
    }
}

void
usage(char* progname)
{
    fprintf(stderr, "usage: %s <filename>\n", progname);
    fprintf(stderr, "This utility uses a new Linux API for punching\n");
    fprintf(stderr, "holes in files on file systems that support it.\n");
    fprintf(stderr, "The file contents are scanned for zero blocks,\n");
    fprintf(stderr, "which are dropped.  This utility will not change\n");
    fprintf(stderr, "the logical file contents in any way.\n");
}

int
main(int argc, char** argv)
{
    int fd = -1;
    int rc = 0;
    struct stat fileinfo;

    if( argc != 2 ) 
    {
        usage(argv[0]);
        return 255;
    }

    fd = open(argv[1], O_RDWR | O_LARGEFILE);
    if( fd < 0 ) 
    {
        fprintf(stderr, "unable to open file: %s\n", strerror(errno));
        return 1;
    }

    if( fstat(fd, &fileinfo) < 0 ) 
    {
        fprintf(stderr, "unable to stat file: %s\n", strerror(errno));
        return 1;
    }

    rc = scanfile(fd, fileinfo.st_size, (unsigned int)fileinfo.st_blksize);
    if( rc < 0 )
        return rc;

    close(fd);
    return 0;
}
