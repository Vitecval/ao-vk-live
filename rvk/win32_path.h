#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <limits>
#include <string>
#include <string_view>

namespace rvk::win32 {
// Cache paths are UTF-8 at the renderer boundary; Windows owns UTF-16 paths.
inline std::string Utf8(std::wstring_view value) {
    if(value.empty()||value.size()>size_t((std::numeric_limits<int>::max)()))return {};
    const int n=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),int(value.size()),nullptr,0,nullptr,nullptr);
    if(!n)return {};
    std::string out(size_t(n),'\0');
    if(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,value.data(),int(value.size()),out.data(),n,nullptr,nullptr)!=n)return {};
    return out;
}
inline std::wstring Wide(std::string_view value) {
    if(value.empty()||value.size()>size_t((std::numeric_limits<int>::max)()))return {};
    const int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),nullptr,0);
    if(!n)return {};
    std::wstring out(size_t(n),L'\0');
    if(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,value.data(),int(value.size()),out.data(),n)!=n)return {};
    return out;
}
inline std::string EnvironmentUtf8(const wchar_t* name) {
    wchar_t value[32768]{};
    const DWORD n=GetEnvironmentVariableW(name,value,DWORD(std::size(value)));
    return n&&n<std::size(value) ? Utf8(std::wstring_view(value,n)):std::string{};
}
inline std::wstring FilePath(std::string_view value) {
    std::wstring path=Wide(value);
    if(path.find(L'\0')!=std::wstring::npos)return {};
    if(path.size()<248||path.rfind(L"\\\\?\\",0)==0||path.rfind(L"\\\\.\\",0)==0)return path;
    std::replace(path.begin(),path.end(),L'/',L'\\');
    const bool drive=path.size()>2&&path[1]==L':'&&path[2]==L'\\';
    const bool unc=path.rfind(L"\\\\",0)==0;
    if(!drive&&!unc)return path;
    wchar_t absolute[32768]{};
    const DWORD n=GetFullPathNameW(path.c_str(),DWORD(std::size(absolute)),absolute,nullptr);
    if(!n||n>=std::size(absolute))return {};
    path.assign(absolute,n);
    return unc ? L"\\\\?\\UNC\\"+path.substr(2):L"\\\\?\\"+path;
}
inline FILE* OpenFile(std::string_view path,const wchar_t* mode) {
    const auto file=FilePath(path);
    if(file.empty()){errno=EINVAL;return nullptr;}
    return _wfopen(file.c_str(),mode);
}
}
