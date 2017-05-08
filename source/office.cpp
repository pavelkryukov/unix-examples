/**
 * office.c
 *
 * Office simulator
 *
 * @author pikryukov
 *
 * Copyright (C) Pavel Kryukov, Konstantin Margorin 2010
 * for MIPT POSIX course
 *
 * Copyright (C) Pavel Kryukov 2012 (remastering)
 */

// C generic
#include <cstdlib>     // exit
#include <cstring>     // strerror
#include <cerrno>      // errno

// C++ generic
#include <iostream>
#include <iomanip>
#include <queue>
#include <vector>

// POSIX generic
#include <unistd.h>     // sleep

// Pthreads
#include <pthread.h>

#define wTime 1        // wTime - average time for preparing document
#define pTime 2        // pTime - average time for printing document

/**
 * Mutex class wrapper
 */
class Mutex
{
private:
    friend class Cond;
    pthread_mutex_t m;
public:
    inline Mutex()  { pthread_mutex_init(&m, NULL); }
    inline ~Mutex() { pthread_mutex_destroy(&m); }
    inline int Lock()    { return pthread_mutex_lock(&m); }
    inline int Unlock()  { return pthread_mutex_unlock(&m); }
    inline int TryLock() { return pthread_mutex_trylock(&m); }
};

/**
 * Condition class wrapper
 */
class Cond
{
private:
    pthread_cond_t c;
public:
    inline Cond()  { pthread_cond_init(&c, NULL); }
    inline ~Cond() { pthread_cond_destroy(&c); }
    inline int Wait(Mutex& mutex)
    {
        return pthread_cond_wait(&c, &mutex.m);
    }
    inline int TimedWait(Mutex& mutex, const timespec* time)
    {
        return pthread_cond_timedwait(&c, &mutex.m, time);
    }
    inline int Signal() { return pthread_cond_signal(&c); }
    inline int Broadcast() { return pthread_cond_broadcast(&c); }
};

/**
 * Printer use table
 */
class Table : private std::vector<std::vector<unsigned> >
{
private:
    size_t N;   // amount of workers
    size_t M;   // amount of printers
    size_t K;   // amount of jobs of every worker
    int counter;            // unfinished jobs
    mutable Mutex mutex;
    typedef typename std::vector<unsigned> Row;
    typedef typename std::vector<std::vector<unsigned> > base;    
public:
    // Constructor
    Table(size_t N, size_t M, size_t K) 
        : base(N, Row(M, 0))
        , N(N)
        , M(M)
        , K(K)
        , counter(N * K)
    { }
    
    // Getters
    inline size_t GetK() const { return K; }
    inline size_t GetM() const { return M; }
    
    // Mutexed addition
    void Add(int rank, int document)
    {
        mutex.Lock();
        ++(*this)[document][rank];
        --counter;
        std::cout << "Printer " << rank << ": document from " << document << 
            " (" << counter << " left)." << std::endl;
        mutex.Unlock();
    }
    
    // If all jobs have been already finished, returns true (method is mutexed)
    bool Check() const
    {
        mutex.Lock();
        int result = counter > 0;
        mutex.Unlock();
        return result;
    }
    
    // Print of table
    void Print() const
    {
        Row sum(M, 0);
        for (size_t i = 0; i < N; ++i)
        {
            std::cout << "| " << std::setw(2) << i << " ||";
            for (size_t j = 0; j < M; ++j)
            {
                std::cout << " " << std::setw(2) << (*this)[i][j] << " |";
                sum[j] += (*this)[i][j];
            }
            std::cout << std::endl;
        }
        std::cout << "------";
        for (size_t j = 0; j < M; j++)
            std::cout << "-----";

        std::cout << std::endl << "|  S ||";
        for (size_t j = 0; j < M; j++)
            std::cout << " " << std::setw(2) << sum[j] << " |";

        std::cout << std::endl;
    }
};

/**
 * Document queue type
 */
class Queue : private std::queue<int>
{
private:
    size_t qSize;           // size of queue
    Mutex mutex;
    Cond doc_exist;
    Cond free_space_exist;
public:
    Queue(size_t qSize)
        : qSize(qSize)
    { }

    // Muted push
    // If queue is not empty, broadcasts doc_exist
    void Push(int what)
    {
        mutex.Lock();
        while (qSize == this->size())
            free_space_exist.Wait(mutex);
        
        this->push(what);
        
        if (this->size() == 1)
            doc_exist.Broadcast();
        
        mutex.Unlock();
    }

