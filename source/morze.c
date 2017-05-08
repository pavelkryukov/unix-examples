/**
 * morze.c
 *
 * Copy file with use of POSIX signals
 *
 * @author pikryukov
 *
 * Copyright (C) Pavel Kryukov, Konstantin Margorin 2010
 * for MIPT POSIX course
 *
 * Copyright (C) Pavel Kryukov 2012 (remastering)
 */

/* C generic */
#include <stdio.h>        /* fprintf, printf */
#include <string.h>       /* strerror, memset */
#include <stdlib.h>       /* exit */
#include <errno.h>        /* errno */

/* POSIX generic */
#include <sys/stat.h>    /* fstat, fchmod, lstat, chmod */
#include <sys/wait.h>    /* waitpid */
#include <unistd.h>      /* fork, close, getppid, sleep, read, write */
#include <fcntl.h>       /* O_RDONLY, O_WRONLY, O_CREAT, O_EXCL */
#include <signal.h>      /* kill, sigaction */

/* Signals defines */
#define READY SIGRTMAX /* READY is send by receiver to get next bit */
#define SEND1 SIGUSR1  /* SEND1 is send by sender if data is 1 */
#define SEND0 SIGUSR2  /* SEND0 is send by sender if data is 0 */
#define KILLER SIGINT  /* KILLER is used by both process for stop */

int workfile;                /* working file */
unsigned char byte;          /* byte buffer */
int bit_num;                 /* counter of received/sent bits */

int cpid;                    /* pid of child-sender */
int ppid;                    /* pid of parent-receiver */

/**
 * Sender error handler
 * @param signo number of signal
 */
void sender_handler(int signo)
{
    if (signo == KILLER)
    {
        /* Error message had been already written by receiver */
        close(workfile);
        exit(EXIT_SUCCESS);
    }
    if (signo == READY)
    {
        if (bit_num == 8) /* previous byte is sent already */
        {
            errno = 0;
            /* Read new byte */
            if (read(workfile, &byte, 1) == 0)
            {
                kill(ppid, KILLER);
                close(workfile);
                if (errno)
                {
                    fprintf(stderr,
                        "Sender: error in reading file (Error %d: %s)\n",
                        errno, strerror(errno));
                    exit(EXIT_FAILURE);
                }
                exit(EXIT_SUCCESS);
            }
            bit_num = 0;
        }
        /* Send bit of data */
        kill(ppid, ((byte >> bit_num++) % 2) ? SEND1 : SEND0);
    }
}

/**
 * Receiver error handler
 * @param signo number of signal
 */
void receiver_handler(int signo)
{
    if (signo == KILLER)
    {
        close(workfile);
        waitpid(cpid, NULL, 0);
        printf("Finished!\n");
        exit(EXIT_SUCCESS);
    }
    /* Append byte */
    if ((signo == SEND1) || (signo == SEND0))
    {
        byte += ((signo == SEND1) << bit_num++);
        if (bit_num == 8) /* Byte is completed */
        {
            if (write(workfile, &byte, 1) == -1)
            {
                fprintf(stderr,
                    "Receiver: error in writing to file (Error %d: %s)\n",
                     errno, strerror(errno));
                kill(cpid, KILLER);
                kill(ppid, KILLER);
            }
            byte = 0;
            bit_num = 0;
        }
        /* Ask for next bit */
        kill(cpid, READY);
    }
}

/**
 * Entry point
 * @param argc
 * @param argv
 */
int main(int argc, char** argv)
{
    struct sigaction new_action;
    struct sigaction old_action;
    int cpid;
    sigset_t mask;

    if (argc != 3)
    {
        fprintf(stderr, "Syntax error\n");
        exit(EXIT_FAILURE);
    }

    /* Sender handling settings */
    memset(&new_action, 0, sizeof(struct sigaction));
    new_action.sa_handler = &sender_handler;

    sigaction(KILLER, &new_action, &old_action);
    sigaction(READY, &new_action, &old_action);
    sigaction(SEND0, &new_action, &old_action);
    sigaction(SEND1, &new_action, &old_action);

    sigemptyset(&mask);
    sigaddset(&mask, READY);
    sigaddset(&mask, SEND0);
    sigaddset(&mask, SEND1);

    /* Blocking all signals before start */
    sigprocmask(SIG_BLOCK, &mask, NULL);

    cpid = fork();
    if (cpid == -1)
    {
        fprintf(stderr, "Error in creating process (Error %d: %s)\n",
            errno, strerror(errno));
        exit(EXIT_SUCCESS);
    }
    if (cpid == 0)
    {
        /* CHILD is sender */
        ppid = getppid();
    /*    sleep(10); */

        workfile = open(argv[1], O_RDONLY);
        if (workfile == -1)
        {
            fprintf(stderr, "Error in opening file %s (Error %d: %s)\n",
                argv[1], errno, strerror(errno));
            kill(ppid, KILLER);
            exit(EXIT_FAILURE);
        }

        /* Fill bit_num to 8 for filling byte from file */
        bit_num = 8;

        /* Unblocking signals and start */
        sigprocmask(SIG_UNBLOCK, &mask, NULL);

        while(1) sleep(10);
    }
    else
    {
        /* Parent is receiver */

        /* Receiver handling settings */
        memset(&new_action, 0, sizeof(struct sigaction));
        new_action.sa_handler = &receiver_handler;

        sigprocmask(SIG_UNBLOCK, &mask, NULL);
        sigaction(KILLER, &new_action, &old_action);
        sigaction(READY, &new_action, &old_action);
        sigaction(SEND0, &new_action, &old_action);
        sigaction(SEND1, &new_action, &old_action);

        /* Write file */
        workfile = open(argv[2], O_WRONLY|O_CREAT|O_EXCL, S_IRWXO|S_IRWXG|S_IRWXU);
        if (workfile == -1)
        {
            fprintf(stderr, "Error in creating file %s (Error %d: %s)\n",
                argv[2], errno, strerror(errno));
            kill(cpid, KILLER);
            exit(EXIT_FAILURE);
        }

        bit_num = 0;
        byte = 0;

        /* Radio. Live transmission. */
        printf("Translation started...\n");
        sleep(1);
        kill(cpid, READY);

        while(1) sleep(10);
    }
}
