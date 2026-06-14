#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

#ifndef PB_TARGET_KB
#define PB_TARGET_KB 50
#endif

#ifndef PB_FEAT_LOG
#if PB_TARGET_KB >= 50
#define PB_FEAT_LOG 1
#else
#define PB_FEAT_LOG 0
#endif
#endif

#ifndef PB_FEAT_METRICS
#if PB_TARGET_KB >= 50
#define PB_FEAT_METRICS 1
#else
#define PB_FEAT_METRICS 0
#endif
#endif

#ifndef PB_FEAT_STREAM
#if PB_TARGET_KB >= 100
#define PB_FEAT_STREAM 1
#else
#define PB_FEAT_STREAM 0
#endif
#endif

#ifndef PB_FEAT_RANGE
#if PB_TARGET_KB >= 100
#define PB_FEAT_RANGE 1
#else
#define PB_FEAT_RANGE 0
#endif
#endif

#ifndef PB_FEAT_MIME_PLUS
#if PB_TARGET_KB >= 50
#define PB_FEAT_MIME_PLUS 1
#else
#define PB_FEAT_MIME_PLUS 0
#endif
#endif

#ifndef PB_FEAT_COPY_URL
#if PB_TARGET_KB >= 50
#define PB_FEAT_COPY_URL 1
#else
#define PB_FEAT_COPY_URL 0
#endif
#endif

#ifndef PB_FEAT_BROWSE
#if PB_TARGET_KB >= 50
#define PB_FEAT_BROWSE 1
#else
#define PB_FEAT_BROWSE 0
#endif
#endif

#ifndef PB_FEAT_TIMEOUT
#if PB_TARGET_KB >= 100
#define PB_FEAT_TIMEOUT 1
#else
#define PB_FEAT_TIMEOUT 0
#endif
#endif

#ifndef PB_FEAT_STATUS_ENDPOINT
#if PB_TARGET_KB >= 100
#define PB_FEAT_STATUS_ENDPOINT 1
#else
#define PB_FEAT_STATUS_ENDPOINT 0
#endif
#endif

#ifndef PB_FEAT_ACCESS_LOG
#if PB_TARGET_KB >= 100
#define PB_FEAT_ACCESS_LOG 1
#else
#define PB_FEAT_ACCESS_LOG 0
#endif
#endif

#ifndef PB_FEAT_CONFIG
#if PB_TARGET_KB >= 100
#define PB_FEAT_CONFIG 1
#else
#define PB_FEAT_CONFIG 0
#endif
#endif

#ifndef PB_FEAT_DIR_LIST
#if PB_TARGET_KB >= 100
#define PB_FEAT_DIR_LIST 1
#else
#define PB_FEAT_DIR_LIST 0
#endif
#endif

#ifndef PB_FEAT_BIND_ALL
#if PB_TARGET_KB >= 100
#define PB_FEAT_BIND_ALL 1
#else
#define PB_FEAT_BIND_ALL 0
#endif
#endif

#ifndef PB_FEAT_IPV6
#if PB_TARGET_KB >= 100
#define PB_FEAT_IPV6 1
#else
#define PB_FEAT_IPV6 0
#endif
#endif

#ifndef PB_FEAT_TRAY
#if PB_TARGET_KB >= 100
#define PB_FEAT_TRAY 1
#else
#define PB_FEAT_TRAY 0
#endif
#endif

#ifndef PB_FEAT_THREADS
#define PB_FEAT_THREADS 0
#endif

#ifndef PB_FEAT_WORKERS
#define PB_FEAT_WORKERS 0
#endif

#ifndef PB_FEAT_DEFENSE
#if PB_TARGET_KB >= 100
#define PB_FEAT_DEFENSE 1
#else
#define PB_FEAT_DEFENSE 0
#endif
#endif

#ifndef PB_FEAT_JELLY
#ifdef PORTBLASTER_CHECK
#define PB_FEAT_JELLY 1
#else
#define PB_FEAT_JELLY 0
#endif
#endif

#define ID_PORT 101
#define ID_ROOT 102
#define ID_START 103
#define ID_STOP 104
#define ID_STATUS 105
#define ID_ACTIVITY 106
#define ID_COPY_URL 107
#define ID_BROWSE 108
#define ID_TIMEOUT 109
#define ID_WORKERS 110
#define ID_QUEUE 111
#define ID_DEFENSE 112
#define ID_TRAY_RESTORE 201
#define ID_TRAY_COPY 202
#define ID_TRAY_STOP 203
#define ID_TRAY_EXIT 204
#define WM_TRAY (WM_APP + 20)

#define REQ_MAX 1024
#define ROOT_MAX 384
#define PATH_MAX_LOCAL 768
#define HEADER_MAX 512
#define FILE_MAX (1024 * 1024)
#define STREAM_CHUNK 8192
#define PB_WORKER_MAX 8
#define PB_QUEUE_MAX 16

static HWND g_port_edit;
static HWND g_root_edit;
#if PB_FEAT_BROWSE
static HWND g_browse_button;
#endif
static HWND g_start_button;
static HWND g_stop_button;
#if PB_FEAT_COPY_URL
static HWND g_copy_button;
#endif
static HWND g_status;
static HWND g_activity;
static HWND g_main;
#if PB_FEAT_DEFENSE
static HWND g_timeout_edit;
static HWND g_workers_edit;
static HWND g_queue_edit;
static HWND g_defense_status;
#endif
#if PB_FEAT_TRAY
static NOTIFYICONDATAA g_tray;
static int g_tray_added = 0;
#endif

static SOCKET g_listener = INVALID_SOCKET;
static HANDLE g_thread = 0;
#if PB_FEAT_THREADS || PB_FEAT_WORKERS
static CRITICAL_SECTION g_note_lock;
#endif
#if PB_FEAT_WORKERS
static CRITICAL_SECTION g_queue_lock;
static HANDLE g_queue_event;
static SOCKET g_queue[PB_QUEUE_MAX];
static int g_queue_head = 0;
static int g_queue_tail = 0;
static int g_queue_count = 0;
#endif
static volatile LONG g_running = 0;
static volatile LONG g_request_count = 0;
static volatile LONG g_last_status = 0;
#if PB_FEAT_DEFENSE
static volatile LONG g_active_workers = 0;
static volatile LONG g_rejected_clients = 0;
static volatile LONG g_timeout_clients = 0;
static int g_timeout_ms = 5000;
static int g_worker_count = 4;
static int g_queue_limit = 16;
#endif
#if PB_FEAT_METRICS
static volatile LONG g_bytes_served = 0;
#endif
#if PB_FEAT_LOG
static volatile LONG g_last_bytes = 0;
static LONG g_log_chars = 0;
#endif
#if PB_FEAT_METRICS
static DWORD g_started_tick = 0;
#endif
static char g_root[ROOT_MAX];
static char g_root_full[PATH_MAX_LOCAL];
static char g_url[64];
static char g_activity_text[256];
static char g_last_path[128];
#if PB_FEAT_ACCESS_LOG
static char g_access_log_path[PATH_MAX_LOCAL];
#endif
#if PB_FEAT_CONFIG
static char g_config[512];
static int g_config_error = 0;
#endif
#if PB_FEAT_DIR_LIST
static int g_dir_list = 0;
#endif
#if PB_FEAT_BIND_ALL
static int g_bind_all = 0;
#endif
#if PB_FEAT_IPV6
static int g_ipv6 = 0;
#endif
static unsigned short g_port = 8083;

