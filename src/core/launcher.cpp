#include "aura/launcher.h"
#include <algorithm>

namespace aura {
std::vector<LauncherEntry> DefaultLaunchers() {
    const wchar_t* names[]{L"文件",L"下载",L"文档",L"项目主页",L"系统设置",L"回收站"};
    std::vector<LauncherEntry> entries;
    for(int i=0;i<6;++i) entries.push_back({L"builtin-"+std::to_wstring(i),names[i],L"",i});
    return entries;
}
bool ValidLauncher(const LauncherEntry& entry) {
    auto clean=[](const std::wstring& value,size_t max) {
        return value.size()<=max && std::none_of(value.begin(),value.end(),[](wchar_t c){return c<32;});
    };
    return !entry.id.empty() && !entry.name.empty() && clean(entry.id,64) && clean(entry.name,128) &&
        clean(entry.target,4096) && entry.builtin>=-1 && entry.builtin<6 &&
        (entry.builtin>=0 || !entry.target.empty());
}
size_t LauncherPages(size_t count) { return std::max(size_t(1),(count+launcherPageSize-1)/launcherPageSize); }
}
