#include <windows.h>
#include <psapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <tlhelp32.h>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <iomanip>
#include <stdexcept>
#include <algorithm>

struct Memory {
    long long privateWs = -1, privateBytes = -1, ws = -1, handles = -1, gdi = -1, user = -1;
    unsigned long long cpu = 0;
};
struct Process {
    HANDLE handle{};
    std::vector<BYTE> pages = std::vector<BYTE>(65536);
    explicit Process(DWORD pid) {
        if (pid) {
            handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
            if (!handle) handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        }
    }
    ~Process() { if (handle) CloseHandle(handle); }
    Memory Read() {
        Memory out;
        if (!handle) return out;
        PROCESS_MEMORY_COUNTERS_EX counters{};
        counters.cb = sizeof(counters);
        if (GetProcessMemoryInfo(handle, reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters))) {
            out.privateBytes = static_cast<long long>(counters.PrivateUsage);
            out.ws = static_cast<long long>(counters.WorkingSetSize);
        }
        bool available = false;
        for (int retry = 0; retry < 5; ++retry) {
            if (QueryWorkingSet(handle, pages.data(), DWORD(pages.size()))) { available = true; break; }
            if (GetLastError() != ERROR_BAD_LENGTH || pages.size() >= 8 * 1024 * 1024) break;
            pages.resize(pages.size() * 2);
        }
        if (available) {
            SYSTEM_INFO info{}; GetSystemInfo(&info);
            auto* ws = reinterpret_cast<PSAPI_WORKING_SET_INFORMATION*>(pages.data());
            unsigned long long privatePages = 0;
            for (ULONG_PTR i = 0; i < ws->NumberOfEntries; ++i)
                if (!ws->WorkingSetInfo[i].Shared) ++privatePages;
            out.privateWs = static_cast<long long>(privatePages * info.dwPageSize);
        }
        DWORD handles{};
        if (GetProcessHandleCount(handle, &handles)) out.handles = handles;
        out.gdi = GetGuiResources(handle, GR_GDIOBJECTS);
        out.user = GetGuiResources(handle, GR_USEROBJECTS);
        FILETIME created{}, exited{}, kernel{}, user{};
        if (GetProcessTimes(handle, &created, &exited, &kernel, &user)) {
            ULARGE_INTEGER k{}, u{};
            k.LowPart = kernel.dwLowDateTime; k.HighPart = kernel.dwHighDateTime;
            u.LowPart = user.dwLowDateTime; u.HighPart = user.dwHighDateTime;
            out.cpu = k.QuadPart + u.QuadPart;
        }
        return out;
    }
};
struct Gpu {
    PDH_HQUERY query{};
    PDH_HCOUNTER dedicated{}, shared{};
    Gpu() {
        if (PdhOpenQueryW(nullptr, 0, &query) != ERROR_SUCCESS) return;
        if (PdhAddEnglishCounterW(query, L"\\GPU Process Memory(*)\\Dedicated Usage", 0, &dedicated) != ERROR_SUCCESS)
            dedicated = nullptr;
        if (PdhAddEnglishCounterW(query, L"\\GPU Process Memory(*)\\Shared Usage", 0, &shared) != ERROR_SUCCESS)
            shared = nullptr;
        PdhCollectQueryData(query);
    }
    ~Gpu() { if (query) PdhCloseQuery(query); }
    long long Sum(PDH_HCOUNTER counter, DWORD pid) {
        if (!counter || !pid) return -1;
        DWORD bytes = 0, count = 0;
        if (PdhGetFormattedCounterArrayW(counter, PDH_FMT_LARGE, &bytes, &count, nullptr) != PDH_MORE_DATA) return -1;
        std::vector<BYTE> buffer(bytes);
        auto* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buffer.data());
        if (PdhGetFormattedCounterArrayW(counter, PDH_FMT_LARGE, &bytes, &count, items) != ERROR_SUCCESS) return -1;
        std::wstring prefix = L"pid_" + std::to_wstring(pid) + L"_";
        long long sum = 0;
        for (DWORD i = 0; i < count; ++i)
            if (std::wstring(items[i].szName).starts_with(prefix) &&
                (items[i].FmtValue.CStatus == PDH_CSTATUS_VALID_DATA || items[i].FmtValue.CStatus == PDH_CSTATUS_NEW_DATA))
                sum += items[i].FmtValue.largeValue;
        return sum;
    }
};
DWORD FindDwm() {
    DWORD session{};
    ProcessIdToSessionId(GetCurrentProcessId(), &session);
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W entry{sizeof(entry)};
    DWORD found = 0;
    if (Process32FirstW(snapshot, &entry)) do {
        DWORD processSession{};
        if (_wcsicmp(entry.szExeFile, L"dwm.exe") == 0 &&
            ProcessIdToSessionId(entry.th32ProcessID, &processSession) && processSession == session) {
            found = entry.th32ProcessID; break;
        }
    } while (Process32NextW(snapshot, &entry));
    CloseHandle(snapshot); return found;
}
int wmain(int argc, wchar_t** argv) {
    try {
        DWORD pid = 0, seconds = 30, interval = 1000, dwmOverride = 0;
        std::wstring output;
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (i + 1 >= argc) throw std::invalid_argument("Missing value");
            std::wstring value = argv[++i];
            if (arg == L"--pid") pid = std::stoul(value);
            else if (arg == L"--dwm-pid") dwmOverride = std::stoul(value);
            else if (arg == L"--seconds") seconds = std::stoul(value);
            else if (arg == L"--interval-ms") interval = std::stoul(value);
            else if (arg == L"--output") output = value;
            else throw std::invalid_argument("Unknown metrics option");
        }
        if (output.empty() || seconds > 86400 || !seconds || interval < 100 || interval > 60000)
            throw std::invalid_argument("Invalid bounded measurement arguments");
        std::ofstream file{std::filesystem::path(output)};
        if (!file) throw std::runtime_error("Cannot open metrics output");
        Process process(pid);
        if (pid && !process.handle) throw std::runtime_error("Cannot open target process");
        DWORD dwmPid = dwmOverride ? dwmOverride : FindDwm();
        Process dwm(dwmPid);
        Gpu gpu;
        file << "elapsed_s,pid,private_ws_bytes,private_bytes,working_set_bytes,cpu_percent,handles,gdi,user,"
            "gpu_dedicated_bytes,gpu_shared_bytes,dwm_private_bytes,dwm_working_set_bytes,"
            "dwm_gpu_dedicated_bytes,dwm_gpu_shared_bytes,system_commit_bytes\n";
        auto start = std::chrono::steady_clock::now(), previous = start;
        auto prior = process.Read();
        DWORD processors = std::max(DWORD(1), GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
        unsigned samples = (seconds * 1000 + interval - 1) / interval;
        for (unsigned i = 0; i < samples; ++i) {
            std::this_thread::sleep_until(start + std::chrono::milliseconds(uint64_t(i + 1) * interval));
            DWORD exitCode{};
            if (pid && (!GetExitCodeProcess(process.handle, &exitCode) || exitCode != STILL_ACTIVE))
                throw std::runtime_error("Target exited before measurement completed");
            auto now = std::chrono::steady_clock::now();
            auto memory = process.Read(), dm = dwm.Read();
            double delta = std::chrono::duration<double>(now - previous).count();
            double cpu = !pid ? -1 : (memory.cpu - prior.cpu) / 10000000. / delta / processors * 100;
            if (gpu.query) PdhCollectQueryData(gpu.query);
            PERFORMANCE_INFORMATION system{sizeof(system)};
            GetPerformanceInfo(&system, sizeof(system));
            file << std::fixed << std::setprecision(4) << std::chrono::duration<double>(now - start).count() << ','
                << pid << ',' << memory.privateWs << ',' << memory.privateBytes << ',' << memory.ws << ',' << cpu << ','
                << memory.handles << ',' << memory.gdi << ',' << memory.user << ','
                << gpu.Sum(gpu.dedicated, pid) << ',' << gpu.Sum(gpu.shared, pid) << ','
                << dm.privateBytes << ',' << dm.ws << ',' << gpu.Sum(gpu.dedicated, dwmPid) << ','
                << gpu.Sum(gpu.shared, dwmPid) << ','
                << uint64_t(system.CommitTotal) * system.PageSize << '\n';
            file.flush();
            previous = now; prior = memory;
        }
        std::cout << "Recorded " << samples << " samples; DWM pid=" << dwmPid
            << ". -1 indicates unavailable counters; GPU counts sum adapter instances.\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
