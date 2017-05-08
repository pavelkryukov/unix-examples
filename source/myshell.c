/**
 * myshell.c
 *
 * Shell with pipe support
 *
 * @author pikryukov
 *
 * Copyright (C) Pavel Kryukov, Konstantin Margorin 2010
 * for MIPT POSIX course
 *
 * Copyright (C) Pavel Kryukov 2012 (remastering)
 */

/* C generic */
#include <stdio.h>        /* fprintf, printf, fgets, stdin */
#include <string.h>       /* strlen, strtok, strcpy, strncpy, strcmp, strerror */
#include <stdlib.h>       /* malloc, realloc, exit */
#include <errno.h>        /* errno */

/* POSIX generic */
#include <sys/wait.h>    /* waitpid */
#include <unistd.h>      /* fork, pipe, close, dup2, execvp, read */

#define STDSIZE 256

/**
 * Frees two-dimension array with NULL terminator
 * @param array array
 */
void char_free(char **array)
{
    size_t i = 0;
    while (array[i] != NULL)
        free(array[i++]);
    free(array);
}

/**
 * Parse string to string array with memory allocation
 * @param line parsed string
 * @param dividers string with dividers
 * @return array of strings with NULL terminator
 */
char** parser(char *line, char *dividers)
{
    int i = 0;

    /* First token */
    char* token = strtok(line, dividers);

    /* Array with strings */
    char** array = malloc(sizeof(char*));

    while (token != NULL)
    {
        /* Reallocation */
        array = (char**)realloc(array, (i + 1) * sizeof(char*));

        /* Copy token */
        array[i] = (char*)malloc(strlen(token) * sizeof(char));
        strcpy(array[i], token);

        /* Next token */
        i++;
        token = strtok(NULL, dividers);
    }
    /* Fill the last line with NULL for execvp */
    array = realloc(array, (i + 1) * sizeof(char*));
    array[i] = NULL;
    return array;
}

/**
 * Execution of command pipeline
 * @param Cmd command pipeline((command (argument), (argument)) (command (arg)))
 * @param counter amount of commands in pipeline
 */
int MyExec(char ***Cmd, int counter)
{
    char s = 0;     /* Buffer symbol */
    char s1 = 0;    /* Previous symbol */

    int readres = 0;
    size_t j, k = 0;
    
    int words = 0;   /* counters */
    int strings = 0;
    int symbols = 0;

    int** pipes = (int**)malloc(sizeof(int*) * counter);    /* pipes */
    int* pid = (int*)malloc(sizeof(int) * counter);   /* process ids */

    for (j = 0; j < counter; j++)
    {
        pipes[j] = (int*)malloc(sizeof(int) * 2);
        if (pipe(pipes[j]) == -1)
        {
            fprintf(stderr, "Error in creating %d pipe (Error %d: %s)\n",
                (int)j + 1, errno, strerror(errno));
            return -1;
        }
    }

    /* Make counter forks */
    for (j = 0; j < counter; j++)
    {
        pid[j] = fork();
        if (pid[j] == -1)
        {
            fprintf(stderr, "Error in creating process %s, (Error %d: %s)\n",
                Cmd[j][0],errno, strerror(errno));
            free(pid);
            free(pipes);
            return -1;
        }
        /* CHILD */
        if (pid[j] == 0)
        {
            size_t k;
            /* Everyone except first one creates read pipe */
            if (j != 0)
            {
                close(pipes[j-1][1]);
                dup2(pipes[j-1][0],0);
                close(pipes[j-1][0]);
            }

            /* Creating write pipe */
            close(pipes[j][0]);
            dup2(pipes[j][1],1);
            close(pipes[j][1]);

            /* Close all unnessesary pipes */
            for (k = 0; k < counter; k++)
                if ((k != j) && (k != j-1))
                {
                    close(pipes[k][0]);
                    close(pipes[k][1]);
                    free(pipes[k]);
                }

            if (execvp(Cmd[j][0], Cmd[j]) == -1);            /* выполнение */
            {
                fprintf(stderr, 
                    "Error in executing command %s (Error %d: %s)\n",
                    Cmd[j][0], errno, strerror(errno));
                free(pid);                
                free(pipes[j]);
                free(pipes[j-1]);
                free(pipes);
                return -1;
            }
        }
    }

    /*PARENT */

    /* Close all unnessesary pipes */
    for (k = 0; k < counter - 1; k++)
    {
        close(pipes[k][0]);
        close(pipes[k][1]);
        free(pipes[k]);
    }

    /* Close last pipe read end */
    close(pipes[counter-1][1]);

    /* Reading from last pipe */
    while(readres = read(pipes[counter-1][0], &s, 1), readres > 0)
    {
        /* If previous symbol was not a splitter, and this is */
        /* increment amount of words */
        if (((s == ' ') || (s == '\n')) && (s1 != ' ') && (s1 != '\n') && (s1 != 0))
            words++;

        /* String splitter */
        if(s == '\n')
            strings++;
        symbols++;

        /* Show symbol on screen */
        printf("%c", s);
        s1 = s;
    }

    /* Error handler */
    if (readres == -1)
    {
        fprintf(stderr, "Error during sending data to MyShell\n");
        free(pid);
        free(pipes);
        return -1;
    }

    /* Close last pipe */
    close(pipes[counter - 1][0]);
    free(pipes[counter - 1]);

    /* Wait for zombies */
    for (j = 0; j < counter; j++)
          waitpid(pid[j], NULL, 0);

    printf("[symbols: %d]\n[words: %d]\n[lines: %d]\n", symbols, words, strings);
    free(pid);
    free(pipes);
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
int main(int argc, char *argv[])
{
    char* Line = (char*)malloc(sizeof(char) * STDSIZE);
    char** Cmdstr = NULL;                  /* array of commands divided by | */
    char*** Cmd = (char***)malloc(sizeof(char**));  /* full pipeline */
    int counter = 0;
    
    if (argc > 1)                        /* проверка на синтаксис */
    {
        printf("This program doesn't need any arguments\n");
        exit(EXIT_SUCCESS);
    }

    printf("********************\n");
    printf("MyShell ver 2.0\nTo exit, type 'exit'\n");
    printf("You can use '|' to create pipe between commands\n");
    printf("********************\n");

    while(1)
    {
        printf("C:\\>");
        fgets(Line, STDSIZE, stdin);
        if (!strcmp(Line, "exit\n"))
        {        
            free(Cmd);
            free(Line);
            return 0;
        }
        counter = 0;

        /* Split commmand by '|' */
        Cmdstr = parser(Line, "|");
        while (Cmdstr[counter] != NULL)
        {
            /* Split command by ' ' */
            Cmd = realloc(Cmd, (counter + 1) * sizeof(char**));
            Cmd[counter] = parser(Cmdstr[counter], " \n");
            counter++;
        }

        /* Exec */
        MyExec(Cmd, counter);
        
        /* Mem free */
        char_free(Cmdstr);
        counter = 0;
        while (Cmdstr[counter] != NULL)
            char_free(Cmd[counter++]);
    }
}