typedef struct PB_CLIENT {
    SOCKET client;
    char request[REQ_MAX];
    char path[PATH_MAX_LOCAL];
    char header[HEADER_MAX];
    char last_path[128];
#if PB_FEAT_STATUS_ENDPOINT
    char status_body[HEADER_MAX];
#endif
#if PB_FEAT_DIR_LIST
    char dir_body[HEADER_MAX];
#endif
#if PB_FEAT_STREAM
    unsigned char chunk[STREAM_CHUNK];
#else
    unsigned char file[FILE_MAX];
#endif
} PB_CLIENT;

static void refresh_activity(void);
static int starts_with(const char *s, const char *prefix);
static unsigned short parse_port(const char *s);

static void zero_bytes(void *ptr, DWORD size) {
    volatile unsigned char *p = (volatile unsigned char *)ptr;
    while (size--) *p++ = 0;
}

static void set_status(const char *text) {
    SetWindowTextA(g_status, text);
}

static void set_default_root(void) {
    DWORD n = GetModuleFileNameA(0, g_root, sizeof(g_root) - 16);
    while (n > 0 && g_root[n - 1] != '\\' && g_root[n - 1] != '/') n--;
    g_root[n] = 0;
    lstrcatA(g_root, "public");
}

#if PB_FEAT_CONFIG
static void copy_line_value(char *dst, int dst_len, const char *src) {
    int p = 0;
    while (*src && *src != '\r' && *src != '\n' && p < dst_len - 1) {
        dst[p++] = *src++;
    }
    dst[p] = 0;
}

static void load_config(void) {
    char path[PATH_MAX_LOCAL];
    HANDLE f;
    DWORD n;
    char *p;
    DWORD len = GetModuleFileNameA(0, path, sizeof(path) - 17);
    while (len > 0 && path[len - 1] != '\\' && path[len - 1] != '/') len--;
    path[len] = 0;
    lstrcatA(path, "portblaster.ini");
    f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (f == INVALID_HANDLE_VALUE) return;
    if (!ReadFile(f, g_config, sizeof(g_config) - 1, &n, 0)) n = 0;
    CloseHandle(f);
    g_config[n] = 0;
    p = g_config;
    while (*p) {
        if (starts_with(p, "port=")) {
            char tmp[16];
            unsigned short port;
            copy_line_value(tmp, sizeof(tmp), p + 5);
            port = parse_port(tmp);
            if (port) {
                g_port = port;
            } else {
                g_config_error = 1;
            }
        } else if (starts_with(p, "root=")) {
            copy_line_value(g_root, sizeof(g_root), p + 5);
#if PB_FEAT_DEFENSE
        } else if (starts_with(p, "timeout_ms=")) {
            char tmp[16];
            unsigned short v;
            copy_line_value(tmp, sizeof(tmp), p + 11);
            v = parse_port(tmp);
            if (v >= 1000 && v <= 30000) g_timeout_ms = v;
            else g_config_error = 1;
        } else if (starts_with(p, "workers=")) {
            char tmp[16];
            unsigned short v;
            copy_line_value(tmp, sizeof(tmp), p + 8);
            v = parse_port(tmp);
            if (v >= 1 && v <= PB_WORKER_MAX) g_worker_count = v;
            else g_config_error = 1;
        } else if (starts_with(p, "queue=")) {
            char tmp[16];
            unsigned short v;
            copy_line_value(tmp, sizeof(tmp), p + 6);
            v = parse_port(tmp);
            if (v >= 1 && v <= PB_QUEUE_MAX) g_queue_limit = v;
            else g_config_error = 1;
#endif
#if PB_FEAT_DIR_LIST
        } else if (starts_with(p, "dir_list=1")) {
            g_dir_list = 1;
#endif
#if PB_FEAT_BIND_ALL
        } else if (starts_with(p, "bind=all")) {
            g_bind_all = 1;
#endif
#if PB_FEAT_IPV6
        } else if (starts_with(p, "ipv6=1")) {
            g_ipv6 = 1;
#endif
        }
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
    }
}
#endif

#if PB_FEAT_ACCESS_LOG
static void set_access_log_path(void) {
    DWORD n = GetModuleFileNameA(0, g_access_log_path, sizeof(g_access_log_path) - 18);
    while (n > 0 && g_access_log_path[n - 1] != '\\' && g_access_log_path[n - 1] != '/') n--;
    g_access_log_path[n] = 0;
    lstrcatA(g_access_log_path, "portblaster.log");
}
#endif

static void set_running_status(void) {
#if PB_FEAT_IPV6
    if (g_ipv6) {
        wsprintfA(g_url, "http://[::1]:%u/", (unsigned int)g_port);
    } else
#endif
    wsprintfA(g_url, "http://127.0.0.1:%u/", (unsigned int)g_port);
#if PB_FEAT_METRICS
#if PB_FEAT_BIND_ALL
    if (g_bind_all) {
        wsprintfA(g_activity_text, "PortBlaster - Running all :%u", (unsigned int)g_port);
    } else
#endif
    wsprintfA(g_activity_text, "PortBlaster - Running :%u", (unsigned int)g_port);
    SetWindowTextA(g_main, g_activity_text);
#if PB_FEAT_TRAY
    if (g_tray_added) {
        lstrcpynA(g_tray.szTip, g_activity_text, sizeof(g_tray.szTip));
        Shell_NotifyIconA(NIM_MODIFY, &g_tray);
    }
#endif
    SetTimer(g_main, 1, 1000, 0);
    refresh_activity();
#else
    wsprintfA(g_activity_text, "Running: %s", g_url);
    set_status(g_activity_text);
#endif
}

static void refresh_activity(void) {
    LONG count = g_request_count;
    LONG status = g_last_status;
#if PB_FEAT_METRICS
    LONG bytes = g_bytes_served;
    DWORD up = g_started_tick ? (GetTickCount() - g_started_tick) / 1000 : 0;
    DWORD h = up / 3600;
    DWORD m = (up / 60) % 60;
    DWORD s = up % 60;
    if (g_running) {
#if PB_FEAT_BIND_ALL
        if (g_bind_all) {
            wsprintfA(g_activity_text, "%s all | Req: %ld | Bytes: %ld | Up: %02lu:%02lu:%02lu", g_url, count, bytes, h, m, s);
        } else
#endif
        wsprintfA(g_activity_text, "%s | Req: %ld | Bytes: %ld | Up: %02lu:%02lu:%02lu", g_url, count, bytes, h, m, s);
        set_status(g_activity_text);
    }
#else
    if (count <= 0) {
        SetWindowTextA(g_activity, "Requests: 0 | Last: none");
    } else {
        wsprintfA(g_activity_text, "Requests: %ld | Last: %ld %s", count, status, g_last_path);
        SetWindowTextA(g_activity, g_activity_text);
    }
#endif
#if PB_FEAT_DEFENSE
    if (g_defense_status) {
        int q = 0;
#if PB_FEAT_WORKERS
        EnterCriticalSection(&g_queue_lock);
        q = g_queue_count;
        LeaveCriticalSection(&g_queue_lock);
#endif
        wsprintfA(g_activity_text, "Active %ld | Queue %d/%d | Rejected %ld | Timeouts %ld",
            g_active_workers, q, g_queue_limit, g_rejected_clients, g_timeout_clients);
        SetWindowTextA(g_defense_status, g_activity_text);
    }
#endif
}

