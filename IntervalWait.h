#pragma once



#include <cstddef>
#include <atomic>

#include <windows.h>
#include <dwmapi.h>
#include <avrt.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "avrt.lib")



class Mmcss {
    public:
        ~Mmcss() noexcept                   { if (--snInstance == 0) DwmEnableMMCSS(FALSE); }
        
        Mmcss()                             { if (snInstance++ == 0) DwmEnableMMCSS(TRUE); }
        Mmcss(const Mmcss&)                 = delete;
        Mmcss(Mmcss&&)                      = delete;
        
        Mmcss& operator =(const Mmcss&)     = delete;
        Mmcss& operator =(Mmcss&&)          = delete;
    
    
    private:
        static inline std::atomic_uint32_t snInstance = 0;
};



class IntervalWait {
    public:
        enum class eLimiter {
            Average,
            Minimum,
        };
    
    
    
    public:
        ~IntervalWait() noexcept
        {
            if (mhShutdown) SetEvent(mhShutdown);
            if (mhThread) WaitForSingleObject(mhThread, INFINITE);
            
            if (mhThread) CloseHandle(mhThread);
            if (mhShutdown) CloseHandle(mhShutdown);
            if (mhWait) CloseHandle(mhWait);
        }
        
        
        
        IntervalWait(int64_t nsec, eLimiter Limiter = eLimiter::Average)
        :mInterval(nsec)
        ,mLimiter((Limiter == eLimiter::Average)? 0:-1)
        ,mhWait(CreateEvent(NULL, FALSE, TRUE, NULL))
        ,mhShutdown(CreateEvent(NULL, FALSE, FALSE, NULL))
        ,mhThread(CreateThread(NULL, 0, Thread, this, 0, NULL))
        {
        }
        
        
        
        IntervalWait(IntervalWait&& v)
        :mInterval(v.mInterval)
        ,mLimiter(v.mLimiter)
        ,mhWait(v.mhWait)
        ,mhShutdown(v.mhShutdown)
        ,mhThread(v.mhThread)
        {
            v.mInterval = 0;
            v.mLimiter = 0;
            v.mhWait = NULL;
            v.mhShutdown = NULL;
            v.mhThread = NULL;
        }
        
        
        
        IntervalWait& operator =(IntervalWait&& v)
        {
            new(this) IntervalWait(std::move(v)); return *this;
        }
        
        
        
        void Wait() const noexcept
        {
            WaitForSingleObject(mhWait, INFINITE);
        }
    
    
    
    public:
        IntervalWait(const IntervalWait&)               = delete;
        
        IntervalWait& operator =(const IntervalWait&)   = delete;
    
    
    
    private:
        DWORD Thread() const noexcept
        {
            HMODULE hDll = NULL;
            HANDLE hRevert = NULL;
            HANDLE hTimer = NULL;
            
            using ProcNtQueryTimerResolution = LONG(_stdcall*)(PULONG maximum, PULONG minimum, PULONG current);
            using ProcNtSetTimerResolution = LONG(_stdcall*)(ULONG resolution, BOOL set, PULONG current);
            ProcNtQueryTimerResolution NtQueryTimerResolution = nullptr;
            ProcNtSetTimerResolution NtSetTimerResolution = nullptr;
            
            ULONG Maximum = 0;
            ULONG Minimum = 0;
            ULONG Current = 0;
            
            do {
                hDll = LoadLibrary("ntdll.dll");
                if (!hDll) break;
                
                NtQueryTimerResolution = (ProcNtQueryTimerResolution) GetProcAddress(hDll, "NtQueryTimerResolution");
                NtSetTimerResolution = (ProcNtSetTimerResolution) GetProcAddress(hDll, "NtSetTimerResolution");
                if (!NtQueryTimerResolution) break;
                if (!NtSetTimerResolution) break;
                
                NtQueryTimerResolution(&Maximum, &Minimum, &Current);
                NtSetTimerResolution(Minimum, TRUE, &Current);
                
                DWORD TaskIndex = 0;
                hRevert = AvSetMmThreadCharacteristics("PRO Audio", &TaskIndex);
                hTimer = CreateWaitableTimer(NULL, TRUE, NULL);
                if (!hRevert) break;
                if (!hTimer) break;
                
                LARGE_INTEGER Frequency;
                LARGE_INTEGER Counter;
                QueryPerformanceFrequency(&Frequency);
                QueryPerformanceCounter(&Counter);
                
                int64_t Precision = (/*sec*/1ll * /*msec*/1000 * /*usec*/1000 * /*nsec*/1000) / Frequency.QuadPart;
                int64_t Absolute = Counter.QuadPart * Precision;
                int64_t Relative = 0;
                
                while (WaitForSingleObject(mhShutdown, 0) != WAIT_OBJECT_0){
                    Counter.QuadPart = (mInterval + Relative) / -Precision;
                    Counter.QuadPart = (Counter.QuadPart < 0)? Counter.QuadPart: mLimiter;
                    if (Counter.QuadPart){
                        SetWaitableTimer(hTimer, &Counter, 0, NULL, NULL, FALSE);
                        WaitForSingleObject(hTimer, INFINITE);
                    }
                    
                    SetEvent(mhWait);
                    
                    QueryPerformanceCounter(&Counter);
                    Absolute += mInterval;
                    Relative = Absolute - (Counter.QuadPart * Precision);
                }
            } while (false);
            
            if (hTimer) CloseHandle(hTimer);
            if (hRevert) AvRevertMmThreadCharacteristics(hRevert);
            
            if (NtSetTimerResolution) NtSetTimerResolution(0, FALSE, &Current);
            
            if (hDll) FreeLibrary(hDll);
            return 0;
        }
        
        
        
        static DWORD Thread(void* pVoid) noexcept
        {
            auto pThis = reinterpret_cast<IntervalWait*>(pVoid);
            return pThis->Thread();
        }
    
    
    
    private:
        int64_t mInterval;
        LONGLONG mLimiter;
        
        HANDLE mhWait;
        HANDLE mhShutdown;
        HANDLE mhThread;
};
