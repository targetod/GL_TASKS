
/*
*  Using other 
* cl
*/
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <iostream>
#include <thread>
#include <future>
#include <deque>
#include <atomic>
#include <mutex>
#include <functional>
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

#include "resizer.h"


class Executor
{
    std::atomic_bool done;
    std::deque<std::packaged_task<void()> > tasks;
    std::mutex tasks_mutex;
    std::thread worker;

    void work_tread(){
        while(!done){
            std::unique_lock<std::mutex> lock(tasks_mutex);
            if(tasks.empty()){
                lock.unlock();
                std::this_thread::yield();
            }
            else{
                std::packaged_task<void()> taskq = std::move(tasks.front());
                tasks.pop_front();
                lock.unlock();
                taskq(); // run task
            }
        }
    }

public:

    Executor() : done(false), worker (&Executor::work_tread, this)
    {
    }
    ~Executor(){
        worker.join();// wait worker
    }
    void exit(){
       done = true; 
    }

    template <typename Func>
    std::future<void> add_task(Func f){
        std::packaged_task<void()> task(f);
        std::future<void> result =task.get_future();
        std::lock_guard<std::mutex> lock(tasks_mutex);
        tasks.push_back(std::move(task));
        return result;
    }

};


/*
   run prog from cmd like:
   ./exec 2 1500 1500 0
   0 - threads
   1 - async 
*/
int main( int argc, char** argv )
{
    int w = atoi(argv[3]); // width
    int h = atoi(argv[2]); // height
    int thrds = atoi(argv[1]); // num of threads you want
    int nFunc = atoi(argv[4]); // 0 - threads  , 1 - async

    Executor execut;

    fs::path pthImgs("imgs"); // img path
        // get all file names of imgs  
    std::vector<fs::path> vecFiles;
    std::copy(fs::directory_iterator(pthImgs), 
                fs::directory_iterator(), 
                back_inserter(vecFiles)
             );
             
    std::vector<std::future<void> > vF(vecFiles.size());
    int i =0;
    for(auto it(vecFiles.cbegin()); it != vecFiles.cend(); ++it){
       
        vF[i++] = execut.add_task( [=] () { 
            try { 
                Resizer rszizer( it->c_str(), w, h, thrds); 
                rszizer.readImg();
                if ( 0 == nFunc ) rszizer.resize();
                else rszizer.resizeAsync();
                rszizer.writeImg();
            } 
            catch (const std::exception &  err){
                std::cout << err.what()  << "\n";
                throw;
            }
         }
        );     
        
    }

    try {
        for (int i=0; i< vF.size(); ++i){
            vF[i].get();
        }
    }
    catch (const std::exception &  err){
        std::cout << err.what()  << "\n";
    }

    execut.exit();


    return 0;
}

