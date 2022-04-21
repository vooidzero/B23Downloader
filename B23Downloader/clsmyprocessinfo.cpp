#include "clsmyprocessinfo.h"

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

//class clsmyprocessinfo
//{
//    public:
//        clsmyprocessinfo();
//        ~clsmyprocessinfo();
//        int getcurprocessid();
////        int  getcurprocessid()
////        {
////            int pid = (int)getpid();
////    //        std::cout<<"The process id is: "<<iPid<<std::endl;
////            return pid;
////        }

//};
clsmyprocessinfo::clsmyprocessinfo()
        {

        }
clsmyprocessinfo::~clsmyprocessinfo()
        {

        }

int clsmyprocessinfo::getcurprocessid()
{
    int pid = (int)getpid();
    return pid;
}
char* clsmyprocessinfo::getcurprocessidstr()
{
    int pid=getcurprocessid();
    std::string pidstr=std::to_string(pid);
    char* pidchar=strtochar(pidstr);
    return pidchar;
}
char* clsmyprocessinfo::strtochar(std::string str)
{
    int charsize=str.length()+1;
    char* mychar=new char[charsize+1];
    strncpy(mychar,str.c_str(),charsize);
    return mychar;
}
char* clsmyprocessinfo::mystrcat(char* char1,char* char2)
{
    int char1size0,char1size1,char2size,copysize;
    char1size0=strlen(char1);
    char2size=strlen(char2);
    copysize=char2size-1-char1size0;
    strncat(char1,char2,copysize);
    return char1;
}
//clsmyprocessinfo::myprocessinfo() : data(new myprocessinfoData)
//{

//}

//clsmyprocessinfo::myprocessinfo(const myprocessinfo &rhs)
//    : data{rhs.data}
//{

//}

//clsmyprocessinfo &myprocessinfo::operator=(const myprocessinfo &rhs)
//{
//    if (this != &rhs)
//        data.operator=(rhs.data);
//    return *this;
//}

//clsmyprocessinfo::~myprocessinfo()
//{

//}
