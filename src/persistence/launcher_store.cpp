#include "aura/launcher.h"
#include <windows.h>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>

namespace aura {
namespace {
std::string Utf8(const std::wstring& value) {
    int size=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),int(value.size()),nullptr,0,nullptr,nullptr);
    if(!size && !value.empty()) throw std::runtime_error("Invalid shortcut text");
    std::string bytes(size_t(size),'\0');
    if(size) WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),int(value.size()),bytes.data(),size,nullptr,nullptr);
    return bytes;
}
std::wstring Wide(const std::string& bytes) {
    int size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,bytes.data(),int(bytes.size()),nullptr,0);
    if(!size && !bytes.empty()) throw std::runtime_error("Invalid UTF-8 shortcut text");
    std::wstring value(size_t(size),L'\0');
    if(size) MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,bytes.data(),int(bytes.size()),value.data(),size);
    return value;
}
}
LauncherStore::LauncherStore(std::filesystem::path directory):file_(std::move(directory)/L"launchers.txt") {}
std::vector<LauncherEntry> LauncherStore::Load() const {
    if(!std::filesystem::exists(file_)) return DefaultLaunchers();
    if(std::filesystem::file_size(file_)>1024*1024) throw std::runtime_error("Shortcut file exceeds limit");
    std::ifstream input(file_,std::ios::binary);
    std::string magic; int version=0;
    if(!(input>>magic>>version) || magic!="AURA_LAUNCHERS" || version!=1) throw std::runtime_error("Unsupported shortcut file");
    std::vector<LauncherEntry> entries;
    std::set<std::wstring> ids;
    for(;;) {
        input>>std::ws;
        if(input.peek()==EOF) break;
        std::string id,name,target; int builtin=-1;
        if(entries.size()>=launcherEntryLimit || !(input>>std::quoted(id)>>std::quoted(name)>>std::quoted(target)>>builtin))
            throw std::runtime_error("Invalid shortcut file");
        LauncherEntry entry{Wide(id),Wide(name),Wide(target),builtin};
        if(!ValidLauncher(entry) || !ids.insert(entry.id).second) throw std::runtime_error("Invalid shortcut entry");
        entries.push_back(std::move(entry));
    }
    return entries;
}
void LauncherStore::Save(const std::vector<LauncherEntry>& entries) const {
    if(entries.size()>launcherEntryLimit) throw std::runtime_error("Shortcut limit reached");
    std::ostringstream out; out<<"AURA_LAUNCHERS 1\n";
    std::set<std::wstring> ids;
    for(const auto& entry:entries) {
        if(!ValidLauncher(entry) || !ids.insert(entry.id).second) throw std::runtime_error("Invalid shortcut entry");
        out<<std::quoted(Utf8(entry.id))<<' '<<std::quoted(Utf8(entry.name))<<' '<<std::quoted(Utf8(entry.target))<<' '<<entry.builtin<<'\n';
    }
    auto bytes=out.str();
    if(bytes.size()>1024*1024) throw std::runtime_error("Shortcut file exceeds limit");
    std::filesystem::create_directories(file_.parent_path());
    auto temp=file_; temp+=L".tmp";
    { std::ofstream output(temp,std::ios::binary|std::ios::trunc);
      output.write(bytes.data(),std::streamsize(bytes.size()));output.flush();
      if(!output) throw std::runtime_error("Cannot save shortcut file"); }
    if(!MoveFileExW(temp.c_str(),file_.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Cannot replace shortcut file");
}
}