#if PB_FEAT_LOG
static void append_log(void) {
    LONG bytes = g_last_bytes;
    if (g_log_chars > 12000) {
        SetWindowTextA(g_activity, "");
        g_log_chars = 0;
    }
    wsprintfA(g_activity_text, "%ld %s %ld B\r\n", g_last_status, g_last_path, bytes);
    g_log_chars += lstrlenA(g_activity_text);
    SendMessageA(g_activity, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    SendMessageA(g_activity, EM_REPLACESEL, 0, (LPARAM)g_activity_text);
    SendMessageA(g_activity, EM_SCROLLCARET, 0, 0);
}
#endif

static void append(char *dst, int *pos, const char *src) {
    while (*src && *pos < HEADER_MAX - 1) {
        dst[(*pos)++] = *src++;
    }
    dst[*pos] = 0;
}

static void append_u32(char *dst, int *pos, DWORD value) {
    char tmp[16];
    int n = 0;
    if (!value) {
        append(dst, pos, "0");
        return;
    }
    while (value && n < 16) {
        tmp[n++] = (char)('0' + value % 10);
        value /= 10;
    }
    while (n > 0 && *pos < HEADER_MAX - 1) {
        dst[(*pos)++] = tmp[--n];
    }
    dst[*pos] = 0;
}

#if PB_FEAT_ACCESS_LOG
static void append_log_file(int status, DWORD bytes, const char *path) {
    HANDLE f;
    DWORD wrote;
    int p = 0;
    char line[HEADER_MAX];
    append_u32(line, &p, (DWORD)status);
    append(line, &p, " ");
    append(line, &p, path);
    append(line, &p, " ");
    append_u32(line, &p, bytes);
    append(line, &p, " B\r\n");
    f = CreateFileA(g_access_log_path, FILE_APPEND_DATA, FILE_SHARE_READ, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (f == INVALID_HANDLE_VALUE) return;
    WriteFile(f, line, (DWORD)p, &wrote, 0);
    CloseHandle(f);
}
#endif

static int starts_with(const char *s, const char *prefix) {
    while (*prefix) {
        if (*s++ != *prefix++) return 0;
    }
    return 1;
}

static int token_equals(const char *s, const char *token) {
    while (*token) {
        if (*s++ != *token++) return 0;
    }
    return *s == ' ' || *s == '?' || *s == 0 || *s == '\r' || *s == '\n';
}

static const char *find_text(const char *s, const char *needle) {
    const char *p;
    while (*s) {
        p = needle;
        while (*p && s[p - needle] == *p) p++;
        if (!*p) return s;
        s++;
    }
    return 0;
}

#if PB_FEAT_JELLY
static int contains(const char *s, const char *needle) {
    const char *p;
    while (*s) {
        p = needle;
        while (*p && s[p - needle] == *p) p++;
        if (!*p) return 1;
        s++;
    }
    return 0;
}
#endif

#if PB_FEAT_BROWSE
static void browse_root(void) {
    BROWSEINFOA bi;
    LPITEMIDLIST pidl;
    char path[ROOT_MAX];
    zero_bytes(&bi, sizeof(bi));
    bi.hwndOwner = g_main;
    bi.lpszTitle = "Choose PortBlaster root";
    bi.ulFlags = BIF_RETURNONLYFSDIRS;
    pidl = SHBrowseForFolderA(&bi);
    if (!pidl) return;
    if (SHGetPathFromIDListA(pidl, path)) {
        SetWindowTextA(g_root_edit, path);
    }
    CoTaskMemFree(pidl);
}
#endif

#if PB_FEAT_COPY_URL
static void copy_current_url(void) {
    HGLOBAL mem;
    char *dst;
    int len;
    if (!g_running || !g_url[0] || !OpenClipboard(g_main)) return;
    EmptyClipboard();
    len = lstrlenA(g_url) + 1;
    mem = GlobalAlloc(GMEM_MOVEABLE, len);
    if (mem) {
        dst = (char *)GlobalLock(mem);
        if (dst) {
            lstrcpyA(dst, g_url);
            GlobalUnlock(mem);
            if (SetClipboardData(CF_TEXT, mem)) {
                mem = 0;
                set_status("Copied URL.");
            }
        }
        if (mem) GlobalFree(mem);
    }
    CloseClipboard();
}
#endif

static int ends_with(const char *s, const char *suffix) {
    int sl = lstrlenA(s);
    int tl = lstrlenA(suffix);
    if (tl > sl) return 0;
    return lstrcmpiA(s + sl - tl, suffix) == 0;
}

static const char *type_for(const char *path) {
    if (ends_with(path, ".html") || ends_with(path, ".htm")) return "text/html; charset=utf-8";
    if (ends_with(path, ".css")) return "text/css; charset=utf-8";
    if (ends_with(path, ".js")) return "application/javascript; charset=utf-8";
    if (ends_with(path, ".gif")) return "image/gif";
    if (ends_with(path, ".png")) return "image/png";
    if (ends_with(path, ".jpg") || ends_with(path, ".jpeg")) return "image/jpeg";
    if (ends_with(path, ".svg")) return "image/svg+xml";
    if (ends_with(path, ".ico")) return "image/x-icon";
    if (ends_with(path, ".txt")) return "text/plain; charset=utf-8";
#if PB_FEAT_MIME_PLUS
    if (ends_with(path, ".json") || ends_with(path, ".map")) return "application/json; charset=utf-8";
    if (ends_with(path, ".webp")) return "image/webp";
    if (ends_with(path, ".wasm")) return "application/wasm";
    if (ends_with(path, ".mjs")) return "application/javascript; charset=utf-8";
    if (ends_with(path, ".pdf")) return "application/pdf";
#endif
    return "application/octet-stream";
}

static void copy_url_token(char *dst, const char *url) {
    int p = 0;
    while (*url && *url != ' ' && *url != '?' && p < 127) {
        dst[p++] = *url++;
    }
    if (p == 0 || (p == 1 && dst[0] == '/')) {
        dst[0] = '/';
        dst[1] = 0;
    } else {
        dst[p] = 0;
    }
}

static void note_request(int status, DWORD bytes, const char *path) {
    InterlockedIncrement((LONG *)&g_request_count);
    InterlockedExchange((LONG *)&g_last_status, status);
#if PB_FEAT_LOG
    InterlockedExchange((LONG *)&g_last_bytes, (LONG)bytes);
#endif
#if PB_FEAT_METRICS
    if (bytes) InterlockedExchangeAdd((LONG *)&g_bytes_served, (LONG)bytes);
#else
    (void)bytes;
#endif
#if PB_FEAT_THREADS || PB_FEAT_WORKERS
    EnterCriticalSection(&g_note_lock);
#endif
    lstrcpynA(g_last_path, path, sizeof(g_last_path));
#if PB_FEAT_ACCESS_LOG
    append_log_file(status, bytes, path);
#endif
#if PB_FEAT_THREADS || PB_FEAT_WORKERS
    LeaveCriticalSection(&g_note_lock);
#endif
    PostMessageA(g_main, WM_APP + 4, 0, 0);
}

static unsigned short parse_port(const char *s) {
    DWORD value = 0;
    while (*s >= '0' && *s <= '9') {
        value = value * 10 + (DWORD)(*s - '0');
        s++;
    }
    if (value < 1 || value > 65535) return 0;
    return (unsigned short)value;
}

static int bad_path(const char *s) {
    if (*s != '/') return 1;
    while (*s && *s != ' ' && *s != '?') {
        if (s[0] == '.' && s[1] == '.') return 1;
        if (*s == ':') return 1;
        if (*s == '\\') return 1;
        if (*s == '%') return 1;
        if ((unsigned char)*s < 32) return 1;
        s++;
    }
    return 0;
}

static int good_request_line(const char *s) {
    while (*s && *s != ' ' && *s != '\r' && *s != '\n') s++;
    if (*s++ != ' ') return 0;
    return (starts_with(s, "HTTP/1.0\r") || starts_with(s, "HTTP/1.1\r") ||
        starts_with(s, "HTTP/1.0\n") || starts_with(s, "HTTP/1.1\n"));
}

static void trim_root_slash(char *s) {
    int n = lstrlenA(s);
    while (n > 3 && (s[n - 1] == '\\' || s[n - 1] == '/')) {
        s[--n] = 0;
    }
}

static int same_prefix_i(const char *s, const char *prefix) {
    while (*prefix) {
        char a = *s++;
        char b = *prefix++;
        if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
        if (b >= 'A' && b <= 'Z') b = (char)(b + 32);
        if (a != b) return 0;
    }
    return 1;
}

static int path_under_root(const char *path) {
    int n = lstrlenA(g_root_full);
    if (!same_prefix_i(path, g_root_full)) return 0;
    return path[n] == 0 || path[n] == '\\' || path[n] == '/';
}

static int set_root_full(void) {
    DWORD n = GetFullPathNameA(g_root, sizeof(g_root_full), g_root_full, 0);
    DWORD attr;
    if (n == 0 || n >= sizeof(g_root_full)) return 0;
    trim_root_slash(g_root_full);
    attr = GetFileAttributesA(g_root_full);
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY);
}

static int make_path(PB_CLIENT *ctx, const char *url) {
    int p = 0;
    int i = 0;
    int root_len = lstrlenA(g_root_full);
    char full[PATH_MAX_LOCAL];
    DWORD full_len;

    if (root_len <= 0 || bad_path(url)) return 0;

    while (g_root_full[i] && p < PATH_MAX_LOCAL - 2) {
        char ch = g_root_full[i++];
        ctx->path[p++] = ch;
    }
    if (p > 0 && ctx->path[p - 1] != '\\' && ctx->path[p - 1] != '/') {
        ctx->path[p++] = '\\';
    }

    if (url[0] == '/' && (url[1] == ' ' || url[1] == '?' || url[1] == 0)) {
        url = "/index.html";
    }
    if (*url == '/') url++;

    while (*url && *url != ' ' && *url != '?' && p < PATH_MAX_LOCAL - 1) {
        char ch = *url++;
        ctx->path[p++] = (ch == '/') ? '\\' : ch;
    }
    if (*url && *url != ' ' && *url != '?') return 0;
    ctx->path[p] = 0;

    full_len = GetFullPathNameA(ctx->path, sizeof(full), full, 0);
    if (full_len == 0 || full_len >= sizeof(full) || !path_under_root(full)) return 0;
    lstrcpyA(ctx->path, full);
    return 1;
}

static void send_all(SOCKET client, const char *data, DWORD len) {
    while (len) {
        int n = send(client, data, len > 32767 ? 32767 : (int)len, 0);
        if (n <= 0) return;
        data += n;
        len -= (DWORD)n;
    }
}

#if PB_FEAT_TIMEOUT
static void set_client_timeout(SOCKET client) {
#if PB_FEAT_DEFENSE
    DWORD timeout = (DWORD)g_timeout_ms;
#else
    DWORD timeout = 5000;
#endif
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
    setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof(timeout));
}
#endif

