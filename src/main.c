#include <windows.h>
#include <wtsapi32.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
#include "shared.h"

SERVICE_STATUS g_ServiceStatus;
SERVICE_STATUS_HANDLE g_StatusHandle;
HANDLE g_ServiceStopEvent = NULL;

char g_ProcessNameToCheck[MAX_PATH];
char g_ProcessPathToRun[MAX_PATH];
char g_ProcessNameToRun[MAX_PATH];

FILE* logger = NULL;

void log_init() {
    #if 0
    logger = fopen("D:\\rpip_service_log.txt", "a+");
    if (logger) {
        fprintf(logger, "=== Service started ===\n");
        fflush(logger);
    }
    #endif
}

void log_msg(const char* fmt, ...) {
    #if 0
    if (!logger) return;
    va_list args;
    va_start(args, fmt);
    vfprintf(logger, fmt, args);
    fprintf(logger, "\n");
    fflush(logger);
    va_end(args);
    #endif
}

int is_process_running_by_name(const char* name) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32 entry = { .dwSize = sizeof(PROCESSENTRY32) };

    if (!Process32First(snapshot, &entry)) {
        CloseHandle(snapshot);
        return 0;
    }

    do {
        if (_stricmp(entry.szExeFile, name) == 0) {
            CloseHandle(snapshot);
            return 1;
        }
    } while (Process32Next(snapshot, &entry));

    CloseHandle(snapshot);
    return 0;
}

int run_process(const char* path) {
    DWORD sessionId = WTSGetActiveConsoleSessionId();
    HANDLE userToken = NULL;

    if (!WTSQueryUserToken(sessionId, &userToken)) {
        log_msg("WTSQueryUserToken failed with error: %lu", GetLastError());
        return 1;
    }

    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi;
    si.cb = sizeof(STARTUPINFOA);
    si.lpDesktop = "winsta0\\default";  // necessary for GUI apps

    char cmdLine[MAX_PATH * 2];
    snprintf(cmdLine, sizeof(cmdLine), "\"%s\"", path);

    BOOL result = CreateProcessAsUserA(
        userToken,
        NULL,
        cmdLine,
        NULL,
        NULL,
        FALSE,
        CREATE_NEW_CONSOLE,
        NULL,
        NULL,
        &si,
        &pi
    );

    CloseHandle(userToken);

    if (!result) {
        DWORD err = GetLastError();
        log_msg("CreateProcessAsUser failed with error: %lu", err);
        return err;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    log_msg("Process started successfully as user.");
    return 0;
}

DWORD WINAPI ServiceWorkerThread(LPVOID lpParam) {
    bool launched = false;

    while (WaitForSingleObject(g_ServiceStopEvent, 0) != WAIT_OBJECT_0) {
        if (is_process_running_by_name(g_ProcessNameToCheck) != 1) {
            log_msg("Check process not running: %s", g_ProcessNameToCheck);
            launched = false;
            Sleep(2000);
            continue;
        }

        if (is_process_running_by_name(g_ProcessNameToRun) == 1) {
            log_msg("Run target already running: %s", g_ProcessNameToRun);
            launched = true;
            Sleep(2000);
            continue;
        }

        if (!launched && run_process(g_ProcessPathToRun) == 0) {
            launched = true;
        }

        Sleep(2000);
    }

    return 0;
}

void WINAPI ServiceCtrlHandler(DWORD ctrlCode) {
    switch (ctrlCode) {
        case SERVICE_CONTROL_STOP:
            if (g_ServiceStatus.dwCurrentState != SERVICE_RUNNING)
                return;
            g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
            SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
            SetEvent(g_ServiceStopEvent);
            break;
        default:
            break;
    }
}

void WINAPI ServiceMain(DWORD argc, LPTSTR* argv) {
    log_init();

    g_StatusHandle = RegisterServiceCtrlHandler("rpip", ServiceCtrlHandler);
    if (!g_StatusHandle) return;

    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

    g_ServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!g_ServiceStopEvent) {
        g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        g_ServiceStatus.dwWin32ExitCode = GetLastError();
        SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
        return;
    }

    LPSTR cmdLine = GetCommandLineA();
    char exe[MAX_PATH];
    sscanf(cmdLine, "\"%[^\"]\" %s \"%[^\"]\" %s", exe, g_ProcessNameToCheck, g_ProcessPathToRun, g_ProcessNameToRun);
    log_msg("Parsed args: check=%s, path=%s, run=%s", g_ProcessNameToCheck, g_ProcessPathToRun, g_ProcessNameToRun);

    if (strlen(g_ProcessNameToCheck) == 0 || strlen(g_ProcessPathToRun) == 0 || strlen(g_ProcessNameToRun) == 0) {
        log_msg("Invalid parsed arguments.");
        g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        g_ServiceStatus.dwWin32ExitCode = ERROR_INVALID_PARAMETER;
        SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
        return;
    }

    g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

    HANDLE hThread = CreateThread(NULL, 0, ServiceWorkerThread, NULL, 0, NULL);
    WaitForSingleObject(hThread, INFINITE);
    CloseHandle(g_ServiceStopEvent);

    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
}

int main() {
    SERVICE_TABLE_ENTRY ServiceTable[] = {
        { SERVICE_NAME, (LPSERVICE_MAIN_FUNCTION)ServiceMain },
        { NULL, NULL }
    };
    StartServiceCtrlDispatcher(ServiceTable);
    return 0;
}