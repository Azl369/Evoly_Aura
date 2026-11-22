#include "aura/system_sampler.h"
#include <pdhmsg.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <string>
#include <string_view>

namespace aura {
namespace {
unsigned long long TimeValue(FILETIME value) {
    return (static_cast<unsigned long long>(value.dwHighDateTime)<<32)|value.dwLowDateTime;
}
}
SystemSampler::~SystemSampler() { Stop(); }
void SystemSampler::Start(HWND receiver,UINT message) {
    if (timer_) return;
    receiver_=receiver; message_=message;
    timer_=CreateThreadpoolTimer(Tick,this,nullptr);
    if (!timer_) return;
    FILETIME due{};
    SetThreadpoolTimer(timer_,&due,2000,100);
}
void SystemSampler::Stop() {
    if (timer_) {
        SetThreadpoolTimer(timer_,nullptr,0,0);
        WaitForThreadpoolTimerCallbacks(timer_,TRUE);
        CloseThreadpoolTimer(timer_); timer_=nullptr;
    }
    if (query_) PdhCloseQuery(query_);
    query_=nullptr; gpu_=nullptr; gpuAttempted_=false; cpuPrimed_=false;
    std::vector<unsigned char>().swap(gpuBuffer_);
    std::lock_guard lock(valueMutex_); value_={};
}
SystemSnapshot SystemSampler::Read() { std::lock_guard lock(valueMutex_); return value_; }
void CALLBACK SystemSampler::Tick(PTP_CALLBACK_INSTANCE,void* context,PTP_TIMER) {
    auto& sampler=*static_cast<SystemSampler*>(context);
    std::unique_lock lock(sampler.sampleMutex_,std::try_to_lock);
    if (!lock.owns_lock()) return;
    try { sampler.Sample(); } catch (...) {
        std::lock_guard valueLock(sampler.valueMutex_);
        sampler.value_={};
    }
    PostMessageW(sampler.receiver_,sampler.message_,0,0);
}
void SystemSampler::Sample() {
    SystemSnapshot next;
    FILETIME idle{},kernel{},user{};
    if (GetSystemTimes(&idle,&kernel,&user)) {
        const auto i=TimeValue(idle),k=TimeValue(kernel),u=TimeValue(user);
        if (cpuPrimed_ && k>=kernel_ && u>=user_ && i>=idle_) {
            const auto total=(k-kernel_)+(u-user_),idleDelta=i-idle_;
            if (total && idleDelta<=total) {
                next.cpuValid=true;
                next.cpuPercent=100.0*double(total-idleDelta)/double(total);
            }
        }
        idle_=i; kernel_=k; user_=u; cpuPrimed_=true;
    }
    MEMORYSTATUSEX memory{sizeof(memory)};
    if (GlobalMemoryStatusEx(&memory)) {
        next.memoryValid=true; next.totalMemory=memory.ullTotalPhys;
        next.availableMemory=std::min(memory.ullAvailPhys,memory.ullTotalPhys);
    }
    if (!gpuAttempted_) {
        gpuAttempted_=true;
        if (PdhOpenQueryW(nullptr,0,&query_)==ERROR_SUCCESS) {
            if (PdhAddEnglishCounterW(query_,L"\\GPU Engine(*)\\Utilization Percentage",0,&gpu_)!=ERROR_SUCCESS) {
                PdhCloseQuery(query_); query_=nullptr; gpu_=nullptr;
            }
        }
    }
    if (gpu_ && PdhCollectQueryData(query_)==ERROR_SUCCESS) {
        DWORD bytes=0,count=0;
        constexpr DWORD format=PDH_FMT_DOUBLE|PDH_FMT_NOCAP100;
        if (PdhGetFormattedCounterArrayW(gpu_,format,&bytes,&count,nullptr)==PDH_MORE_DATA && bytes<=1024*1024) {
            gpuBuffer_.resize(bytes);
            auto* items=reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(gpuBuffer_.data());
            if (PdhGetFormattedCounterArrayW(gpu_,format,&bytes,&count,items)==ERROR_SUCCESS) {
                std::map<std::wstring,double> engines;
                for (DWORD n=0;n<count;++n) {
                    const auto& item=items[n];
                    if (item.FmtValue.CStatus!=PDH_CSTATUS_VALID_DATA && item.FmtValue.CStatus!=PDH_CSTATUS_NEW_DATA) continue;
                    const double percent=item.FmtValue.doubleValue;
                    if (!item.szName || !std::isfinite(percent) || percent<0) continue;
                    std::wstring_view name(item.szName);
                    const auto begin=name.find(L"luid_"),end=name.find(L"_engtype_");
                    if (begin==name.npos || end==name.npos || end<=begin) continue;
                    // Sum processes sharing one physical engine, then choose the
                    // busiest engine across adapters. Do not sum different engines.
                    engines[std::wstring(name.substr(begin,end-begin))]+=percent;
                }
                for (const auto& [engine,percent]:engines) {
                    (void)engine; next.gpuValid=true;
                    next.gpuPercent=std::max(next.gpuPercent,std::clamp(percent,0.0,100.0));
                }
            }
        }
    }
    std::lock_guard lock(valueMutex_); value_=next;
}
}
