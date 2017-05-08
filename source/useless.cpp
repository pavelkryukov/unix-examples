/**
 * useless.c
 *
 * Command scheduler
 *
 * @author pikryukov
 *
 * Copyright (C) Pavel Kryukov, Konstantin Margorin 2010
 * for MIPT POSIX course
 *
 * Copyright (C) Pavel Kryukov 2012 (remastering)
 */

// C generic
#include <cstdio>       // FILE, fopen, fclose
#include <cstdlib>      // exit, system, strtoul, malloc, realloc
#include <cstring>      // strerror, strncpy, strlen, strtok
#include <ctime>        // time
#include <cerrno>       // errno2

// C++ generic
#include <iostream>
#include <list>
#include <algorithm>

// POSIX generic
#include <sys/types.h>
#include <sys/wait.h>    // waitpid
#include <unistd.h>      // sleep, fork, execvp

#define STDSIZE 256

/**
 * Command struct type
 */
class Task
{
private:
    int delay;              // delay
    char **arguments;       // arguments
    size_t arg_counter;     // amount of arguments
    int id;                 // assigned pid
    mutable bool finished;  // if 1, command is finished
public:
    Task(char* line) : id(0), finished(false)
    {
        // -1 column has delay and won't be copied to arguments list
        int i = -1;

        char* token = strtok(line, " \n");

        arguments = (char**)malloc(sizeof(char*));

        while (token != NULL)
        {
            // Read delay
            if (i == -1)
            {
                delay = strtoul(token, NULL, 0);
            }
            else {
                arguments = (char**)realloc(arguments, (i + 1) * sizeof(char*));
                arguments[i] = (char*)malloc(strlen(token) * sizeof(char));
                strcpy(arguments[i], token);
            }

            // Next token
            i++;
            token = strtok(NULL," \n");
        }

        // Fill last word with NULL for execvp
        arguments = (char**)realloc(arguments, (i + 1) * sizeof(char*));
        arguments[i] = NULL;
        arg_counter = i + 1;
    }
    
    inline void setId(int pid) { this->id = pid; }
    inline int getDelay() const { return this->delay; }
    
    friend std::ostream& operator<<(std::ostream& out, const Task& what);
    friend bool operator<(const Task& fst, const Task& snd);
    
    void Test(int options, int start_time) const
    {
        int status = 0;
        if (!finished && waitpid(id, &status, options))
        {
            std::cout << "[Command '" << arguments[0] << "' finished at "
                << (int)(time(NULL) - start_time) << " with status " << status
                << "]" << std::endl;
            finished = true;
        }
    }
    
    inline int Run() const { return execvp(arguments[0], arguments);}
    
    ~Task()
    {
        for (size_t j = 0; j < arg_counter - 1; ++j)
            free(arguments[j]);
        free(arguments);
    }
};


std::ostream& operator<<(std::ostream& out, const Task& what)
{
    out << "'" << what.arguments[0] << "' at " << what.delay;
    return out;
}

inline bool operator<(const Task& fst, const Task& snd)
{
    return fst.delay < snd.delay;
}

class CommandList : public std::list<Task*>
{
    bool isValid;
    int start_time;
    typedef typename std::list<Task*> base;
public:
    typedef typename base::iterator iterator;
    typedef typename base::const_iterator const_iterator;

    CommandList(FILE* fp) : start_time(time(NULL))
    {
        char* line = new char[STDSIZE];

        // Reading lines and create commands
        while (fgets(line, STDSIZE, fp) != NULL)
        {
            this->push_front(new Task(line));
            if (this->front()->getDelay() < 0)
            {
                std::cerr << "Invalid delay value\n";
                isValid = false;
                break;
            }
        }

        // fgets error handler
        if (errno)
        {
            std::cerr << "Error in reading file (Error " 
                << errno << ": " << std::strerror(errno) << std::endl;
            isValid = false;
        }
        else {
            isValid = true;
        }
        delete[] line;
    }
    
    void Test(iterator from,
        iterator to, int options = 0) const
    {
        const_iterator jt;
        for (jt = from; jt != to; ++jt)
            (*jt)->Test(options, start_time);
    }
    
    inline bool IsValid() const { return isValid; }
    
    ~CommandList()
    {
        const_iterator jt;
        for ( jt = this->begin(); jt != this->end(); ++jt)
            delete *jt;    
    }
};

/**
 * Entry point
 * @param argc
 * @param argv
 * @return 0 on success
 * @return 1 on error
 * @return -1 on syntax error
 */
int main (int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Syntax error" << std::endl
            << "The only parameter is the name of command file" << std::endl;
        return -1;
    }

    FILE *fp = fopen(argv[1],"r");
    if (fp == NULL)
    {
        std::cerr << "Error in opening '" << argv[1] <<"' (Error " 
                << errno << ": " << std::strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }

    CommandList commands(fp);
    fclose(fp);
    
    if (!commands.IsValid())
        return 1;

    commands.sort();

    // Wait for time to run 1st program
    sleep(commands.front()->getDelay());
    for (CommandList::iterator it = commands.begin(); it != commands.end(); ++it)
    {
        int pid = fork();
        if (pid == -1)    // Fork error handling
        {
            std::cerr << "Error in creating process " << **it 
                << "' (Error " << errno 
                << ": " << std::strerror(errno) << std::endl;
            return 1;
        }
        if (pid == 0) // Child
        {
            std::cout << "[Starting command " << **it << "]" << std::endl;

            // Run
            if ((*it)->Run() == -1)
            {
                std::cerr << "Error in executing process " << **it 
                    << "' (Error " << errno 
                    << ": " << std::strerror(errno) << std::endl;
                return 1;
            }

            return 0;
        }
        else { // Parent
            (*it)->setId(pid);

            // If process is not the last, sleep to time to run next
            CommandList::iterator ct = it;
            ++ct;
            if (ct != commands.end())
                sleep((*ct)->getDelay() - (*it)->getDelay());

            // Check if anybody has already finished
            // WNOHANG doesn't wait for finish
            commands.Test(commands.begin(), it, WNOHANG);
        }
    }

    // Wait for all processes
    commands.Test(commands.begin(), commands.end());

    return 0;
}
