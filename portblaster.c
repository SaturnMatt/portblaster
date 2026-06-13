#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

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

#define REQ_MAX 1024
#define ROOT_MAX 384
#define PATH_MAX_LOCAL 768
#define HEADER_MAX 512
#define FILE_MAX (1024 * 1024)
#define STREAM_CHUNK 8192

static HWND g_port_edit;
static HWND g_root_edit;
static HWND g_start_button;
static HWND g_stop_button;
static HWND g_status;
static HWND g_activity;
static HWND g_main;

static SOCKET g_listener = INVALID_SOCKET;
static HANDLE g_thread = 0;
static volatile LONG g_running = 0;
static volatile LONG g_request_count = 0;
static volatile LONG g_last_status = 0;
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
static char g_dir_body[HEADER_MAX];
#endif
#if PB_FEAT_BIND_ALL
static int g_bind_all = 0;
#endif
#if PB_FEAT_IPV6
static int g_ipv6 = 0;
#endif
static unsigned short g_port = 8083;

static char g_request[REQ_MAX];
static char g_path[PATH_MAX_LOCAL];
static char g_header[HEADER_MAX];
#if PB_FEAT_STATUS_ENDPOINT
static char g_status_body[HEADER_MAX];
#endif
#if PB_FEAT_STREAM
static unsigned char g_chunk[STREAM_CHUNK];
#else
static unsigned char g_file[FILE_MAX];
#endif

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
static void append_log_file(int status, DWORD bytes) {
    HANDLE f;
    DWORD wrote;
    int p = 0;
    append_u32(g_header, &p, (DWORD)status);
    append(g_header, &p, " ");
    append(g_header, &p, g_last_path);
    append(g_header, &p, " ");
    append_u32(g_header, &p, bytes);
    append(g_header, &p, " B\r\n");
    f = CreateFileA(g_access_log_path, FILE_APPEND_DATA, FILE_SHARE_READ, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (f == INVALID_HANDLE_VALUE) return;
    WriteFile(f, g_header, (DWORD)p, &wrote, 0);
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

static void copy_url_token(const char *url) {
    int p = 0;
    while (*url && *url != ' ' && *url != '?' && p < (int)sizeof(g_last_path) - 1) {
        g_last_path[p++] = *url++;
    }
    if (p == 0 || (p == 1 && g_last_path[0] == '/')) {
        g_last_path[0] = '/';
        g_last_path[1] = 0;
    } else {
        g_last_path[p] = 0;
    }
}

static void note_request(int status, DWORD bytes) {
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
#if PB_FEAT_ACCESS_LOG
    append_log_file(status, bytes);
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

static int make_path(const char *url) {
    int p = 0;
    int i = 0;
    int root_len = lstrlenA(g_root_full);
    char full[PATH_MAX_LOCAL];
    DWORD full_len;

    if (root_len <= 0 || bad_path(url)) return 0;

    while (g_root_full[i] && p < PATH_MAX_LOCAL - 2) {
        char c = g_root_full[i++];
        g_path[p++] = c;
    }
    if (p > 0 && g_path[p - 1] != '\\' && g_path[p - 1] != '/') {
        g_path[p++] = '\\';
    }

    if (url[0] == '/' && (url[1] == ' ' || url[1] == '?' || url[1] == 0)) {
        url = "/index.html";
    }
    if (*url == '/') url++;

    while (*url && *url != ' ' && *url != '?' && p < PATH_MAX_LOCAL - 1) {
        char c = *url++;
        g_path[p++] = (c == '/') ? '\\' : c;
    }
    if (*url && *url != ' ' && *url != '?') return 0;
    g_path[p] = 0;

    full_len = GetFullPathNameA(g_path, sizeof(full), full, 0);
    if (full_len == 0 || full_len >= sizeof(full) || !path_under_root(full)) return 0;
    lstrcpyA(g_path, full);
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
    DWORD timeout = 5000;
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
    setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof(timeout));
}
#endif

static void send_response(SOCKET client, int status, const char *reason, const char *type, const unsigned char *body, DWORD body_len, int head_only) {
    int p = 0;
    append(g_header, &p, "HTTP/1.1 ");
    append_u32(g_header, &p, (DWORD)status);
    append(g_header, &p, " ");
    append(g_header, &p, reason);
    append(g_header, &p, "\r\nContent-Type: ");
    append(g_header, &p, type);
    append(g_header, &p, "\r\nContent-Length: ");
    append_u32(g_header, &p, body_len);
    append(g_header, &p, "\r\nConnection: close\r\n\r\n");
    send_all(client, g_header, (DWORD)p);
    if (!head_only && body_len) {
        send_all(client, (const char *)body, body_len);
    }
}

#if PB_FEAT_RANGE
static int parse_range(DWORD file_len, DWORD *start, DWORD *end) {
    const char *p = find_text(g_request, "\r\nRange: bytes=");
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

static void send_range_bad(SOCKET client, DWORD file_len) {
    int p = 0;
    append(g_header, &p, "HTTP/1.1 416 Range Not Satisfiable\r\nContent-Range: bytes */");
    append_u32(g_header, &p, file_len);
    append(g_header, &p, "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
    send_all(client, g_header, (DWORD)p);
}

static int send_file_range(SOCKET client, HANDLE file, DWORD file_len, DWORD start, DWORD end, const char *type, int head_only) {
    int p = 0;
    DWORD left = end - start + 1;
    DWORD read_count;
    SetFilePointer(file, (LONG)start, 0, FILE_BEGIN);
    append(g_header, &p, "HTTP/1.1 206 Partial Content\r\nContent-Type: ");
    append(g_header, &p, type);
    append(g_header, &p, "\r\nContent-Range: bytes ");
    append_u32(g_header, &p, start);
    append(g_header, &p, "-");
    append_u32(g_header, &p, end);
    append(g_header, &p, "/");
    append_u32(g_header, &p, file_len);
    append(g_header, &p, "\r\nContent-Length: ");
    append_u32(g_header, &p, left);
    append(g_header, &p, "\r\nConnection: close\r\n\r\n");
    send_all(client, g_header, (DWORD)p);
    if (head_only) return 1;
    while (left) {
        DWORD want = left > STREAM_CHUNK ? STREAM_CHUNK : left;
        if (!ReadFile(file, g_chunk, want, &read_count, 0) || read_count == 0) return 0;
        send_all(client, (const char *)g_chunk, read_count);
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

static void send_directory_listing(SOCKET client, int head_only) {
    WIN32_FIND_DATAA fd;
    HANDLE find;
    char pattern[PATH_MAX_LOCAL];
    int p = 0;
    append(g_dir_body, &p, "<!doctype html><title>Index</title><h1>Index</h1><ul>");
    lstrcpyA(pattern, g_path);
    lstrcatA(pattern, "\\*");
    find = FindFirstFileA(pattern, &fd);
    if (find != INVALID_HANDLE_VALUE) {
        do {
            if (fd.cFileName[0] == '.') continue;
            if (fd.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) continue;
            append(g_dir_body, &p, "<li><a href=\"");
            append_html_name(g_dir_body, &p, fd.cFileName);
            append(g_dir_body, &p, "\">");
            append_html_name(g_dir_body, &p, fd.cFileName);
            append(g_dir_body, &p, "</a></li>");
        } while (FindNextFileA(find, &fd));
        FindClose(find);
    }
    append(g_dir_body, &p, "</ul>");
    send_response(client, 200, "OK", "text/html; charset=utf-8", (const unsigned char *)g_dir_body, (DWORD)p, head_only);
    note_request(200, head_only ? 0 : (DWORD)p);
}
#endif

#if PB_FEAT_STATUS_ENDPOINT
static void send_status_endpoint(SOCKET client, int head_only) {
    int p = 0;
    DWORD up = g_started_tick ? (GetTickCount() - g_started_tick) / 1000 : 0;
    append(g_status_body, &p, "target=");
    append_u32(g_status_body, &p, PB_TARGET_KB);
    append(g_status_body, &p, "\nrequests=");
    append_u32(g_status_body, &p, (DWORD)g_request_count);
    append(g_status_body, &p, "\nbytes=");
    append_u32(g_status_body, &p, (DWORD)g_bytes_served);
    append(g_status_body, &p, "\nuptime=");
    append_u32(g_status_body, &p, up);
    append(g_status_body, &p, "\nfeatures=LOG,METRICS,STREAM,MIME_PLUS,TIMEOUT,STATUS_ENDPOINT\n");
    send_response(client, 200, "OK", "text/plain; charset=utf-8", (const unsigned char *)g_status_body, (DWORD)p, head_only);
    note_request(200, head_only ? 0 : (DWORD)p);
}
#endif

#if PB_FEAT_STREAM
static int send_file_response(SOCKET client, HANDLE file, DWORD body_len, const char *type, int head_only) {
    int p = 0;
    DWORD left = body_len;
    DWORD read_count;
    append(g_header, &p, "HTTP/1.1 200 OK\r\nContent-Type: ");
    append(g_header, &p, type);
    append(g_header, &p, "\r\nContent-Length: ");
    append_u32(g_header, &p, body_len);
    append(g_header, &p, "\r\nConnection: close\r\n\r\n");
    send_all(client, g_header, (DWORD)p);
    if (head_only) return 1;
    while (left) {
        DWORD want = left > STREAM_CHUNK ? STREAM_CHUNK : left;
        if (!ReadFile(file, g_chunk, want, &read_count, 0) || read_count == 0) return 0;
        send_all(client, (const char *)g_chunk, read_count);
        left -= read_count;
    }
    return 1;
}
#endif

static void handle_client(SOCKET client) {
    int n = recv(client, g_request, REQ_MAX - 1, 0);
    int head_only = 0;
    const char *url;
    HANDLE file;
    DWORD size_low;
    DWORD attr;
#if !PB_FEAT_STREAM
    DWORD read_count;
#endif

    if (n <= 0) {
        closesocket(client);
        return;
    }
    g_request[n] = 0;

    if (starts_with(g_request, "HEAD ")) {
        head_only = 1;
        url = g_request + 5;
    } else if (!starts_with(g_request, "GET ")) {
        static const unsigned char msg[] = "Method Not Allowed\n";
        send_response(client, 405, "Method Not Allowed", "text/plain; charset=utf-8", msg, sizeof(msg) - 1, 0);
        lstrcpyA(g_last_path, "(method)");
        note_request(405, 0);
        closesocket(client);
        return;
    } else {
        url = g_request + 4;
    }

    copy_url_token(url);
    if (!good_request_line(url)) {
        static const unsigned char msg[] = "Bad Request\n";
        send_response(client, 400, "Bad Request", "text/plain; charset=utf-8", msg, sizeof(msg) - 1, 0);
        note_request(400, 0);
        closesocket(client);
        return;
    }

#if PB_FEAT_STATUS_ENDPOINT
    if (token_equals(url, "/__pb/status")) {
        send_status_endpoint(client, head_only);
        closesocket(client);
        return;
    }
#endif

    if (!make_path(url)) {
        static const unsigned char msg[] = "Forbidden\n";
        send_response(client, 403, "Forbidden", "text/plain; charset=utf-8", msg, sizeof(msg) - 1, 0);
        note_request(403, 0);
        closesocket(client);
        return;
    }

    attr = GetFileAttributesA(g_path);
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY)) {
#if PB_FEAT_DIR_LIST
        if (g_dir_list) {
            send_directory_listing(client, head_only);
            closesocket(client);
            return;
        }
#endif
        {
            static const unsigned char msg[] = "Not Found\n";
            send_response(client, 404, "Not Found", "text/plain; charset=utf-8", msg, sizeof(msg) - 1, 0);
            note_request(404, 0);
            closesocket(client);
            return;
        }
    }

    file = CreateFileA(g_path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (file == INVALID_HANDLE_VALUE) {
        static const unsigned char msg[] = "Not Found\n";
        send_response(client, 404, "Not Found", "text/plain; charset=utf-8", msg, sizeof(msg) - 1, 0);
        note_request(404, 0);
        closesocket(client);
        return;
    }

    size_low = GetFileSize(file, 0);
#if PB_FEAT_STREAM
    if (size_low == INVALID_FILE_SIZE) {
        static const unsigned char msg[] = "Read Error\n";
        CloseHandle(file);
        send_response(client, 500, "Internal Server Error", "text/plain; charset=utf-8", msg, sizeof(msg) - 1, 0);
        note_request(500, 0);
        closesocket(client);
        return;
    }

    {
#if PB_FEAT_RANGE
        DWORD range_start = 0;
        DWORD range_end = 0;
        int range = parse_range(size_low, &range_start, &range_end);
        if (range < 0) {
            send_range_bad(client, size_low);
            CloseHandle(file);
            note_request(416, 0);
            closesocket(client);
            return;
        }
        if (range > 0) {
            if (!send_file_range(client, file, size_low, range_start, range_end, type_for(g_path), head_only)) {
                CloseHandle(file);
                note_request(500, 0);
                closesocket(client);
                return;
            }
            CloseHandle(file);
            note_request(206, head_only ? 0 : range_end - range_start + 1);
            closesocket(client);
            return;
        }
#endif
    }
    if (!send_file_response(client, file, size_low, type_for(g_path), head_only)) {
        CloseHandle(file);
        note_request(500, 0);
        closesocket(client);
        return;
    }

    CloseHandle(file);
    note_request(200, head_only ? 0 : size_low);
    closesocket(client);
    return;
#else
    if (size_low == INVALID_FILE_SIZE || size_low > FILE_MAX) {
        static const unsigned char msg[] = "File Too Large\n";
        CloseHandle(file);
        send_response(client, 413, "Payload Too Large", "text/plain; charset=utf-8", msg, sizeof(msg) - 1, 0);
        note_request(413, 0);
        closesocket(client);
        return;
    }

    if (!ReadFile(file, g_file, size_low, &read_count, 0) || read_count != size_low) {
        static const unsigned char msg[] = "Read Error\n";
        CloseHandle(file);
        send_response(client, 500, "Internal Server Error", "text/plain; charset=utf-8", msg, sizeof(msg) - 1, 0);
        note_request(500, 0);
        closesocket(client);
        return;
    }

    CloseHandle(file);
    send_response(client, 200, "OK", type_for(g_path), g_file, size_low, head_only);
    note_request(200, head_only ? 0 : size_low);
    closesocket(client);
#endif
}

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
    PostMessageA(g_main, WM_APP + 2, 0, 0);

    while (g_running) {
        SOCKET client = accept(g_listener, 0, 0);
        if (client == INVALID_SOCKET) break;
#if PB_FEAT_TIMEOUT
        set_client_timeout(client);
#endif
        handle_client(client);
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
    InterlockedExchange((LONG *)&g_request_count, 0);
    InterlockedExchange((LONG *)&g_last_status, 0);
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
#if PB_FEAT_METRICS
    SetWindowTextA(g_main, "PortBlaster - Stopping");
#endif
    set_status("Stopping...");
}

static void set_running_ui(int running) {
    EnableWindow(g_start_button, !running);
    EnableWindow(g_stop_button, running);
    EnableWindow(g_port_edit, !running);
    EnableWindow(g_root_edit, !running);
}

static LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    (void)lp;
    switch (msg) {
    case WM_CREATE:
    {
        char port_text[16];
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
        g_root_edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", g_root, WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 82, 46, 360, 24, hwnd, (HMENU)ID_ROOT, 0, 0);
        g_start_button = CreateWindowA("BUTTON", "Start", WS_CHILD | WS_VISIBLE, 82, 84, 90, 28, hwnd, (HMENU)ID_START, 0, 0);
        g_stop_button = CreateWindowA("BUTTON", "Stop", WS_CHILD | WS_VISIBLE, 182, 84, 90, 28, hwnd, (HMENU)ID_STOP, 0, 0);
        g_status = CreateWindowA("STATIC", "Stopped.", WS_CHILD | WS_VISIBLE, 12, 126, 430, 24, hwnd, (HMENU)ID_STATUS, 0, 0);
#if PB_FEAT_LOG
        g_activity = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL, 12, 152, 430, 78, hwnd, (HMENU)ID_ACTIVITY, 0, 0);
#else
        g_activity = CreateWindowA("STATIC", "Requests: 0 | Last: none", WS_CHILD | WS_VISIBLE, 12, 152, 430, 24, hwnd, (HMENU)ID_ACTIVITY, 0, 0);
#endif
        set_running_ui(0);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == ID_START) start_server();
        if (LOWORD(wp) == ID_STOP) stop_server();
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
        set_running_ui(0);
        refresh_activity();
        return 0;
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
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
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

    zero_bytes(&wc, sizeof(wc));
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = inst;
    wc.lpszClassName = "portblaster_window";
    wc.hCursor = LoadCursorA(0, (LPCSTR)32512);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClassA(&wc);

    hwnd = CreateWindowExA(0, "portblaster_window", "PortBlaster - Stopped", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 470, PB_FEAT_LOG ? 285 : 235, 0, 0, inst, 0);

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
