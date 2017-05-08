/**
 * mycopy.c
 *
 * Copies file with its rwx mask
 * Copies symbol link destination
 * If -l flag is provided, creates new symlink
 *
 * @author pikryukov
 *
 * Copyright (C) Pavel Kryukov, Konstantin Margorin 2010
 * for MIPT POSIX course
 *
 * Copyright (C) Pavel Kryukov 2012 (remastering)
 */

/* C generic */
#include <string.h>       /* strcmp, strcpy, strerror */
#include <errno.h>        /* strerror, EINVAL, errno */
#include <stdlib.h>       /* malloc, free */
#include <stdio.h>        /* fprintf */

#define _GNU_SOURCE

/* POSIX generic */
#include <sys/stat.h>    /* fstat, fchmod, lstat, chmod */
#include <unistd.h>      /* readlink, symlink, read, write, close */
#include <fcntl.h>       /* O_RDONLY, O_WRONLY, O_CREAT, O_EXCL */

#define BUF_LEN 65536    /* copy buffer length */
#define ADR_LEN 256      /* UNIX address length */

/**
 * Mask copy
 * @param src source filenane
 * @param dst destination filename
 * @return 0 on success
 * @return 1 on error
 */
int maskCopy(char* src, char* dst)
{
    struct stat p_info;
    int result = lstat(src, &p_info);
    if (result == -1)
    {
        fprintf(stderr,"Failed to read mask from '%s' (Error %d: %s)\n",
            src,errno,strerror(errno));
        return 1;
    }

    result = chmod(dst, p_info.st_mode);
    if (result == -1)
    {
        fprintf(stderr, "Failed to write mask to '%s' (Error %d: %s)\n",
            dst, errno, strerror(errno));
        return 1;
    }
    return 0;
}

/**
 * Usual file copy
 * @param src source filename
 * @param dst destination filename
 * @return 0 on success
 * @return 1 on error
 */
int justCopy(char* src, char* dst){
    int result = 0;
    int d_dst, d_src;
    char* buf;

    /* Source file processing */
    d_src = open(src, O_RDONLY);
    if (d_src == -1)
    {
        fprintf(stderr, "Failed to open '%s' (Error %d: %s)\n",
            src, errno, strerror(errno));
        return 1;
    }

    /* Destination file processing */
    d_dst = open(dst, O_WRONLY|O_CREAT|O_EXCL);
    if (d_dst == -1)
    {
        fprintf(stderr, "Failed to create '%s' (Error %d: %s)\n",
            dst, errno, strerror(errno));
        close(d_src);
        return 1;
    }

    buf = (char*)malloc(BUF_LEN); /* Read buffer */
    while (result = read(d_src, buf, BUF_LEN), result > 0)
    {
        /* Check read errors */
        if (result == -1)
        {
            fprintf(stderr, "Failed to read from '%s' (Error %d: %s)\n",
                src, errno, strerror(errno));
            close(d_src);
            close(d_dst);
            return 1;
        }

        /* Write */
        result = write(d_dst, buf, result);
        if (result == -1)
        {
            fprintf(stderr,"Failed to write to '%s' (Error %d: %s)\n",
                dst,errno,strerror(errno));
            close(d_src);
            close(d_dst);
            return 1;
        }
    }
    free(buf);

    close(d_src);
    close(d_dst);
    return 0;
}

/**
 * Link copy
 * @param src source file
 * @param dst destination file
 * @return 0 on success
 * @return 1 on error
 */
int linkCopy(char* src, char* dst){
    int result = 0;

    /* Buffer to save link destination */
    char linkto[ADR_LEN];
    memset(linkto, 0, ADR_LEN);
    result = readlink(src, linkto, ADR_LEN);
    if (result == -1)
    {
        /* File is not a link, so we call justCopy */
        if (errno == EINVAL)
            return justCopy(src, dst);
        fprintf(stderr, "Link detection in '%s' failed: (Error %d: %s)\n",
            src, errno, strerror(errno));
        return 1;
    }

    /* Creating new symlink */
    result = symlink(linkto, dst);
    if (result == -1)
    {
        fprintf(stderr, "Failed to create link in '%s' (Error %d: %s)\n",
            dst, errno, strerror(errno));
        return 1;
    }
    return 0;
}

/**
 * Entry point
 * @param argc
 * @param argv
 * @return 0 on success
 * @return 1 on error
 * @return -1 on syntax error
 */
int main(int argc, char** argv)
{
    errno = 0;

    /* Standard mode */
    if (argc == 3)
    {
        int result = justCopy(argv[1],argv[2]);
        return result
            ? result
            : maskCopy(argv[1],argv[2]);
    }

    /* -l mode */
    if ((argc == 4) && (strcmp(argv[1],"-l ")))
    {
        int result = linkCopy(argv[2],argv[3]);
        return result
            ? result
            : maskCopy(argv[2],argv[3]);
    }

    fprintf(stderr,"Syntax error");
    return -1;
}
