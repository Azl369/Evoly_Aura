#include "aura/workspace.h"
#include <windows.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <iomanip>
#include <cmath>

namespace aura {
namespace {
void AtomicWrite(const std::filesystem::path& path, const std::string& bytes) {
    auto temporary = path; temporary += L".tmp";
    {
        std::ofstream file(temporary,std::ios::binary|std::ios::trunc);
        if (!file) throw std::runtime_error("Cannot open workspace file");
        file.write(bytes.data(),std::streamsize(bytes.size()));
        file.flush();
        if (!file) throw std::runtime_error("Cannot save workspace file");
    }
    if (!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Cannot replace workspace file");
}
}
WorkspaceStore::WorkspaceStore(std::filesystem::path directory) : directory_(std::move(directory)) {
    directory_=std::filesystem::absolute(directory_).lexically_normal();
    std::filesystem::create_directories(directory_);
}
void WorkspaceStore::Load(std::vector<WidgetPlacement>& widgets,int& theme,float& opacity,int& tasks) {
    auto file=directory_/L"workspace.ini";
    theme=std::clamp(int(GetPrivateProfileIntW(L"appearance",L"theme",0,file.c_str())),0,3);
    opacity=std::clamp(int(GetPrivateProfileIntW(L"appearance",L"opacity",54,file.c_str())),30,85)/100.f;
    tasks=int(GetPrivateProfileIntW(L"workspace",L"tasks",0,file.c_str()))&7;
    for (size_t i=0;i<widgets.size();++i) {
        auto section=L"panel"+std::to_wstring(i);
        auto& p=widgets[i];
        auto integer=[&](const wchar_t* key,int fallback) {
            return int(GetPrivateProfileIntW(section.c_str(),key,fallback,file.c_str()));
        };
        auto number=[&](const wchar_t* key,float fallback) {
            wchar_t value[64]{};
            auto defaultValue=std::to_wstring(fallback);
            GetPrivateProfileStringW(section.c_str(),key,defaultValue.c_str(),value,64,file.c_str());
            wchar_t* end=nullptr;
            float result=std::wcstof(value,&end);
            return end!=value && *end==L'\0' && std::isfinite(result)?result:fallback;
        };
        auto min=MinimumWidgetSize(p.kind);
        p.bounds.x=std::clamp(number(L"x",p.bounds.x),-16000.f,16000.f);
        p.bounds.y=std::clamp(number(L"y",p.bounds.y),-16000.f,16000.f);
        p.bounds.width=std::clamp(number(L"width",p.bounds.width),min.width,1800.f);
        p.bounds.height=std::clamp(number(L"height",p.bounds.height),min.height,1000.f);
        if (p.kind==WidgetKind::toolbar) p.bounds.height=76;
        p.pinned=integer(L"pinned",0)!=0;
    }
}
void WorkspaceStore::Save(const std::vector<WidgetPlacement>& widgets,int theme,float opacity,int tasks) {
    std::ostringstream out;
    out<<std::fixed<<std::setprecision(3);
    out<<"[workspace]\nschema=1\ntasks="<<(tasks&7)<<"\n[appearance]\ntheme="<<theme<<"\nopacity="<<int(opacity*100+.5f)<<"\n";
    for (size_t i=0;i<widgets.size();++i) {
        const auto& p=widgets[i];
        out<<"[panel"<<i<<"]\nx="<<p.bounds.x<<"\ny="<<p.bounds.y
            <<"\nwidth="<<p.bounds.width<<"\nheight="<<p.bounds.height<<"\npinned="<<int(p.pinned)<<"\n";
    }
    AtomicWrite(directory_/L"workspace.ini",out.str());
    WritePrivateProfileStringW(nullptr,nullptr,nullptr,(directory_/L"workspace.ini").c_str());
}
std::wstring WorkspaceStore::LoadNote() const {
    auto path=directory_/L"note.txt";
    if (!std::filesystem::exists(path)) return L"把想法留在这里。\n双击便签，或点击右上角「编辑」。";
    if (std::filesystem::file_size(path)>32768) return L"便签文件超出当前预览的大小限制。";
    std::ifstream file(path,std::ios::binary);
    std::string bytes((std::istreambuf_iterator<char>(file)),{});
    int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,bytes.data(),int(bytes.size()),nullptr,0);
    if (!count && !bytes.empty()) return L"便签文件不是有效的 UTF-8 文本。";
    std::wstring note(size_t(count),L'\0');
    if (count) MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,bytes.data(),int(bytes.size()),note.data(),count);
    return note;
}
void WorkspaceStore::SaveNote(const std::wstring& note) {
    if (note.size()>8192) throw std::runtime_error("Note exceeds 8192 characters");
    int count=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,note.data(),int(note.size()),nullptr,0,nullptr,nullptr);
    if (!count && !note.empty()) throw std::runtime_error("Invalid note text");
    std::string bytes(size_t(count),'\0');
    if (count) WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,note.data(),int(note.size()),bytes.data(),count,nullptr,nullptr);
    AtomicWrite(directory_/L"note.txt",bytes);
}
}
