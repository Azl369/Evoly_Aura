#pragma once
#include "aura/image.h"
#include <windows.h>
#include <deque>
#include <filesystem>
#include <list>
#include <memory>
#include <mutex>
#include <set>
#include <string>

namespace aura {
class LauncherIcons {
public:
    static constexpr size_t memoryLimit=3*1024*1024,entryLimit=96;
    struct Stats { size_t bytes{},entries{},queued{}; };
    ~LauncherIcons();
    void Start(HWND receiver,UINT message,std::filesystem::path directory);
    void Resume();
    void Pause();
    void Refresh();
    void Stop();
    std::shared_ptr<const Image> Get(const std::wstring& source,unsigned pixels);
    Stats ReadStats();
private:
    struct Job { std::wstring source; unsigned pixels{}; uint64_t key{},generation{}; };
    struct Cached { uint64_t key{}; std::shared_ptr<const Image> image; };
    static void CALLBACK Work(PTP_CALLBACK_INSTANCE,void*,PTP_WORK);
    void Run();
    std::shared_ptr<const Image> Load(const Job& job);
    void Prune(bool all);
    PTP_WORK work_{};
    HWND receiver_{}; UINT message_{};
    std::filesystem::path directory_;
    std::mutex mutex_;
    bool active_=false,running_=false,stopping_=false,clearDisk_=false;
    uint64_t generation_=0;
    size_t bytes_=0;
    std::deque<Job> jobs_;
    std::set<std::pair<uint64_t,uint64_t>> pending_;
    std::list<Cached> cache_;
};
}
