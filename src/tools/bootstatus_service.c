// bootstatus_service.c - windows startup service for efi boot status webhook
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <winhttp.h>
#include <stdio.h>

#pragma comment(lib, "winhttp.lib")

typedef struct ELF_BOOT_STATUS {
    unsigned long long BootTimestamp;
    int HvEnabled;
    int HvInitialized;
    int VirtualizationSupported;
    int EptSupported;
    int ConfigLoaded;
    unsigned int HvVersion;
} ELF_BOOT_STATUS;

static const GUID g_elf_boot_status_guid =
    { 0x4c7aaf1d, 0x8f1f, 0x4a0b, {0x9e,0x34,0x52,0x8e,0x21,0x46,0x12,0xa9} };

static const wchar_t* g_status_var = L"elf_boot_status";
static const wchar_t* g_webhook_path = L"C:\\ProgramData\\elfhv\\bootstatus_webhook.txt";

static int enable_system_env_priv(void) {
    HANDLE token = NULL;
    TOKEN_PRIVILEGES tp;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token)) return 0;
    LUID luid;
    if (!LookupPrivilegeValueW(NULL, SE_SYSTEM_ENVIRONMENT_NAME, &luid)) { CloseHandle(token); return 0; }
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    AdjustTokenPrivileges(token, FALSE, &tp, sizeof(tp), NULL, NULL);
    CloseHandle(token);
    return (GetLastError() == ERROR_SUCCESS);
}

static int read_file_utf8(const wchar_t* path, char* out, size_t out_sz) {
    if (!out || out_sz == 0) return 0;
    FILE* f = _wfopen(path, L"rb");
    if (!f) return 0;
    size_t n = fread(out, 1, out_sz - 1, f);
    fclose(f);
    out[n] = '\0';
    // trim trailing whitespace
    while (n && (out[n - 1] == '\r' || out[n - 1] == '\n' || out[n - 1] == ' ' || out[n - 1] == '\t')) {
        out[n - 1] = '\0';
        n--;
    }
    return (n > 0);
}

static int send_webhook(const char* url, const char* json) {
    if (!url || !json) return 0;
    // expect full url: https://discord.com/api/webhooks/...
    wchar_t host[256] = {0};
    wchar_t path[512] = {0};
    INTERNET_PORT port = INTERNET_DEFAULT_HTTPS_PORT;

    if (strncmp(url, "https://", 8) != 0) return 0;
    const char* host_start = url + 8;
    const char* path_start = strchr(host_start, '/');
    size_t host_len = path_start ? (size_t)(path_start - host_start) : strlen(host_start);
    if (host_len == 0 || host_len >= sizeof(host) / sizeof(host[0])) return 0;

    MultiByteToWideChar(CP_UTF8, 0, host_start, (int)host_len, host, (int)(sizeof(host) / sizeof(host[0]) - 1));
    if (path_start) {
        MultiByteToWideChar(CP_UTF8, 0, path_start, -1, path, (int)(sizeof(path) / sizeof(path[0]) - 1));
    } else {
        wcscpy_s(path, sizeof(path) / sizeof(path[0]), L"/");
    }

    HINTERNET h_session = WinHttpOpen(L"elfhv-bootstatus/1.0",
                                      WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME,
                                      WINHTTP_NO_PROXY_BYPASS, 0);
    if (!h_session) return 0;
    HINTERNET h_connect = WinHttpConnect(h_session, host, port, 0);
    if (!h_connect) { WinHttpCloseHandle(h_session); return 0; }
    HINTERNET h_request = WinHttpOpenRequest(h_connect, L"POST", path,
                                             NULL, WINHTTP_NO_REFERER,
                                             WINHTTP_DEFAULT_ACCEPT_TYPES,
                                             WINHTTP_FLAG_SECURE);
    if (!h_request) { WinHttpCloseHandle(h_connect); WinHttpCloseHandle(h_session); return 0; }

    const wchar_t* headers = L"content-type: application/json\r\n";
    BOOL ok = WinHttpSendRequest(h_request, headers, (DWORD)-1L,
                                 (LPVOID)json, (DWORD)strlen(json),
                                 (DWORD)strlen(json), 0);
    if (ok) ok = WinHttpReceiveResponse(h_request, NULL);

    WinHttpCloseHandle(h_request);
    WinHttpCloseHandle(h_connect);
    WinHttpCloseHandle(h_session);
    return ok ? 1 : 0;
}

static int read_boot_status(ELF_BOOT_STATUS* out) {
    if (!out) return 0;
    DWORD size = sizeof(*out);
    DWORD read = GetFirmwareEnvironmentVariableW(g_status_var, &g_elf_boot_status_guid, out, size);
    return (read == sizeof(*out));
}

static void clear_boot_status(void) {
    SetFirmwareEnvironmentVariableW(g_status_var, &g_elf_boot_status_guid, NULL, 0);
}

static void format_json(const ELF_BOOT_STATUS* st, char* out, size_t out_sz) {
    if (!st || !out || out_sz == 0) return;
    _snprintf(out, out_sz,
        "{"
        "\"content\":\"efi boot event\","
        "\"embeds\":[{"
          "\"title\":\"hv boot status\","
          "\"color\":3066993,"
          "\"fields\":["
            "{\"name\":\"hv enabled\",\"value\":\"%s\",\"inline\":true},"
            "{\"name\":\"hv initialized\",\"value\":\"%s\",\"inline\":true},"
            "{\"name\":\"virt supported\",\"value\":\"%s\",\"inline\":true},"
            "{\"name\":\"ept supported\",\"value\":\"%s\",\"inline\":true},"
            "{\"name\":\"config loaded\",\"value\":\"%s\",\"inline\":true},"
            "{\"name\":\"hv version\",\"value\":\"%u\",\"inline\":true},"
            "{\"name\":\"boot timestamp\",\"value\":\"%llu\"}"
          "]"
        "}]"
        "}",
        st->HvEnabled ? "true" : "false",
        st->HvInitialized ? "true" : "false",
        st->VirtualizationSupported ? "true" : "false",
        st->EptSupported ? "true" : "false",
        st->ConfigLoaded ? "true" : "false",
        (unsigned)st->HvVersion,
        (unsigned long long)st->BootTimestamp);
    out[out_sz - 1] = '\0';
}

static int run_once(void) {
    if (!enable_system_env_priv()) return 0;
    ELF_BOOT_STATUS st;
    if (!read_boot_status(&st)) return 0;

    char webhook[512];
    if (!read_file_utf8(g_webhook_path, webhook, sizeof(webhook))) return 0;

    char json[2048];
    format_json(&st, json, sizeof(json));
    if (!send_webhook(webhook, json)) return 0;

    clear_boot_status();
    return 1;
}

int wmain(int argc, wchar_t** argv) {
    (void)argv;
    if (argc > 1 && wcscmp(argv[1], L"--once") == 0) {
        return run_once() ? 0 : 1;
    }
    // simple fallback: run once and exit
    return run_once() ? 0 : 1;
}
