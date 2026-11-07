#pragma once
#include <cstdio>
#include <string>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
inline std::wstring utf16(const std::string& s) {
    if(s.empty())return std::wstring();
    int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),int(s.size()),nullptr,0);
    if(!n)throw std::runtime_error("Invalid UTF-8 filename");
    std::wstring w(size_t(n),L'\0');MultiByteToWideChar(CP_UTF8,0,s.data(),int(s.size()),&w[0],n);return w;
}
inline std::string utf8(const std::wstring& w) {
    if(w.empty())return std::string();
    int n=WideCharToMultiByte(CP_UTF8,0,w.data(),int(w.size()),nullptr,0,nullptr,nullptr);
    std::string s(size_t(n),'\0');WideCharToMultiByte(CP_UTF8,0,w.data(),int(w.size()),&s[0],n,nullptr,nullptr);return s;
}
inline FILE* openFile(const std::string& path,const char* mode) {return _wfopen(utf16(path).c_str(),utf16(mode).c_str());}
inline void removeFile(const std::string& path) {_wremove(utf16(path).c_str());}
#else
inline FILE* openFile(const std::string& path,const char* mode) {return std::fopen(path.c_str(),mode);}
inline void removeFile(const std::string& path) {std::remove(path.c_str());}
#endif
