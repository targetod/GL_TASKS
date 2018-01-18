
/* result of task:
*  Intel Core 2 Duo 2 GHz
*  ArrSize = 100000.  NumArr = NumThread            Tsort us
*   1. without thread (2 arr)                   110000
*   2. without thread (4 arr)                   220000
*   3. without thread (8 arr)                   430000
*   4. without thread (16 arr)                  880000
*
*   1. 2 threads                                65000
*   2. 4 threads                                120000 
*   3. 8 threads                                230000
*   4. 16 threads                               470000
** one thread ~ 30000 us
*
*   1. 2 threads_sem                                65000
*   2. 4 threads_sem                               117000 
*   3. 8 threads_sem                                230000
*   4. 16 threads_sem                               522000
*
*/

#include <iostream>
#include <vector>
#include <pthread.h>
//#include <thread>
#include <algorithm>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <semaphore.h>

//#define SHOW

//#define THREAD
// or
#define THREAD_SEMAPHORE
//semaphore for shared static local variable in thread_func()
static sem_t count_sem;

void initArray(std::vector<int> & Arr);
//void sortArr (int numArrs, int size);
void showArray(const std::vector<int> & Arr);
void* thread_sort_fnc(void* arg);
void* thread_sem_sort_fnc(void* arg);

inline long long gettimeus(){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long) tv.tv_sec * 1000000LL + (long long) tv.tv_usec;
}

int main(){
    // get number of cores
    // c++11 from  <thread>
    // int n = std::thread::hardware_concurrency();
    // std::cout << "num cores = " << n <<std::endl;
    // linux cmd from unistd.h
    int numCPU = sysconf(_SC_NPROCESSORS_ONLN);
    std::cout << "num cores = " << numCPU <<std::endl;
 
    int sizeArr = 100000; 
    int numArr = 8;

    std::vector<std::vector<int>> vArrs(numArr);
    
    
    for (int i= 0; i < numArr; ++i){
        vArrs[i].resize(sizeArr);
        initArray (vArrs[i]);
    }

#ifdef SHOW
    for (int i= 0; i < numArr; ++i){
        showArray (vArrs[i]);
    }
#endif
    
    // profiler
    long long tm = -gettimeus();

#ifdef THREAD
    pthread_t* pThrds = new pthread_t [numArr];
    for(int i = 0; i < numArr; ++i){
        pthread_create(pThrds+i, NULL, thread_sort_fnc, (void*) &vArrs[i]);
    }
    for(int i = 0; i < numArr; ++i){
        pthread_join(pThrds[i], NULL);
    }
    delete[] pThrds;
#elif defined THREAD_SEMAPHORE 
// sem_init() -> arg 3, allow at most one thread to have access
    if (sem_init(&count_sem, 0, numCPU) == -1){
        std::cout << "Semaphore error\n";
    }
    pthread_t* pThrds = new pthread_t [numArr];
    for(int i = 0; i < numArr; ++i){
        pthread_create(pThrds+i, NULL, thread_sem_sort_fnc, (void*) &vArrs[i]);
    }
    for(int i = 0; i < numArr; ++i){
        pthread_join(pThrds[i], NULL);
    }
    delete[] pThrds;
#else // WITHOUT THREAD 
    for(int i = 0; i < numArr; ++i){
        std::sort( vArrs[i].begin(), vArrs[i].end() );
    }
#endif

    tm += gettimeus();

#ifdef SHOW
    for (int i= 0; i < numArr; ++i){
        showArray (vArrs[i]);
    }
#endif

    std::cout << "Num Arr = " << numArr << "\n"
            << "Size Arr = " << sizeArr << "\n" 
            << "Num Thread = " << numArr << "\n" 
            << "T sort(us) = " << tm << "\n";

    return 0;
}


void* thread_sort_fnc(void* arg){
    std::vector<int>* pArr = static_cast<std::vector<int>*> (arg);
    std::sort(pArr->begin(), pArr->end());
}

void* thread_sem_sort_fnc(void* arg){ 
    std::vector<int>* pArr = static_cast<std::vector<int>*> (arg);
    // enforce mutual exclusion for access to pArr
    sem_wait(&count_sem); /* start of critical section */
    //std::cout << "cnt sem = " << 1 <<std::endl; // for test sem
    std::sort(pArr->begin(), pArr->end());
    sem_post(&count_sem); /* end of critical section */
}


void initArray(std::vector<int> & Arr){
    for (auto & elem : Arr ){
        elem = rand() % 100 - 20; 
    }
}


void showArray(const std::vector<int> & Arr){
    for (auto & elem : Arr ){
        std::cout << elem << " "; 
    }
    std::cout << std::endl;
}


