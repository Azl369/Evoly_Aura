#include <windows.h>
#include <shellapi.h>
#include <filesystem>
#include <fstream>
int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int){
    int count=0;auto args=CommandLineToArgvW(GetCommandLineW(),&count);
    if(!args)return 1;
    if(count==2){std::ofstream marker(std::filesystem::path(args[1]),std::ios::binary);marker<<"launched";}
    LocalFree(args);return 0;
}
