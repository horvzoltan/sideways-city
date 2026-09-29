#include "process.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
bool LaunchDetached(const std::string& exe, std::string* error) {
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::string cmd = "\"" + exe + "\"";
    std::string dir = exe.substr(0, exe.find_last_of("\\/"));
    if (!CreateProcessA(nullptr, &cmd[0], nullptr, nullptr, FALSE, DETACHED_PROCESS, nullptr,
                        dir.c_str(), &si, &pi)) {
        if (error) *error = "CreateProcess failed (" + std::to_string(GetLastError()) + ")";
        return false;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}
#else
#include <unistd.h>
#include <sys/types.h>
bool LaunchDetached(const std::string& exe, std::string* error) {
    if (access(exe.c_str(), X_OK) != 0) { if (error) *error = "not found: " + exe; return false; }
    pid_t pid = fork();
    if (pid < 0) { if (error) *error = "fork failed"; return false; }
    if (pid == 0) {
        setsid();
        execl(exe.c_str(), exe.c_str(), (char*)nullptr);
        _exit(127);
    }
    return true;
}
#endif