static void send_response(PB_CLIENT *ctx, int status, const char *reason, const char *type, const unsigned char *body, DWORD body_len, int head_only) {
    int p = 0;
    append(ctx->header, &p, "HTTP/1.1 ");
    append_u32(ctx->header, &p, (DWORD)status);
    append(ctx->header, &p, " ");
    append(ctx->header, &p, reason);
    append(ctx->header, &p, "\r\nContent-Type: ");
    append(ctx->header, &p, type);
    append(ctx->header, &p, "\r\nContent-Length: ");
    append_u32(ctx->header, &p, body_len);
    append(ctx->header, &p, "\r\nConnection: close\r\n\r\n");
    send_all(ctx->client, ctx->header, (DWORD)p);
    if (!head_only && body_len) {
        send_all(ctx->client, (const char *)body, body_len);
    }
}

#if PB_FEAT_RANGE
static int parse_range(PB_CLIENT *ctx, DWORD file_len, DWORD *start, DWORD *end) {
    const char *p = find_text(ctx->request, "\r\nRange: bytes=");
    DWORD a = 0;
    DWORD b = 0;
    int has_b = 0;
    if (!p) return 0;
    p += 15;
    if (*p < '0' || *p > '9') return -1;
    while (*p >= '0' && *p <= '9') {
        a = a * 10 + (DWORD)(*p++ - '0');
    }
    if (*p++ != '-') return -1;
    while (*p >= '0' && *p <= '9') {
        has_b = 1;
        b = b * 10 + (DWORD)(*p++ - '0');
    }
    if ((p[0] != '\r' && p[0] != '\n') || a >= file_len) return -1;
    if (!has_b || b >= file_len) b = file_len - 1;
    if (b < a) return -1;
    *start = a;
    *end = b;
    return 1;
}

