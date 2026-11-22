#include "aura/launcher_icons.h"
#include <shlobj.h>
#include <wrl/client.h>
#include <algorithm>
#include <cwctype>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

namespace aura {
namespace {
uint64_t Hash(std::wstring_view value,uint64_t seed=14695981039346656037ull) {
    for(wchar_t c:value) { seed^=uint16_t(std::towlower(c));seed*=1099511628211ull; }
    return seed;
}
bool LocalFile(const std::wstring& path) {
    if(path.size()<3 || path[1]!=L':' || (path[2]!=L'\\' && path[2]!=L'/')) return false;
    wchar_t root[]{path[0],L':',L'\\',0};
    return GetDriveTypeW(root)==DRIVE_FIXED;
}
uint64_t Stamp(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if(!GetFileAttributesExW(path.c_str(),GetFileExInfoStandard,&data)) return 0;
    return (uint64_t(data.ftLastWriteTime.dwHighDateTime)<<32)^data.ftLastWriteTime.dwLowDateTime^
        (uint64_t(data.nFileSizeHigh)<<32)^data.nFileSizeLow;
}
std::wstring IconSource(const std::wstring& source,int& index) {
    std::wstring file=source;
    auto extension=std::filesystem::path(source).extension().wstring();
    std::transform(extension.begin(),extension.end(),extension.begin(),[](wchar_t c){return wchar_t(std::towlower(c));});
    wchar_t path[4096]{};
    if(extension==L".lnk") {
        Microsoft::WRL::ComPtr<IShellLinkW> link;
        Microsoft::WRL::ComPtr<IPersistFile> persist;
        if(FAILED(CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&link))) ||
           FAILED(link.As(&persist)) || FAILED(persist->Load(source.c_str(),STGM_READ))) return {};
        link->GetIconLocation(path,4096,&index);
        if(!path[0]) { index=0;link->GetPath(path,4096,nullptr,SLGP_RAWPATH); }
        file=path;
    } else if(extension==L".url") {
        GetPrivateProfileStringW(L"InternetShortcut",L"IconFile",L"",path,4096,source.c_str());
        index=int(GetPrivateProfileIntW(L"InternetShortcut",L"IconIndex",0,source.c_str()));file=path;
    }
    if(file.empty()) return {};
    const auto count=ExpandEnvironmentStringsW(file.c_str(),path,4096);
    if(!count || count>4096) return {};
    file=path;
    return LocalFile(file)?file:std::wstring{};
}
std::shared_ptr<const Image> Extract(const std::wstring& file,int index,unsigned size) {
    HICON icon=nullptr;
    const HRESULT result=SHDefExtractIconW(file.c_str(),index,0,&icon,nullptr,MAKELONG(size,0));
    struct IconGuard { HICON value; ~IconGuard(){DestroyIcon(value);} } iconGuard{icon};
    if(result!=S_OK || !icon) return {};
    HDC dc=CreateCompatibleDC(nullptr);
    if(!dc) return {};
    BITMAPINFO info{};info.bmiHeader={sizeof(BITMAPINFOHEADER),LONG(size),-LONG(size),1,32,BI_RGB};
    void* bits=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);
    if(!bitmap) {DeleteDC(dc);return {};}
    auto old=SelectObject(dc,bitmap);
    struct BitmapGuard { HDC dc;HBITMAP bitmap;HGDIOBJ old;~BitmapGuard(){SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);} } bitmapGuard{dc,bitmap,old};
    auto paint=[&](uint32_t background) {
        auto* pixels=static_cast<uint32_t*>(bits);std::fill(pixels,pixels+size*size,background);
        DrawIconEx(dc,0,0,icon,int(size),int(size),0,nullptr,DI_NORMAL);GdiFlush();
        return std::vector<uint32_t>(pixels,pixels+size*size);
    };
    // Black/white compositing also recovers alpha for legacy mask-only icons.
    auto black=paint(0),white=paint(0x00ffffff);
    auto image=std::make_shared<Image>();image->width=size;image->height=size;image->pixels.resize(size*size);
    for(size_t i=0;i<black.size();++i) {
        unsigned difference=0;
        for(unsigned shift: {0u,8u,16u}) {
            const int delta=int((white[i]>>shift)&255)-int((black[i]>>shift)&255);
            difference=std::max(difference,unsigned(std::clamp(delta,0,255)));
        }
        unsigned alpha=255-difference;
        image->pixels[i]=(alpha<<24)|
            (std::min((black[i]>>16)&255,alpha)<<16)|
            (std::min((black[i]>>8)&255,alpha)<<8)|std::min(black[i]&255,alpha);
    }
    return image;
}
bool OwnedCacheFile(const std::filesystem::path& path) {
    const auto name=path.filename().wstring();
    return name.size()==21 && name.substr(16)==L".rgba" &&
        std::all_of(name.begin(),name.begin()+16,[](wchar_t c){return std::iswxdigit(c)!=0;});
}
}
LauncherIcons::~LauncherIcons(){Stop();}
void LauncherIcons::Start(HWND receiver,UINT message,std::filesystem::path directory) {
    receiver_=receiver;message_=message;directory_=std::move(directory);
    work_=CreateThreadpoolWork(Work,this,nullptr);
}
void LauncherIcons::Resume(){std::lock_guard lock(mutex_);active_=true;}
void LauncherIcons::Pause(){
    std::lock_guard lock(mutex_);active_=false;++generation_;
    jobs_.clear();pending_.clear();cache_.clear();bytes_=0;
}
void LauncherIcons::Refresh(){
    std::lock_guard lock(mutex_);++generation_;jobs_.clear();pending_.clear();cache_.clear();bytes_=0;
    clearDisk_=true;
    if(work_ && !running_){running_=true;SubmitThreadpoolWork(work_);}
}
void LauncherIcons::Stop(){
    {std::lock_guard lock(mutex_);stopping_=true;active_=false;++generation_;jobs_.clear();pending_.clear();}
    if(work_){WaitForThreadpoolWorkCallbacks(work_,TRUE);CloseThreadpoolWork(work_);work_=nullptr;}
    std::lock_guard lock(mutex_);cache_.clear();bytes_=0;running_=false;
}
std::shared_ptr<const Image> LauncherIcons::Get(const std::wstring& source,unsigned pixels) {
    pixels=std::clamp(pixels,16u,128u);
    const uint64_t key=Hash(source)^uint64_t(pixels);
    std::lock_guard lock(mutex_);
    if(!active_ || stopping_ || !work_) return {};
    auto found=std::find_if(cache_.begin(),cache_.end(),[&](const auto& item){return item.key==key;});
    if(found!=cache_.end()){cache_.splice(cache_.begin(),cache_,found);return cache_.front().image;}
    auto token=std::make_pair(generation_,key);
    if(!pending_.contains(token)) {
        while(jobs_.size()>=6){pending_.erase({jobs_.front().generation,jobs_.front().key});jobs_.pop_front();}
        jobs_.push_back({source,pixels,key,generation_});pending_.insert(token);
        if(!running_){running_=true;SubmitThreadpoolWork(work_);}
    }
    return {};
}
LauncherIcons::Stats LauncherIcons::ReadStats(){std::lock_guard lock(mutex_);return {bytes_,cache_.size(),jobs_.size()};}
void CALLBACK LauncherIcons::Work(PTP_CALLBACK_INSTANCE,void* context,PTP_WORK){static_cast<LauncherIcons*>(context)->Run();}
void LauncherIcons::Run(){
    const HRESULT com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    struct ComGuard {bool initialized;~ComGuard(){if(initialized)CoUninitialize();}} guard{SUCCEEDED(com)};
    for(;;){
        Job job;bool clear=false;
        {std::lock_guard lock(mutex_);
         if(stopping_){running_=false;return;}
         clear=clearDisk_;clearDisk_=false;
         if(!clear && jobs_.empty()){running_=false;return;}
         if(!clear){job=std::move(jobs_.front());jobs_.pop_front();}}
        if(clear){try{Prune(true);}catch(...){}continue;}
        std::shared_ptr<const Image> image;
        try{image=Load(job);}catch(...){}
        bool notify=false;
        {std::lock_guard lock(mutex_);pending_.erase({job.generation,job.key});
         if(active_ && !stopping_ && job.generation==generation_){
            const size_t cost=image?image->pixels.size()*4:0;
            while(!cache_.empty() && (bytes_+cost>memoryLimit || cache_.size()>=entryLimit)) {
                if(cache_.back().image)bytes_-=cache_.back().image->pixels.size()*4;
                cache_.pop_back();
            }
            bytes_+=cost;cache_.push_front({job.key,std::move(image)});notify=true;
         }}
        if(notify)PostMessageW(receiver_,message_,0,0);
    }
}
void LauncherIcons::Prune(bool all){
    std::error_code error;
    if(!std::filesystem::exists(directory_,error))return;
    std::vector<std::filesystem::directory_entry> files;
    uintmax_t bytes=0;
    for(const auto& entry:std::filesystem::directory_iterator(directory_)) {
        if(entry.is_regular_file() && OwnedCacheFile(entry.path())){files.push_back(entry);bytes+=entry.file_size();}
    }
    std::sort(files.begin(),files.end(),[](const auto& a,const auto& b){return a.last_write_time()<b.last_write_time();});
    for(size_t i=0;i<files.size();++i) {
        if(!all && files.size()-i<=128 && bytes<=16*1024*1024)break;
        const auto size=files[i].file_size();
        if(DeleteFileW(files[i].path().c_str()))bytes-=size;
    }
}
std::shared_ptr<const Image> LauncherIcons::Load(const Job& job){
    if(!LocalFile(job.source))return {};
    int index=0;auto source=IconSource(job.source,index);
    if(source.empty())return {};
    auto fingerprint=(Hash(source,job.key)^Stamp(job.source))*1099511628211ull;
    fingerprint=(fingerprint^Stamp(source))*1099511628211ull^uint64_t(uint32_t(index));
    std::wostringstream name;name<<std::hex<<std::setw(16)<<std::setfill(L'0')<<fingerprint<<L".rgba";
    std::filesystem::create_directories(directory_);
    const auto file=directory_/name.str();
    {std::ifstream input(file,std::ios::binary);uint32_t header[3]{};
     if(input.read(reinterpret_cast<char*>(header),sizeof(header)) && header[0]==0x4149434f && header[1]==job.pixels && header[2]==job.pixels &&
        std::filesystem::file_size(file)==sizeof(header)+size_t(job.pixels)*job.pixels*4){
         auto image=std::make_shared<Image>();image->width=job.pixels;image->height=job.pixels;image->pixels.resize(size_t(job.pixels)*job.pixels);
         if(input.read(reinterpret_cast<char*>(image->pixels.data()),std::streamsize(image->pixels.size()*4)))return image;
     }}
    auto image=Extract(source,index,job.pixels);
    if(!image)return {};
    {auto temp=file;temp+=L".tmp";std::ofstream output(temp,std::ios::binary|std::ios::trunc);
     const uint32_t header[]{0x4149434f,job.pixels,job.pixels};
     output.write(reinterpret_cast<const char*>(header),sizeof(header));
     output.write(reinterpret_cast<const char*>(image->pixels.data()),std::streamsize(image->pixels.size()*4));output.close();
     if(output)MoveFileExW(temp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING);
    }
    Prune(false);return image;
}
}
