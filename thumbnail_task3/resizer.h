#ifndef  _RESIZER_H_
#define  _RESIZER_H_  

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <exception>

class ErrorsResizer : public std::exception
{
    std::string m_msg;
public:
    explicit ErrorsResizer(const std::string& msg ) : m_msg(msg){}
    virtual const char* what () const noexcept;
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
    Resizer (const std::string & nameInImg, int widthOut, int heightOut, int numTreads);
    ~Resizer();

    void readImg ();
    void writeImg () const;

    void resize();

    // RESIZE by async
    void resizeAsync();
    // this func works with parts of rows
    void resizingTask (int numberImgPart );

private:
    void countThreads();
};


#endif