static void send_range_bad(PB_CLIENT *ctx, DWORD file_len) {
    int p = 0;
    append(ctx->header, &p, "HTTP/1.1 416 Range Not Satisfiable\r\nContent-Range: bytes */");
    append_u32(ctx->header, &p, file_len);
    append(ctx->header, &p, "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
    send_all(ctx->client, ctx->header, (DWORD)p);
}

static int send_file_range(PB_CLIENT *ctx, HANDLE file, DWORD file_len, DWORD start, DWORD end, const char *type, int head_only) {
    int p = 0;
    DWORD left = end - start + 1;
    DWORD read_count;
    SetFilePointer(file, (LONG)start, 0, FILE_BEGIN);
    append(ctx->header, &p, "HTTP/1.1 206 Partial Content\r\nContent-Type: ");
    append(ctx->header, &p, type);
    append(ctx->header, &p, "\r\nContent-Range: bytes ");
    append_u32(ctx->header, &p, start);
    append(ctx->header, &p, "-");
    append_u32(ctx->header, &p, end);
    append(ctx->header, &p, "/");
    append_u32(ctx->header, &p, file_len);
    append(ctx->header, &p, "\r\nContent-Length: ");
    append_u32(ctx->header, &p, left);
    append(ctx->header, &p, "\r\nConnection: close\r\n\r\n");
    send_all(ctx->client, ctx->header, (DWORD)p);
    if (head_only) return 1;
    while (left) {
        DWORD want = left > STREAM_CHUNK ? STREAM_CHUNK : left;
        if (!ReadFile(file, ctx->chunk, want, &read_count, 0) || read_count == 0) return 0;
        send_all(ctx->client, (const char *)ctx->chunk, read_count);
        left -= read_count;
    }
    return 1;
}
#endif

#if PB_FEAT_DIR_LIST
static void append_html_name(char *dst, int *pos, const char *src) {
    while (*src && *pos < HEADER_MAX - 1) {
        if (*src == '<') append(dst, pos, "&lt;");
        else if (*src == '>') append(dst, pos, "&gt;");
        else if (*src == '&') append(dst, pos, "&amp;");
        else if (*src == '"') append(dst, pos, "&quot;");
        else {
            dst[(*pos)++] = *src;
            dst[*pos] = 0;
        }
        src++;
    }
}

static void send_directory_listing(PB_CLIENT *ctx, int head_only) {
    WIN32_FIND_DATAA fd;
    HANDLE find;
    char pattern[PATH_MAX_LOCAL];
    int p = 0;
    append(ctx->dir_body, &p, "<!doctype html><title>Index</title><h1>Index</h1><ul>");
    lstrcpyA(pattern, ctx->path);
    lstrcatA(pattern, "\\*");
    find = FindFirstFileA(pattern, &fd);
    if (find != INVALID_HANDLE_VALUE) {
        do {
            if (fd.cFileName[0] == '.') continue;
            if (fd.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) continue;
            append(ctx->dir_body, &p, "<li><a href=\"");
            append_html_name(ctx->dir_body, &p, fd.cFileName);
            append(ctx->dir_body, &p, "\">");
            append_html_name(ctx->dir_body, &p, fd.cFileName);
            append(ctx->dir_body, &p, "</a></li>");
        } while (FindNextFileA(find, &fd));
        FindClose(find);
    }
    append(ctx->dir_body, &p, "</ul>");
    send_response(ctx, 200, "OK", "text/html; charset=utf-8", (const unsigned char *)ctx->dir_body, (DWORD)p, head_only);
    note_request(200, head_only ? 0 : (DWORD)p, ctx->last_path);
}
#endif

#if PB_FEAT_STATUS_ENDPOINT
static void send_status_endpoint(PB_CLIENT *ctx, int head_only) {
    int p = 0;
    DWORD up = g_started_tick ? (GetTickCount() - g_started_tick) / 1000 : 0;
    append(ctx->status_body, &p, "target=");
    append_u32(ctx->status_body, &p, PB_TARGET_KB);
    append(ctx->status_body, &p, "\nrequests=");
    append_u32(ctx->status_body, &p, (DWORD)g_request_count);
    append(ctx->status_body, &p, "\nbytes=");
    append_u32(ctx->status_body, &p, (DWORD)g_bytes_served);
    append(ctx->status_body, &p, "\nuptime=");
    append_u32(ctx->status_body, &p, up);
    append(ctx->status_body, &p, "\nfeatures=LOG,METRICS,STREAM,MIME_PLUS,TIMEOUT,STATUS_ENDPOINT");
#if PB_FEAT_THREADS
    append(ctx->status_body, &p, ",THREADS");
#endif
#if PB_FEAT_WORKERS
    append(ctx->status_body, &p, ",WORKERS");
#endif
#if PB_FEAT_DEFENSE
    append(ctx->status_body, &p, ",DEFENSE");
    append(ctx->status_body, &p, "\nworkers=");
    append_u32(ctx->status_body, &p, (DWORD)g_worker_count);
    append(ctx->status_body, &p, "\nqueue=");
#if PB_FEAT_WORKERS
    append_u32(ctx->status_body, &p, (DWORD)g_queue_count);
#else
    append_u32(ctx->status_body, &p, 0);
#endif
    append(ctx->status_body, &p, "\nqueue_limit=");
    append_u32(ctx->status_body, &p, (DWORD)g_queue_limit);
    append(ctx->status_body, &p, "\nactive=");
    append_u32(ctx->status_body, &p, (DWORD)g_active_workers);
    append(ctx->status_body, &p, "\nrejected=");
    append_u32(ctx->status_body, &p, (DWORD)g_rejected_clients);
    append(ctx->status_body, &p, "\ntimeouts=");
    append_u32(ctx->status_body, &p, (DWORD)g_timeout_clients);
    append(ctx->status_body, &p, "\ntimeout_ms=");
    append_u32(ctx->status_body, &p, (DWORD)g_timeout_ms);
#endif
    append(ctx->status_body, &p, "\n");
    send_response(ctx, 200, "OK", "text/plain; charset=utf-8", (const unsigned char *)ctx->status_body, (DWORD)p, head_only);
    note_request(200, head_only ? 0 : (DWORD)p, ctx->last_path);
}
#endif

#if PB_FEAT_STREAM
static int send_file_response(PB_CLIENT *ctx, HANDLE file, DWORD body_len, const char *type, int head_only) {
    int p = 0;
    DWORD left = body_len;
    DWORD read_count;
    append(ctx->header, &p, "HTTP/1.1 200 OK\r\nContent-Type: ");
    append(ctx->header, &p, type);
    append(ctx->header, &p, "\r\nContent-Length: ");
    append_u32(ctx->header, &p, body_len);
    append(ctx->header, &p, "\r\nConnection: close\r\n\r\n");
    send_all(ctx->client, ctx->header, (DWORD)p);
    if (head_only) return 1;
    while (left) {
        DWORD want = left > STREAM_CHUNK ? STREAM_CHUNK : left;
        if (!ReadFile(file, ctx->chunk, want, &read_count, 0) || read_count == 0) return 0;
        send_all(ctx->client, (const char *)ctx->chunk, read_count);
        left -= read_count;
    }
    return 1;
}
#endif

static void handle_client(PB_CLIENT *ctx) {
    SOCKET client = ctx->client;
    int n = recv(client, ctx->request, REQ_MAX - 1, 0);
    int head_only = 0;
    const char *url;
    HANDLE file;
    DWORD size_low;
    DWORD attr;
#if !PB_FEAT_STREAM
    DWORD read_count;
#endif

    if (n <= 0) {
#if PB_FEAT_DEFENSE
        if (WSAGetLastError() == WSAETIMEDOUT) {
            InterlockedIncrement((LONG *)&g_timeout_clients);
            PostMessageA(g_main, WM_APP + 4, 0, 0);
        }
#endif
        closesocket(client);
        return;
    }
    ctx->request[n] = 0;

    if (starts_with(ctx->request, "HEAD ")) {
        head_only = 1;
        url = ctx->request + 5;
    } else if (!starts_with(ctx->request, "GET ")) {
        static const unsigned char msg[] = "Method Not Allowed\n";
        send_response(ctx, 405, "Method Not Allowed", "text/plain; charset=utf-8", msg, sizeof(msg) - 1, 0);
        lstrcpyA(ctx->last_path, "(method)");
        note_request(405, 0, ctx->last_path);
        closesocket(client);
        return;
    } else {
        url = ctx->request + 4;
    }

    copy_url_token(ctx->last_path, url);
    if (!good_request_line(url)) {
        static const unsigned char msg[] = "Bad Request\n";
        send_response(ctx, 400, "Bad Request", "text/plain; charset=utf-8", msg, sizeof(msg) - 1, 0);
        note_request(400, 0, ctx->last_path);
        closesocket(client);
        return;
    }

#if PB_FEAT_STATUS_ENDPOINT
    if (token_equals(url, "/__pb/status")) {
        send_status_endpoint(ctx, head_only);
        closesocket(client);
        return;
    }
#endif

    if (!make_path(ctx, url)) {
        static const unsigned char msg[] = "Forbidden\n";
        send_response(ctx, 403, "Forbidden", "text/plain; charset=utf-8", msg, sizeof(msg) - 1, 0);
        note_request(403, 0, ctx->last_path);
        closesocket(client);
        return;
    }

    attr = GetFileAttributesA(ctx->path);
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
#if PB_FEAT_DIR_LIST
        if (g_dir_list) {
            send_directory_listing(ctx, head_only);
            closesocket(client);
            return;
        }
#endif
        {
            static const unsigned char msg[] = "Not Found\n";
            send_response(ctx, 404, "Not Found", "text/plain; charset=utf-8", msg, sizeof(msg) - 1, 0);
            note_request(404, 0, ctx->last_path);
            closesocket(client);
            return;
        }
    }

    file = CreateFileA(ctx->path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE) {
        static const unsigned char msg[] = "Not Found\n";
        send_response(ctx, 404, "Not Found", "text/plain; charset=utf-8", msg, sizeof(msg) - 1, 0);
        note_request(404, 0, ctx->last_path);
        closesocket(client);
        return;
    }

    size_low = GetFileSize(file, 0);
#if PB_FEAT_STREAM
    if (size_low == INVALID_FILE_SIZE) {
        static const unsigned char msg[] = "Read Error\n";
        CloseHandle(file);
        send_response(ctx, 500, "Internal Server Error", "text/plain; charset=utf-8", msg, sizeof(msg) - 1, 0);
        note_request(500, 0, ctx->last_path);
        closesocket(client);
        return;
    }

    {
#if PB_FEAT_RANGE
        DWORD range_start = 0;
        DWORD range_end = 0;
        int range = parse_range(ctx, size_low, &range_start, &range_end);
        if (range < 0) {
            send_range_bad(ctx, size_low);
            CloseHandle(file);
            note_request(416, 0, ctx->last_path);
            closesocket(client);
            return;
        }
        if (range > 0) {
            if (!send_file_range(ctx, file, size_low, range_start, range_end, type_for(ctx->path), head_only)) {
                CloseHandle(file);
                note_request(500, 0, ctx->last_path);
                closesocket(client);
                return;
            }
            CloseHandle(file);
            note_request(206, head_only ? 0 : range_end - range_start + 1, ctx->last_path);
            closesocket(client);
            return;
        }
#endif
    }
    if (!send_file_response(ctx, file, size_low, type_for(ctx->path), head_only)) {
        CloseHandle(file);
        note_request(500, 0, ctx->last_path);
        closesocket(client);
        return;
    }

    CloseHandle(file);
    note_request(200, head_only ? 0 : size_low, ctx->last_path);
    closesocket(client);
    return;
#else
    if (size_low == INVALID_FILE_SIZE || size_low > FILE_MAX) {
        static const unsigned char msg[] = "File Too Large\n";
        CloseHandle(file);
        send_response(ctx, 413, "Payload Too Large", "text/plain; charset=utf-8", msg, sizeof(msg) - 1, 0);
        note_request(413, 0, ctx->last_path);
        closesocket(client);
        return;
    }

    if (!ReadFile(file, ctx->file, size_low, &read_count, 0) || read_count != size_low) {
        static const unsigned char msg[] = "Read Error\n";
        CloseHandle(file);
        send_response(ctx, 500, "Internal Server Error", "text/plain; charset=utf-8", msg, sizeof(msg) - 1, 0);
        note_request(500, 0, ctx->last_path);
        closesocket(client);
        return;
    }

    CloseHandle(file);
    send_response(ctx, 200, "OK", type_for(ctx->path), ctx->file, size_low, head_only);
    note_request(200, head_only ? 0 : size_low, ctx->last_path);
    closesocket(client);
#endif
}

