#include "common.h"

namespace hx {
namespace {

std::mutex g_logMutex;
FILE* g_logFile = nullptr;

}  // namespace

void logf(const char* fmt, ...) {
    char body[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(body, sizeof(body), fmt, ap);
    va_end(ap);

    SYSTEMTIME st;
    GetLocalTime(&st);

    std::lock_guard<std::mutex> lock(g_logMutex);
    std::printf("[%02d:%02d:%02d.%03d] %s\n", st.wHour, st.wMinute, st.wSecond,
                st.wMilliseconds, body);
    std::fflush(stdout);

    if (g_logFile) {
        std::fprintf(g_logFile, "[%02d:%02d:%02d.%03d] %s\n", st.wHour, st.wMinute, st.wSecond,
                     st.wMilliseconds, body);
        std::fflush(g_logFile);
    }
}

bool logOpenFile(const char* path) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (g_logFile) std::fclose(g_logFile);
    g_logFile = std::fopen(path, "w");
    return g_logFile != nullptr;
}

void logCloseFile() {
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (g_logFile) {
        std::fclose(g_logFile);
        g_logFile = nullptr;
    }
}

void sleepMs(int ms) {
    if (ms > 0) ::Sleep(static_cast<DWORD>(ms));
}

std::wstring widen(const std::string& s) {
    if (s.empty()) return std::wstring();
    const int need =
        ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(need), L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(), need);
    return out;
}

std::string narrow(const std::wstring& s) {
    if (s.empty()) return std::string();
    const int need = ::WideCharToMultiByte(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()),
                                           nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(need), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(), need,
                          nullptr, nullptr);
    return out;
}

}  // namespace hx
