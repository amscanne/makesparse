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

static inline int
is_zeros_aligned(unsigned long* data, unsigned int len)
{
    unsigned long r1 = 0;
    unsigned long r2 = 0;
    unsigned long r3 = 0;
    unsigned long r4 = 0;
    int i;

    for( i = 0; i < len / (4 * sizeof(unsigned long)); i++ )
    {
        r1 |= *(data++);
        r2 |= *(data++);
        r3 |= *(data++);
        r4 |= *(data++);
    }

    r1 |= r3;
    r2 |= r4;
    r1 |= r2;

    return !r1;
}

static inline int
is_zeros_unaligned(char* data, unsigned int len)
{
    int i;
    for( i = 0; i < len; i++ )
        if( data[i] != 0 )
            return 0;
    return 1;
}

static inline int
is_zeros(char* data, unsigned int len)
{
    unsigned int tail = len % (4 * sizeof(unsigned long));
    return is_zeros_aligned((unsigned long*)data, len - tail)
        && is_zeros_unaligned(data + (len - tail), tail);
}

int
scanfile(int fd, off_t filesize, unsigned int blksize)
{
    off_t current = 0;
    char buffer[blksize];

    /*
     * NOTE: We don't do a full mmap() of the file here, because
     * this utility will likely be used with very large files
     * (> 2GB) possibly on 32bit systems.  Since the mmap() 
     * would fail in this case, and supporting mapping chunks
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
            fprintf(stderr, "error: short read @ %llu: %s\n",
                    (unsigned long long)current, strerror(errno));
            return -1;
        }
        if( is_zeros(buffer, nread) )
        {
            if( fallocate(fd, FALLOC_FL_PUNCH_HOLE|FALLOC_FL_KEEP_SIZE,
                          current, blksize) < 0 )
            {
                fprintf(stderr, "error: hole punch @ %llu failed: %s\n",
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
