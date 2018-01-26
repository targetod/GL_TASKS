
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <iostream>
#include <thread>
#include <algorithm>
#include <future>
#include <sys/time.h>
#include <chrono>

cv::Mat myResize(const char * fName, int widthOut, int heightOut, int numTreads );
cv::Mat myFuturesResize(const char * fName, int widthOut, int heightOut, int numTreads );


inline long long gettimeus(){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long) tv.tv_sec * 1000000LL + (long long) tv.tv_usec;
}

typedef cv::Mat (*TFunc)(const char *, int, int, int );
/*
   run prog from cmd like:
   ./mosse 2 1500 1500 0
   0 - threads
   1 - async 
*/
int main( int argc, char** argv )
{
    TFunc Funcs[] = {myResize, myFuturesResize};

    const char * pFileName = "evan.jpg"; // img name
    int w = atoi(argv[3]); // weight
    int h = atoi(argv[2]); // height
    int thrds = atoi(argv[1]); // num of threads you want
    int nFunc = atoi(argv[4]);
    // profiler
    long long t_fullTask = -gettimeus();
    
    // choose threads
    //auto smallerImg = myResize (pFileName, w, h, thrds);
    // or future + async
    auto smallerImg = Funcs[nFunc] (pFileName, w, h, thrds);
   
    t_fullTask += gettimeus();
    std::cout << " Full task time = " << t_fullTask << "us\n"; 
    
    // img write
    // For JPEG, it can be a quality ( CV_IMWRITE_JPEG_QUALITY ) 
    // from 0 to 100 (the higher is the better). Default value is 95.
    cv::imwrite("outSmall.jpg", smallerImg);

    //namedWindow( "Display window", WINDOW_AUTOSIZE );// Create a window for display.
    //imshow( "Display window", smallerImg );          // Show our image inside it.
    //waitKey(0);                                     // Wait for a keystroke in the window
    return 0;
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
