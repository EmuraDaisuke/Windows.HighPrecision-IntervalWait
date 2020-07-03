


#include <chrono>
#include <iostream>
#include <iomanip>

#include <windows.h>
#include <winuser.h>
#include <conio.h>

#include "./IntervalWait.h"

#pragma comment(lib, "user32.lib")



double Now()
{
    using namespace std::chrono;
    return static_cast<double>(duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count()) / 1000000000;
}



int main(int argc, char* argv[])
{
    Mmcss m;
    
//  IntervalWait Interval(/*ms*/1 * /*us*/1000 * /*ns*/1000);
//  IntervalWait Interval(/*us*/1000 * /*ns*/1000);
    IntervalWait Interval(3333333);
    
    double vOld = Now();
    double vNow = Now();
    double vSum = 0;
    int nSum = 0;
    
    while (!(GetAsyncKeyState(VK_ESCAPE) < 0)){
        Interval.Wait();
        
        vOld = vNow;
        vNow = Now();
        vSum += vNow - vOld;
        ++nSum;
        
        std::cout << std::fixed << std::setprecision(9) << (vNow - vOld) << " " << (vSum / nSum) << std::endl;
    }
    
    return 0;
}
