#pragma once
#include "aura/workspace.h"
#include <windows.h>
#include <pdh.h>
#include <mutex>
#include <vector>

namespace aura {
// Windows-only sampler. Timer callbacks use the OS thread pool, not the UI thread.
class SystemSampler {
public:
    ~SystemSampler();
    void Start(HWND receiver,UINT message);
    void Stop();
    SystemSnapshot Read();
private:
    static void CALLBACK Tick(PTP_CALLBACK_INSTANCE,void*,PTP_TIMER);
    void Sample();
    PTP_TIMER timer_{};
    HWND receiver_{};
    UINT message_{};
    PDH_HQUERY query_{};
    PDH_HCOUNTER gpu_{};
    bool gpuAttempted_=false,cpuPrimed_=false;
    unsigned long long idle_=0,kernel_=0,user_=0;
    std::mutex sampleMutex_,valueMutex_;
    SystemSnapshot value_;
    std::vector<unsigned char> gpuBuffer_;
};
}