#if PB_FEAT_THREADS
static DWORD WINAPI client_thread(LPVOID param) {
    PB_CLIENT *ctx = (PB_CLIENT *)param;
    handle_client(ctx);
    HeapFree(GetProcessHeap(), 0, ctx);
    return 0;
}
#endif

#if PB_FEAT_WORKERS
static void send_busy(SOCKET client) {
    static const char msg[] = "HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
#if PB_FEAT_DEFENSE
    InterlockedIncrement((LONG *)&g_rejected_clients);
#endif
    send_all(client, msg, sizeof(msg) - 1);
    closesocket(client);
    PostMessageA(g_main, WM_APP + 4, 0, 0);
}

static int enqueue_client(SOCKET client) {
    int ok = 0;
    EnterCriticalSection(&g_queue_lock);
#if PB_FEAT_DEFENSE
    if (g_queue_count < g_queue_limit) {
#else
    if (g_queue_count < PB_QUEUE_MAX) {
#endif
        g_queue[g_queue_tail] = client;
        g_queue_tail = (g_queue_tail + 1) % PB_QUEUE_MAX;
        g_queue_count++;
        ok = 1;
        SetEvent(g_queue_event);
    }
    LeaveCriticalSection(&g_queue_lock);
    return ok;
}

static SOCKET dequeue_client(void) {
    SOCKET client = INVALID_SOCKET;
    EnterCriticalSection(&g_queue_lock);
    if (g_queue_count > 0) {
        client = g_queue[g_queue_head];
        g_queue_head = (g_queue_head + 1) % PB_QUEUE_MAX;
        g_queue_count--;
        if (g_queue_count > 0) SetEvent(g_queue_event);
    }
    LeaveCriticalSection(&g_queue_lock);
    return client;
}

static DWORD WINAPI worker_thread(LPVOID unused) {
    (void)unused;
    while (g_running || g_queue_count > 0) {
        SOCKET client = dequeue_client();
        PB_CLIENT *ctx;
        if (client == INVALID_SOCKET) {
            WaitForSingleObject(g_queue_event, 1000);
            continue;
        }
#if PB_FEAT_DEFENSE
        InterlockedIncrement((LONG *)&g_active_workers);
        PostMessageA(g_main, WM_APP + 4, 0, 0);
#endif
        ctx = (PB_CLIENT *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(PB_CLIENT));
        if (!ctx) {
            closesocket(client);
#if PB_FEAT_DEFENSE
            InterlockedDecrement((LONG *)&g_active_workers);
#endif
            continue;
        }
        ctx->client = client;
#if PB_FEAT_TIMEOUT
        set_client_timeout(client);
#endif
        handle_client(ctx);
        HeapFree(GetProcessHeap(), 0, ctx);
#if PB_FEAT_DEFENSE
        InterlockedDecrement((LONG *)&g_active_workers);
        PostMessageA(g_main, WM_APP + 4, 0, 0);
#endif
    }
    return 0;
}

static void start_workers(void) {
    int i;
#if PB_FEAT_DEFENSE
    for (i = 0; i < g_worker_count; i++) {
#else
    for (i = 0; i < 4; i++) {
#endif
        HANDLE h = CreateThread(0, 0, worker_thread, 0, 0, 0);
        if (h) CloseHandle(h);
    }
}
#endif

static DWORD WINAPI server_thread(LPVOID unused) {
    WSADATA wsa;
    struct sockaddr_in addr;
    (void)unused;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        PostMessageA(g_main, WM_APP + 1, 0, 0);
        return 1;
    }

#if PB_FEAT_IPV6
    if (g_ipv6) {
        struct sockaddr_in6 addr6;
        zero_bytes(&addr6, sizeof(addr6));
        g_listener = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
        if (g_listener == INVALID_SOCKET) {
            WSACleanup();
            PostMessageA(g_main, WM_APP + 1, 0, 0);
            return 1;
        }
        addr6.sin6_family = AF_INET6;
        addr6.sin6_port = htons(g_port);
#if PB_FEAT_BIND_ALL
        if (!g_bind_all)
#endif
        addr6.sin6_addr.u.Byte[15] = 1;
        if (bind(g_listener, (struct sockaddr *)&addr6, sizeof(addr6)) == SOCKET_ERROR ||
            listen(g_listener, SOMAXCONN) == SOCKET_ERROR) {
            closesocket(g_listener);
            g_listener = INVALID_SOCKET;
            WSACleanup();
            PostMessageA(g_main, WM_APP + 1, 0, 0);
            return 1;
        }
    } else
#endif
    {
        g_listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (g_listener == INVALID_SOCKET) {
            WSACleanup();
            PostMessageA(g_main, WM_APP + 1, 0, 0);
            return 1;
        }

        addr.sin_family = AF_INET;
        addr.sin_port = htons(g_port);
#if PB_FEAT_BIND_ALL
        addr.sin_addr.s_addr = htonl(g_bind_all ? INADDR_ANY : 0x7f000001);
#else
        addr.sin_addr.s_addr = htonl(0x7f000001);
#endif

        if (bind(g_listener, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR ||
            listen(g_listener, SOMAXCONN) == SOCKET_ERROR) {
            closesocket(g_listener);
            g_listener = INVALID_SOCKET;
            WSACleanup();
            PostMessageA(g_main, WM_APP + 1, 0, 0);
            return 1;
        }
    }

    InterlockedExchange(&g_running, 1);
#if PB_FEAT_METRICS
    g_started_tick = GetTickCount();
#endif
#if PB_FEAT_WORKERS
    start_workers();
#endif
    PostMessageA(g_main, WM_APP + 2, 0, 0);

    while (g_running) {
        SOCKET client = accept(g_listener, 0, 0);
#if !PB_FEAT_WORKERS
        PB_CLIENT *ctx;
#endif
        if (client == INVALID_SOCKET) break;
#if PB_FEAT_WORKERS
        if (!enqueue_client(client)) send_busy(client);
#else
        ctx = (PB_CLIENT *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(PB_CLIENT));
        if (!ctx) {
            closesocket(client);
            continue;
        }
        ctx->client = client;
#if PB_FEAT_TIMEOUT
        set_client_timeout(client);
#endif
#if PB_FEAT_THREADS
        {
            HANDLE worker = CreateThread(0, 0, client_thread, ctx, 0, 0);
            if (worker) {
                CloseHandle(worker);
            } else {
                handle_client(ctx);
                HeapFree(GetProcessHeap(), 0, ctx);
            }
        }
#else
        handle_client(ctx);
        HeapFree(GetProcessHeap(), 0, ctx);
#endif
#endif
    }

    if (g_listener != INVALID_SOCKET) {
        closesocket(g_listener);
        g_listener = INVALID_SOCKET;
    }
    WSACleanup();
    InterlockedExchange(&g_running, 0);
    PostMessageA(g_main, WM_APP + 3, 0, 0);
    return 0;
}

static void start_server(void) {
    char port_text[16];
    unsigned short port;
#if PB_FEAT_DEFENSE
    char limit_text[16];
    unsigned short limit;
#endif

    if (g_running) return;

#if PB_FEAT_CONFIG
    if (g_config_error) {
        set_status("Invalid config.");
        return;
    }
#endif

    GetWindowTextA(g_port_edit, port_text, sizeof(port_text));
    port = parse_port(port_text);
    if (!port) {
        set_status("Invalid port.");
        return;
    }
    g_port = port;
#if PB_FEAT_DEFENSE
    GetWindowTextA(g_timeout_edit, limit_text, sizeof(limit_text));
    limit = parse_port(limit_text);
    if (limit < 1000 || limit > 30000) {
        set_status("Timeout must be 1000-30000 ms.");
        return;
    }
    g_timeout_ms = limit;
    GetWindowTextA(g_workers_edit, limit_text, sizeof(limit_text));
    limit = parse_port(limit_text);
    if (limit < 1 || limit > PB_WORKER_MAX) {
        set_status("Workers must be 1-8.");
        return;
    }
    g_worker_count = limit;
    GetWindowTextA(g_queue_edit, limit_text, sizeof(limit_text));
    limit = parse_port(limit_text);
    if (limit < 1 || limit > PB_QUEUE_MAX) {
        set_status("Queue must be 1-16.");
        return;
    }
    g_queue_limit = limit;
#endif
    InterlockedExchange((LONG *)&g_request_count, 0);
    InterlockedExchange((LONG *)&g_last_status, 0);
#if PB_FEAT_DEFENSE
    InterlockedExchange((LONG *)&g_active_workers, 0);
    InterlockedExchange((LONG *)&g_rejected_clients, 0);
    InterlockedExchange((LONG *)&g_timeout_clients, 0);
#endif
#if PB_FEAT_METRICS
    InterlockedExchange((LONG *)&g_bytes_served, 0);
#endif
#if PB_FEAT_LOG
    InterlockedExchange((LONG *)&g_last_bytes, 0);
    g_log_chars = 0;
#endif
#if PB_FEAT_METRICS
    g_started_tick = 0;
#endif
#if PB_FEAT_WORKERS
    EnterCriticalSection(&g_queue_lock);
    g_queue_head = 0;
    g_queue_tail = 0;
    g_queue_count = 0;
    LeaveCriticalSection(&g_queue_lock);
#endif
    lstrcpyA(g_last_path, "");
#if PB_FEAT_LOG
    SetWindowTextA(g_activity, "");
#else
    refresh_activity();
#endif

    GetWindowTextA(g_root_edit, g_root, sizeof(g_root));
    if (!g_root[0]) {
        set_status("Choose a root directory.");
        return;
    }
    if (!set_root_full()) {
        set_status("Root directory not found.");
        return;
    }

    g_thread = CreateThread(0, 0, server_thread, 0, 0, 0);
    if (!g_thread) {
        set_status("Could not start server thread.");
#if PB_FEAT_METRICS
        SetWindowTextA(g_main, "PortBlaster - Start failed");
#endif
    } else {
#if PB_FEAT_METRICS
        SetWindowTextA(g_main, "PortBlaster - Starting");
#endif
        set_status("Starting...");
        CloseHandle(g_thread);
        g_thread = 0;
    }
}

static void stop_server(void) {
    InterlockedExchange(&g_running, 0);
    if (g_listener != INVALID_SOCKET) {
        closesocket(g_listener);
    }
#if PB_FEAT_WORKERS
    SetEvent(g_queue_event);
#endif
#if PB_FEAT_METRICS
    SetWindowTextA(g_main, "PortBlaster - Stopping");
#endif
    set_status("Stopping...");
}

static void set_running_ui(int running) {
    EnableWindow(g_start_button, !running);
    EnableWindow(g_stop_button, running);
#if PB_FEAT_COPY_URL
    EnableWindow(g_copy_button, running);
#endif
    EnableWindow(g_port_edit, !running);
    EnableWindow(g_root_edit, !running);
#if PB_FEAT_BROWSE
    EnableWindow(g_browse_button, !running);
#endif
#if PB_FEAT_DEFENSE
    EnableWindow(g_timeout_edit, !running);
    EnableWindow(g_workers_edit, !running);
    EnableWindow(g_queue_edit, !running);
#endif
}

#if PB_FEAT_TRAY
static void tray_remove(void) {
    if (g_tray_added) {
        Shell_NotifyIconA(NIM_DELETE, &g_tray);
        g_tray_added = 0;
    }
}

static void tray_add(HWND hwnd) {
    if (g_tray_added) return;
    zero_bytes(&g_tray, sizeof(g_tray));
    g_tray.cbSize = sizeof(g_tray);
    g_tray.hWnd = hwnd;
    g_tray.uID = 1;
    g_tray.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_tray.uCallbackMessage = WM_TRAY;
    g_tray.hIcon = LoadIconA(0, (LPCSTR)32512);
    lstrcpynA(g_tray.szTip, g_running ? g_url : "PortBlaster - Stopped", sizeof(g_tray.szTip));
    if (Shell_NotifyIconA(NIM_ADD, &g_tray)) g_tray_added = 1;
}

static void restore_window(HWND hwnd) {
    ShowWindow(hwnd, SW_SHOW);
    ShowWindow(hwnd, SW_RESTORE);
    SetForegroundWindow(hwnd);
    tray_remove();
}

static void tray_menu(HWND hwnd) {
    POINT pt;
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    AppendMenuA(menu, MF_STRING, ID_TRAY_RESTORE, "Open");
#if PB_FEAT_COPY_URL
    AppendMenuA(menu, g_running ? MF_STRING : MF_GRAYED, ID_TRAY_COPY, "Copy URL");
#endif
    AppendMenuA(menu, g_running ? MF_STRING : MF_GRAYED, ID_TRAY_STOP, "Stop");
    AppendMenuA(menu, MF_STRING, ID_TRAY_EXIT, "Exit");
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, 0);
    DestroyMenu(menu);
}
#endif

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
    {
        char port_text[16];
#if PB_FEAT_DEFENSE
        char safety_text[16];
#endif
        g_main = hwnd;
        set_default_root();
#if PB_FEAT_CONFIG
        load_config();
#endif
        wsprintfA(port_text, "%u", (unsigned int)g_port);
        CreateWindowA("STATIC", "Port", WS_CHILD | WS_VISIBLE, 12, 14, 60, 22, hwnd, 0, 0, 0);
        g_port_edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", port_text, WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 82, 12, 80, 24, hwnd, (HMENU)ID_PORT, 0, 0);
        CreateWindowA("STATIC", "Root", WS_CHILD | WS_VISIBLE, 12, 48, 60, 22, hwnd, 0, 0, 0);
#if PB_FEAT_ACCESS_LOG
        set_access_log_path();
#endif
#if PB_FEAT_BROWSE
        g_root_edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", g_root, WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 82, 46, 270, 24, hwnd, (HMENU)ID_ROOT, 0, 0);
        g_browse_button = CreateWindowA("BUTTON", "Browse", WS_CHILD | WS_VISIBLE, 362, 46, 80, 24, hwnd, (HMENU)ID_BROWSE, 0, 0);
#else
        g_root_edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", g_root, WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 82, 46, 360, 24, hwnd, (HMENU)ID_ROOT, 0, 0);
#endif
        g_start_button = CreateWindowA("BUTTON", "Start", WS_CHILD | WS_VISIBLE, 82, 84, 90, 28, hwnd, (HMENU)ID_START, 0, 0);
        g_stop_button = CreateWindowA("BUTTON", "Stop", WS_CHILD | WS_VISIBLE, 182, 84, 90, 28, hwnd, (HMENU)ID_STOP, 0, 0);
#if PB_FEAT_COPY_URL
        g_copy_button = CreateWindowA("BUTTON", "Copy URL", WS_CHILD | WS_VISIBLE, 282, 84, 90, 28, hwnd, (HMENU)ID_COPY_URL, 0, 0);
#endif
        g_status = CreateWindowA("STATIC", "Stopped.", WS_CHILD | WS_VISIBLE, 12, 126, 430, 24, hwnd, (HMENU)ID_STATUS, 0, 0);
#if PB_FEAT_LOG
        g_activity = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL, 12, 152, 430, 78, hwnd, (HMENU)ID_ACTIVITY, 0, 0);
#else
        g_activity = CreateWindowA("STATIC", "Requests: 0 | Last: none", WS_CHILD | WS_VISIBLE, 12, 152, 430, 24, hwnd, (HMENU)ID_ACTIVITY, 0, 0);
#endif
#if PB_FEAT_DEFENSE
        CreateWindowA("BUTTON", "Safety limits", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 12, 238, 430, 92, hwnd, 0, 0, 0);
        CreateWindowA("STATIC", "Timeout ms", WS_CHILD | WS_VISIBLE, 24, 260, 72, 20, hwnd, 0, 0, 0);
        wsprintfA(safety_text, "%d", g_timeout_ms);
        g_timeout_edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", safety_text, WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 96, 258, 58, 22, hwnd, (HMENU)ID_TIMEOUT, 0, 0);
        CreateWindowA("STATIC", "Workers", WS_CHILD | WS_VISIBLE, 168, 260, 52, 20, hwnd, 0, 0, 0);
        wsprintfA(safety_text, "%d", g_worker_count);
        g_workers_edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", safety_text, WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 220, 258, 38, 22, hwnd, (HMENU)ID_WORKERS, 0, 0);
        CreateWindowA("STATIC", "Queue", WS_CHILD | WS_VISIBLE, 274, 260, 42, 20, hwnd, 0, 0, 0);
        wsprintfA(safety_text, "%d", g_queue_limit);
        g_queue_edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", safety_text, WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 316, 258, 38, 22, hwnd, (HMENU)ID_QUEUE, 0, 0);
        g_defense_status = CreateWindowA("STATIC", "Active 0 | Queue 0/16 | Rejected 0 | Timeouts 0", WS_CHILD | WS_VISIBLE, 24, 294, 400, 22, hwnd, (HMENU)ID_DEFENSE, 0, 0);
#endif
        set_running_ui(0);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == ID_START) start_server();
        if (LOWORD(wp) == ID_STOP) stop_server();
