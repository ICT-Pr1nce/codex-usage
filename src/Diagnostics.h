#pragma once
#include <windows.h>
#include <filesystem>
#include "Quota.h"
inline std::filesystem::path DiagnosticPath(){
    wchar_t b[32768]{};DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",b,32768);
    if(!n || n>=32768)return {};
    return std::filesystem::path(b)/L"TrafficMonitor/CodexQuota.log";
}
inline std::string Utf8(const wchar_t* text){
    int n=WideCharToMultiByte(CP_UTF8,0,text,-1,nullptr,0,nullptr,nullptr);if(n<=0)return {};
    std::string out(n,'\0');WideCharToMultiByte(CP_UTF8,0,text,-1,out.data(),n,nullptr,nullptr);out.resize(n-1);return out;
}
// Never log raw RPC messages, server error bodies, tokens, account IDs or environment values.
inline void Diagnostic(const char* event,Json fields=Json::object()) noexcept {
    DWORD saved=GetLastError();
    try{
        auto path=DiagnosticPath();if(path.empty())return;
        std::error_code ec;std::filesystem::create_directories(path.parent_path(),ec);if(ec)return;
        auto size=std::filesystem::file_size(path,ec);
        if(!ec && size>1024*1024){auto previous=path;previous+=L".1";MoveFileExW(path.c_str(),previous.c_str(),MOVEFILE_REPLACE_EXISTING);}
        SYSTEMTIME now{};GetSystemTime(&now);char date[64]{};
        sprintf_s(date,"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,now.wMilliseconds);
        fields["time_utc"]=date;fields["event"]=event;fields["version"]="1.2.1";fields["pid"]=GetCurrentProcessId();
        auto line=fields.dump()+"\n";
        HANDLE file=CreateFileW(path.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file!=INVALID_HANDLE_VALUE){DWORD count=0;WriteFile(file,line.data(),static_cast<DWORD>(line.size()),&count,nullptr);CloseHandle(file);}
    }catch(...){}
    SetLastError(saved);
}
inline Json RpcErrorSummary(const Json& error){
    Json summary=Json::object();
    if(!error.is_object()){summary["category"]="malformed_error";return summary;}
    auto code=error.find("code");if(code!=error.end() && code->is_number_integer())summary["rpc_code"]=*code;
    std::string message;auto text=error.find("message");if(text!=error.end() && text->is_string())message=text->get<std::string>();
    for(auto& c:message)if(c>='A' && c<='Z')c=static_cast<char>(c-'A'+'a');
    Json tags=Json::array();
    for(auto tag:{"401","403","407","429","500","502","503","504","timeout","timed out","proxy","connect","dns","certificate","auth","token","refresh","rate limit","not logged","not supported","attestation"})
        if(message.find(tag)!=std::string::npos)tags.push_back(tag);
    summary["error_tags"]=tags;summary["message_length"]=message.size();return summary;
}
