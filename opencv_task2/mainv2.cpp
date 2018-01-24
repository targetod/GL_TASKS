
/*
*  Using other 
* cl
*/
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <iostream>
#include <thread>
#include <algorithm>
#include <future>
#include <sys/time.h>
#include <chrono>
#include <exception>

class ErrorsResizer : public std::exception
{
    std::string m_msg;
public:
    explicit ErrorsResizer(const std::string& msg ) : m_msg(msg){}
    virtual const char* what () const noexcept{
        return m_msg.c_str();
    }
};

class Resizer
{

    std::string m_nameInImg;
    std::string m_nameOutImg;
   
    int m_widthOut, m_heightOut;
    int m_numTreads; 

    cv::Mat m_inImage;
    cv::Mat m_outImage; // smaller
public:
    Resizer (const std::string & nameInImg, int widthOut, int heightOut, int numTreads) 
        : m_nameInImg(nameInImg),
            m_nameOutImg("small_" + m_nameInImg),
            m_widthOut (widthOut), 
            m_heightOut (heightOut), 
            m_numTreads (numTreads),
            m_outImage (m_heightOut, m_widthOut, CV_8UC3)
    {
        
    }
    ~Resizer(){}

    void readImg () {
        m_inImage = cv::imread(m_nameInImg, CV_LOAD_IMAGE_COLOR);   
        //std:: cout << m_inImage.type() << std::endl;
        // Check for invalid input
        if(! m_inImage.data ){
            throw (ErrorsResizer("Could not open or find the image\n"));
        }

         // Check size of image and out size
        if (m_inImage.rows <=  m_heightOut || m_inImage.cols <= m_widthOut){
            throw (ErrorsResizer("Size of img is small\n"));
        }
    }
    void writeImg (){
        cv::imwrite(m_nameOutImg, m_outImage);
    }

    void resize(){
        countThreads();

        // time
        auto t1 = std::chrono::high_resolution_clock::now();

        int imgParts = m_numTreads;
         // resize in threads
        std::vector<std::thread> vTreads(m_numTreads-1);

        for(int i=0; i < imgParts-1; ++i){   
            vTreads[i]= std::thread( &Resizer::resizingTask, this, i);
        }

        resizingTask (imgParts-1);
        // wait threads
        for(int i=0; i<m_numTreads-1; ++i){
            vTreads[i].join();
        }

        auto t2 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::micro> time_span = t2 - t1;
        std::cout << "Time of resizing: " << time_span.count() <<" us"<< std::endl;
    }

    // this func working with parts of rows
    void resizingTask (int numberImgPart ){
        // find  coord and size of img inPart
        int imgParts = m_numTreads;
        int deltaRow = m_inImage.cols / imgParts; // expl 250

        int y = 0;
        int heigthIn = m_inImage.rows;

        int x =  numberImgPart * deltaRow ;  // 0 * 250,  1 * 250   
        int widthIn = numberImgPart+1 != imgParts ? deltaRow :  // 250 
                            m_inImage.cols - numberImgPart * deltaRow; // 752 - 2*250 = 252
       
        cv::Mat inPart = m_inImage( cv::Rect(x, y, widthIn, heigthIn) );
        cv::Mat outPart;

        // find size of outPart img
        int heightOut =  m_outImage.rows;
        int deltaRowOut = m_outImage.cols / imgParts; 
        int widthOut = numberImgPart+1 != imgParts ? deltaRowOut :  // 250 
                            m_outImage.cols - numberImgPart * deltaRowOut; // 752 - 2*250 = 252

        cv::resize(inPart, outPart, cv::Size(widthOut, heightOut) );

        // set part into out Img
        // find position x, y 
        int yOut = 0;
        int xOut = numberImgPart * deltaRowOut ;
        outPart.copyTo(m_outImage(cv::Rect(xOut, yOut, widthOut, heightOut)));
    }

private:

    void countThreads(){
        // count how many parts of image should to process
        int numWidthParts = m_inImage.cols / m_widthOut;
        int numHeightParts = m_inImage.rows / m_heightOut;
        // count  threads
        int numCores = std::thread::hardware_concurrency();
        int maxThreads = numWidthParts * numHeightParts;
        m_numTreads = std::min( numCores < m_numTreads ? numCores: m_numTreads, maxThreads);
        //std::cout <<  "Num threads = " << threads << " are working." << std::endl;
    }
    
};


/*
   run prog from cmd like:
   ./mosse 2 1500 1500 0
   0 - threads
   1 - async 
*/
int main( int argc, char** argv )
{
    int w = atoi(argv[3]); // weight
    int h = atoi(argv[2]); // height
    int thrds = atoi(argv[1]); // num of threads you want
    int nFunc = atoi(argv[4]); // 

    try {
        Resizer rszizer("evan.jpg", w,h,thrds); 
        rszizer.readImg();
        rszizer.resize();
        rszizer.writeImg();
    }
    catch (const ErrorsResizer &  err){
        std::cout << err.what()  << "\n";
    }
    catch (const std::exception &  err){
        std::cout << err.what()  << "\n";
    }


    return 0;
}