#if PB_FEAT_TRAY
        if (LOWORD(wp) == ID_TRAY_RESTORE) restore_window(hwnd);
        if (LOWORD(wp) == ID_TRAY_STOP) stop_server();
        if (LOWORD(wp) == ID_TRAY_EXIT) {
            stop_server();
            tray_remove();
            DestroyWindow(hwnd);
        }
#endif
#if PB_FEAT_BROWSE
        if (LOWORD(wp) == ID_BROWSE) browse_root();
#endif
#if PB_FEAT_COPY_URL
        if (LOWORD(wp) == ID_COPY_URL) copy_current_url();
#if PB_FEAT_TRAY
        if (LOWORD(wp) == ID_TRAY_COPY) copy_current_url();
#endif
#endif
        return 0;
    case WM_APP + 1:
        set_status("Start failed. Port may be in use.");
#if PB_FEAT_METRICS
        SetWindowTextA(g_main, "PortBlaster - Start failed");
#endif
        goto not_running;
    case WM_APP + 2:
        set_running_status();
        set_running_ui(1);
#if PB_FEAT_JELLY && PB_FEAT_COPY_URL
        if (contains(GetCommandLineA(), "--copy-url-test")) copy_current_url();
#endif
        return 0;
    case WM_APP + 3:
        set_status("Stopped.");
