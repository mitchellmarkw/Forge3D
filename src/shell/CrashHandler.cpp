// SPDX-License-Identifier: GPL-3.0-or-later
#include "CrashHandler.h"

#ifdef _WIN32

#include <QDateTime>
#include <QDir>

#include <windows.h>
// dbghelp.h must come after windows.h
#include <dbghelp.h>

#include <string>

namespace forge::app {

namespace {
std::wstring g_dumpDir;

LONG WINAPI writeMiniDump(EXCEPTION_POINTERS* info)
{
    SYSTEMTIME now;
    GetLocalTime(&now);
    wchar_t name[64];
    swprintf_s(name, L"\\forge3d-crash-%04d%02d%02d-%02d%02d%02d.dmp", now.wYear, now.wMonth,
               now.wDay, now.wHour, now.wMinute, now.wSecond);
    const std::wstring path = g_dumpDir + name;

    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION exceptionInfo{};
        exceptionInfo.ThreadId = GetCurrentThreadId();
        exceptionInfo.ExceptionPointers = info;
        exceptionInfo.ClientPointers = FALSE;
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file,
                          static_cast<MINIDUMP_TYPE>(MiniDumpWithIndirectlyReferencedMemory |
                                                     MiniDumpWithThreadInfo),
                          &exceptionInfo, nullptr, nullptr);
        CloseHandle(file);
    }
    return EXCEPTION_CONTINUE_SEARCH;  // let Windows show its normal crash dialog
}
}  // namespace

void installCrashHandler(const QString& dumpDirectory)
{
    QDir().mkpath(dumpDirectory);
    g_dumpDir = QDir::toNativeSeparators(dumpDirectory).toStdWString();
    SetUnhandledExceptionFilter(writeMiniDump);
}

}  // namespace forge::app

#else

namespace forge::app {
void installCrashHandler(const QString&) {}
}  // namespace forge::app

#endif
