
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <iostream>
#include <thread>
#include <algorithm>
#include <future>
#include <sys/time.h>
#include <chrono>
#include <deque>
#include <mutex>
#include <functional>
//#include <filesystem>
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

std::deque<std::packaged_task<cv::Mat()> > tasks;
std::mutex tasks_mutex;
std::mutex exit_mutex;
bool exitVal = false;

cv::Mat myResize(const char * fName, int widthOut, int heightOut, int numTreads );
cv::Mat myFuturesResize(const char * fName, int widthOut, int heightOut, int numTreads );
void batch_system();

inline long long gettimeus(){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long) tv.tv_sec * 1000000LL + (long long) tv.tv_usec;
}

template<typename func>
std::future<cv::Mat> add_task(func f){
    std::packaged_task<cv::Mat()> task(f);
    std::future<cv::Mat> result =task.get_future();
    std::lock_guard<std::mutex> lock(tasks_mutex);
    tasks.push_back(std::move(task));
    return result;
}

bool is_exit(){
    std::lock_guard<std::mutex> lock(exit_mutex);
    return exitVal;
}

typedef cv::Mat (*TFunc)(const char *, int, int, int );

/*
   run prog from cmd like:
   ./mosse 2  1500 1500   0
           ^    ^    ^    ^
           |    |    |    |-- 0 - threads
           |    |    |    |-- 1 - async 
           |    --H  --W
           --num of threads

*/
int main( int argc, char** argv )
{
    TFunc Funcs[] = {myResize, myFuturesResize};

    fs::path pthImgs("imgs"); // img path
    int w = atoi(argv[3]); // weight
    int h = atoi(argv[2]); // height
    int thrds = atoi(argv[1]); // num of threads you want
    int nFunc = atoi(argv[4]);
    
    // get all file names of imgs  
    std::vector<fs::path> vecFiles;
    std::copy(fs::directory_iterator(pthImgs), 
                fs::directory_iterator(), 
                back_inserter(vecFiles)
             );
    /*
    // show path 
    for(auto it(vecFiles.cbegin()); it != vecFiles.cend(); ++it){
         std::cout<<" "<< it->c_str() << std::endl;
    }
    */

    std::thread executor(batch_system);
    std::vector<std::future<cv::Mat> > vF(vecFiles.size());
    int i =0;
    for(auto it(vecFiles.cbegin()); it != vecFiles.cend(); ++it){
        auto s = std::bind( &(Funcs[nFunc]), it->c_str(), w, h, thrds) ;
        vF[i++] = add_task( std::bind( Funcs[nFunc], it->c_str(), w, h, thrds) );     
    }
    
    // write to file
    for (int i=0; i< vF.size(); ++i){
        std::string fName =  std::string("thumbs/") + std::to_string(i)+ std::string (".jpg");
        cv::imwrite(fName.c_str(), vF[i].get() );
    }
    

    std::unique_lock<std::mutex> lock(exit_mutex);
    exitVal = true;
    lock.unlock();
    executor.join();

    return 0;
}



void batch_system(){

    while(!is_exit()){ // bad!
        std::unique_lock<std::mutex> lock(tasks_mutex);
        if(tasks.empty()) continue;
        std::packaged_task<cv::Mat()> taskq = std::move(tasks.front());
        tasks.pop_front();
        lock.unlock();
        taskq(); // run task

    }
}