    // Muted pop
    // If queue is not full, broadcasts free_space_exist
    int Pop() 
    {
        mutex.Lock();
        while(this->empty())
            doc_exist.Wait(mutex);

        int result = this->front();
        this->pop();        

        if (this->size() == qSize - 1)
            free_space_exist.Broadcast();

        mutex.Unlock();
        return result;
    }
};

/**
 * Task type
 */
struct Task {
    Table* table;
    Queue* q;
    int rank;
    std::vector<Task*>& tasks;
    
    Task(Table* table, Queue* q, int rank, std::vector<Task*>& tasks)
        : table(table)
        , q(q)
        , rank(rank)
        , tasks(tasks)
    { }
};

/**
 * Worker thread
 * @param t pointer to task_t
 */
void* worker(void* t)
{
    Task* task   = reinterpret_cast<Task*>(t);
    Table* table = task->table;
    Queue* q     = task->q;
    size_t rank  = task->rank;
    size_t K     = table->GetK();
    std::cout << "Worker " << std::setw(3) << rank
        << ": came to office" << std::endl;

    for (size_t sended = 0; sended < K; ++sended)
    {
        sleep(wTime);
        std::cout << "Worker " 
            << std::setw(3) << rank 
            << ": wrote " << sended << " document" << std::endl;
        q->Push(rank);
    }

    std::cout << "Worker " << std::setw(3) << rank 
        << ": went home" << std::endl;
    pthread_exit(EXIT_SUCCESS);
}

/**
 * Printer thread
 * @param t pointer to task_t
 */
void* printer(void* t)
{
    Task* task   = reinterpret_cast<Task*>(t);
    Table* table = task->table;
    Queue* q     = task->q;
    size_t rank  = task->rank;
    size_t M     = table->GetM();
    std::cout << "Printer "
        << std::setw(3) << rank << ": turned on" << std::endl;

    while (table->Check())
    {
        const int document = q->Pop();
        sleep(pTime);
        table->Add(rank, document);
    }
    std::cout << "Office was closed." << std::endl;
    table->Print();
    delete q;
    std::vector<Task*>& tasks = task->tasks;

    for (size_t i = 0; i < M; ++i)
        delete tasks[i];

    delete table;
    std::exit(EXIT_SUCCESS);
}

/**
 * Entry point
 * @param argc
 * @param argv
 */
int main(int argc, char** argv)
{
    if (argc != 5)
    {
        std::cerr << "Syntax error" << std::endl
            << "Parameters are sizes of: workers printers documents queue"
            << std::endl;
        exit(EXIT_FAILURE);
    }

    size_t N = strtoul(argv[1], NULL, 0);
    size_t M = strtoul(argv[2], NULL, 0);
    size_t K = strtoul(argv[3], NULL, 0);
    size_t qSize = strtoul(argv[4], NULL, 0);

    Table* table = new Table(N, M, K);
    Queue* q = new Queue(qSize);

    std::cout << "Office was opened" << std::endl;

    std::vector<pthread_t> workers(N);
    std::vector<Task*> workersTasks(N);
    std::vector<pthread_t> printers(M);
    std::vector<Task*> printersTasks(M);

    for (size_t i = 0; i < N; i++)
    {
        workersTasks[i] = new Task(table, q, i, printersTasks);
        if (pthread_create(&workers[i], 0, worker,
            (void*)workersTasks[i]) == -1)
        {
            std::cerr << "Error in creating thread for "
                << i << " worker (Error " 
                << errno << ": " << std::strerror(errno) << std::endl;
            std::exit(EXIT_SUCCESS);
        }
    }

    for (size_t i = 0; i < M; i++)
    {
        printersTasks[i] = new Task(table, q, i, printersTasks);
        if (pthread_create(&printers[i], 0, printer,
            (void*)printersTasks[i]) == -1)
        {
            std::cerr << "Error in creating thread for "
                << i << " printer (Error " 
                << errno << ": " << std::strerror(errno) << std::endl;
            std::exit(EXIT_FAILURE);
        }
    }

    for (size_t i = 0; i < N; i++)
    {
        pthread_join(workers[i], NULL);
        delete workersTasks[i];
    }
    
    for (size_t i = 0; i < M; i++)
    {
        pthread_join(printers[i], NULL);
    }

    return 0;
}