#if PB_FEAT_METRICS
        SetWindowTextA(g_main, "PortBlaster - Stopped");
#endif
not_running:
#if PB_FEAT_METRICS
        KillTimer(hwnd, 1);
        g_started_tick = 0;
#endif
#if PB_FEAT_TRAY
        if (g_tray_added) {
            lstrcpynA(g_tray.szTip, "PortBlaster - Stopped", sizeof(g_tray.szTip));
            Shell_NotifyIconA(NIM_MODIFY, &g_tray);
        }
#endif
        set_running_ui(0);
        refresh_activity();
        return 0;
#if PB_FEAT_TRAY
    case WM_SIZE:
        if (wp == SIZE_MINIMIZED) {
            tray_add(hwnd);
            ShowWindow(hwnd, SW_HIDE);
        }
        return 0;
    case WM_TRAY:
        if (lp == WM_LBUTTONDBLCLK) restore_window(hwnd);
        if (lp == WM_RBUTTONUP) tray_menu(hwnd);
        return 0;
#endif
    case WM_APP + 4:
#if PB_FEAT_LOG
        append_log();
#endif
        refresh_activity();
        return 0;
#if PB_FEAT_METRICS
    case WM_TIMER:
        if (wp == 1) refresh_activity();
        return 0;
#endif
    case WM_CLOSE:
        stop_server();
#if PB_FEAT_TRAY
        tray_remove();
#endif
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
#if PB_FEAT_TRAY
        tray_remove();
#endif
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

void WinMainCRTStartup(void) {
    WNDCLASSA wc;
    HWND hwnd;
    MSG msg;
    HINSTANCE inst = GetModuleHandleA(0);

#if PB_FEAT_THREADS || PB_FEAT_WORKERS
    InitializeCriticalSection(&g_note_lock);
#endif
#if PB_FEAT_WORKERS
    InitializeCriticalSection(&g_queue_lock);
    g_queue_event = CreateEventA(0, FALSE, FALSE, 0);
#endif
    zero_bytes(&wc, sizeof(wc));
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.lpszClassName = "portblaster_window";
    wc.hCursor = LoadCursorA(0, (LPCSTR)32512);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClassA(&wc);

    hwnd = CreateWindowExA(0, "portblaster_window", "PortBlaster - Stopped", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 470, PB_FEAT_DEFENSE ? 380 : (PB_FEAT_LOG ? 285 : 235), 0, 0, inst, 0);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

#if PB_FEAT_JELLY
    if (contains(GetCommandLineA(), "--start")) {
        PostMessageA(hwnd, WM_COMMAND, ID_START, 0);
    }
#endif

    while (GetMessageA(&msg, 0, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    ExitProcess(0);
}