// return input or zeros if any errors
cv::Mat myResize(const char * fName, int widthOut, int heightOut, int numTreads ){
    cv::Mat smallerImg;
    // Read the file
    cv::Mat image;
    image = cv::imread(fName, CV_LOAD_IMAGE_COLOR);   

    // Check for invalid input
    if(! image.data ){
        std::cout <<  "Could not open or find the image" << std::endl ;
        return cv::Mat::zeros(2,2,CV_8UC3);
    }
    // Check size of image and out size
    if (image.rows <=  heightOut || image.cols <= widthOut){
        std::cout <<  "Bad out image size." << std::endl ;
        return image;
    }
    // count how many parts of image should to process
    int numWidthParts = image.cols / widthOut;
    int numHeightParts = image.rows /  heightOut;
    // count  threads
    int numCores = std::thread::hardware_concurrency();
    int maxThreads = numWidthParts * numHeightParts;
    int threads = std::min( numCores < numTreads ? numCores:numTreads, maxThreads);
    std::cout <<  "Num threads = " << threads << " are working." << std::endl;

/////////
    // get block width  
    int blockWidthOut = widthOut / threads; 
    int blockWidth = image.cols / threads;

    int blockHeightOut = heightOut / threads; 
    int blockHeight = image.rows / threads;
    //
    long long t_allthead = -gettimeus();

// cut image on parts
// [0*blockWidth  1*blockWidth)
// [1*blockWidth  2*blockWidth)
// [2*blockWidth  3*blockWidth)
    std::vector<cv::Mat> vImgParts(threads);
    std::vector<cv::Mat> vImgOutParts(threads);
    for(int i = 0; i < threads; ++i){
        
        //vImgParts[i] = image(cv::Range(0,image.rows), 
        //                    cv::Range(i*blockWidth, (i+1)* blockWidth)); 
        vImgParts[i] = image(cv::Range(i*blockHeight, (i+1)* blockHeight), 
                            cv::Range(0,image.cols)); 
    }

    // time
    auto t1 = std::chrono::high_resolution_clock::now();

    // resize in threads
    std::vector<std::thread> vTreads(threads-1);
    for(int i=0; i<threads-1; ++i){
        /*
        vTreads[i]= std::thread( [&vImgParts, &vImgOutParts, i, blockWidthOut, heightOut](){
            cv::resize(vImgParts[i],
                        vImgOutParts[i],  
                        cv::Size(blockWidthOut, heightOut) );
        });
        */
        vTreads[i]= std::thread( [&vImgParts, &vImgOutParts, i, widthOut, blockHeightOut](){
            cv::resize(vImgParts[i],
                        vImgOutParts[i],  
                        cv::Size(widthOut, blockHeightOut) );
        });
       // vTreads[i]= std::thread(&(cv::resize), std::cref(vImgParts[i]),
       //                         std::ref(vImgOutParts[i]),  
       //                         cv::Size(blockWidthOut, heightOut) );
    }

    auto t3 = std::chrono::high_resolution_clock::now();
    // resize in main thread
    long long t_mth = - gettimeus();
    /* // for Width blocks
    cv::resize(vImgParts[threads-1], 
                vImgOutParts[threads-1], 
                cv::Size(blockWidthOut, heightOut)
                );
    */
    cv::resize(vImgParts[threads-1], 
                vImgOutParts[threads-1], 
                cv::Size(widthOut, blockHeightOut)
                );
    t_mth += gettimeus();
    std::cout<< "Time. Main thread has worked  = "<< t_mth << " us \n";
    
    // wait threads
    for(int i=0; i<threads-1; ++i){
        vTreads[i].join();
    }

    t_allthead+= gettimeus();
    std::cout << "Time. All threads have worked = " << t_allthead <<" us \n";
    std::chrono::duration<double, std::micro> time_span2 = t3 - t1;
    std::cout << "Time of threads creating: " << time_span2.count() <<" us"<< std::endl;
      
    long long tconc = -gettimeus();
    // concatenate Mat
     //cv::hconcat(vImgOutParts, smallerImg);
     cv::vconcat(vImgOutParts, smallerImg);
     tconc += gettimeus();
     std::cout << "Time of concatenate = " << tconc <<" us \n";

    return smallerImg;
}



cv::Mat myFuturesResize(const char * fName, int widthOut, int heightOut, int numTreads )
{
    cv::Mat smallerImg;
    // Read the file
    cv::Mat image;
    image = cv::imread(fName, CV_LOAD_IMAGE_COLOR);   

    // Check for invalid input
    if(! image.data ){
        std::cout <<  "Could not open or find the image" << std::endl ;
        return cv::Mat::zeros(2,2,CV_8UC3);
    }
    // Check size of image and out size
    if (image.rows <=  heightOut || image.cols <= widthOut){
        std::cout <<  "Bad out image size." << std::endl ;
        return image;
    }
    // count how many parts of image should to process
    int numWidthParts = image.cols / widthOut;
    int numHeightParts = image.rows /  heightOut;
    
    std::cout <<  "Num threads = " << numTreads << std::endl;

    int blockWidthOut = widthOut / numTreads; 
    int blockWidth = image.cols / numTreads;

    int blockHeightOut = heightOut / numTreads; 
    int blockHeight = image.rows / numTreads;

    long long t_allthead = -gettimeus();

    // cut image on parts
    // [0*blockWidth  1*blockWidth)
    // [1*blockWidth  2*blockWidth)
    // [2*blockWidth  3*blockWidth)
    std::vector<cv::Mat> vImgParts(numTreads);
    std::vector<cv::Mat> vImgOutParts(numTreads);
    for(int i = 0; i < numTreads; ++i){ 
        vImgParts[i] = image(cv::Range(i*blockHeight, (i+1)* blockHeight), 
                            cv::Range(0,image.cols)); 
    }
   
     // time
    auto t1 = std::chrono::high_resolution_clock::now();

    // resize in threads
    std::vector<std::future<void>> vTreads(numTreads-1);
    for(int i=0; i<numTreads-1; ++i){
        
        vTreads[i] = std::async( std::launch::async, [&vImgParts, &vImgOutParts, i, widthOut, blockHeightOut](){
            cv::resize(vImgParts[i],
                        vImgOutParts[i],  
                        cv::Size(widthOut, blockHeightOut) );
        });
    }

    auto t3 = std::chrono::high_resolution_clock::now();
    // resize in main thread
    long long t_mth = - gettimeus();
    cv::resize(vImgParts[numTreads-1], 
                vImgOutParts[numTreads-1], 
                cv::Size(widthOut, blockHeightOut)
                );
                
    t_mth += gettimeus();
    std::cout<< "Time main thread = "<< t_mth << " us\n";
    
    // wait threads
    for(int i=0; i<numTreads-1; ++i){
        vTreads[i].get();
    }

    t_allthead+= gettimeus();
    std::cout << "Time. All threads have worked = " << t_allthead <<" us\n";

    std::chrono::duration<double, std::micro> time_span2 = t3 - t1;
    std::cout << "Time. Thread creating: " << time_span2.count() <<" us"<< std::endl;


    long long tconc = -gettimeus(); 
     cv::vconcat(vImgOutParts, smallerImg);
     tconc += gettimeus();
     std::cout << "Time of concatenating = " << tconc <<" us\n";
  
    return smallerImg;
}
