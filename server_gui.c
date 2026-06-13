#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>

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
static volatile LONG g_bytes_served = 0;
static volatile LONG g_last_bytes = 0;
static LONG g_log_chars = 0;
static DWORD g_started_tick = 0;
static char g_root[ROOT_MAX];
static char g_root_full[PATH_MAX_LOCAL];
static char g_url[64];
static char g_activity_text[256];
static char g_last_path[128];
static unsigned short g_port = 8083;

static char g_request[REQ_MAX];
static char g_path[PATH_MAX_LOCAL];
static char g_header[HEADER_MAX];
static unsigned char g_file[FILE_MAX];

static void refresh_activity(void);

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

static void set_running_status(void) {
    wsprintfA(g_url, "http://127.0.0.1:%u/", (unsigned int)g_port);
    wsprintfA(g_activity_text, "PortBlaster - Running :%u", (unsigned int)g_port);
    SetWindowTextA(g_main, g_activity_text);
    SetTimer(g_main, 1, 1000, 0);
    refresh_activity();
}

static void refresh_activity(void) {
    LONG count = g_request_count;
    LONG status = g_last_status;
    LONG bytes = g_bytes_served;
    DWORD up = g_started_tick ? (GetTickCount() - g_started_tick) / 1000 : 0;
    DWORD h = up / 3600;
    DWORD m = (up / 60) % 60;
    DWORD s = up % 60;
    if (g_running) {
        wsprintfA(g_activity_text, "%s | Req: %ld | Bytes: %ld | Up: %02lu:%02lu:%02lu", g_url, count, bytes, h, m, s);
        set_status(g_activity_text);
    }
}

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

static int starts_with(const char *s, const char *prefix) {
    while (*prefix) {
        if (*s++ != *prefix++) return 0;
    }
    return 1;
}

#ifdef PORTBLASTER_CHECK
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
    InterlockedExchange((LONG *)&g_last_bytes, (LONG)bytes);
    if (bytes) InterlockedExchangeAdd((LONG *)&g_bytes_served, (LONG)bytes);
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

static void handle_client(SOCKET client) {
    int n = recv(client, g_request, REQ_MAX - 1, 0);
    int head_only = 0;
    const char *url;
    HANDLE file;
    DWORD size_low;
    DWORD read_count;

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

    if (!make_path(url)) {
        static const unsigned char msg[] = "Forbidden\n";
        send_response(client, 403, "Forbidden", "text/plain; charset=utf-8", msg, sizeof(msg) - 1, 0);
        note_request(403, 0);
        closesocket(client);
        return;
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
    if (size_low == INVALID_FILE_SIZE || size_low > FILE_MAX) {
        static const unsigned char msg[] = "File Too Large\n";
        CloseHandle(file);
        send_response(client, 500, "Internal Server Error", "text/plain; charset=utf-8", msg, sizeof(msg) - 1, 0);
        note_request(500, 0);
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
}

static DWORD WINAPI server_thread(LPVOID unused) {
    WSADATA wsa;
    struct sockaddr_in addr;
    (void)unused;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        PostMessageA(g_main, WM_APP + 1, 0, 0);
        return 1;
    }

    g_listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_listener == INVALID_SOCKET) {
        WSACleanup();
        PostMessageA(g_main, WM_APP + 1, 0, 0);
        return 1;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(g_port);
    addr.sin_addr.s_addr = htonl(0x7f000001);

    if (bind(g_listener, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR ||
        listen(g_listener, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(g_listener);
        g_listener = INVALID_SOCKET;
        WSACleanup();
        PostMessageA(g_main, WM_APP + 1, 0, 0);
        return 1;
    }

    InterlockedExchange(&g_running, 1);
    g_started_tick = GetTickCount();
    PostMessageA(g_main, WM_APP + 2, 0, 0);

    while (g_running) {
        SOCKET client = accept(g_listener, 0, 0);
        if (client == INVALID_SOCKET) break;
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

    GetWindowTextA(g_port_edit, port_text, sizeof(port_text));
    port = parse_port(port_text);
    if (!port) {
        set_status("Invalid port.");
        return;
    }
    g_port = port;
    InterlockedExchange((LONG *)&g_request_count, 0);
    InterlockedExchange((LONG *)&g_last_status, 0);
    InterlockedExchange((LONG *)&g_bytes_served, 0);
    InterlockedExchange((LONG *)&g_last_bytes, 0);
    g_log_chars = 0;
    g_started_tick = 0;
    lstrcpyA(g_last_path, "");
    SetWindowTextA(g_activity, "");

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
        SetWindowTextA(g_main, "PortBlaster - Start failed");
    } else {
        SetWindowTextA(g_main, "PortBlaster - Starting");
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
    SetWindowTextA(g_main, "PortBlaster - Stopping");
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
        g_main = hwnd;
        CreateWindowA("STATIC", "Port", WS_CHILD | WS_VISIBLE, 12, 14, 60, 22, hwnd, 0, 0, 0);
        g_port_edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "8083", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 82, 12, 80, 24, hwnd, (HMENU)ID_PORT, 0, 0);
        CreateWindowA("STATIC", "Root", WS_CHILD | WS_VISIBLE, 12, 48, 60, 22, hwnd, 0, 0, 0);
        set_default_root();
        g_root_edit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", g_root, WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 82, 46, 360, 24, hwnd, (HMENU)ID_ROOT, 0, 0);
        g_start_button = CreateWindowA("BUTTON", "Start", WS_CHILD | WS_VISIBLE, 82, 84, 90, 28, hwnd, (HMENU)ID_START, 0, 0);
        g_stop_button = CreateWindowA("BUTTON", "Stop", WS_CHILD | WS_VISIBLE, 182, 84, 90, 28, hwnd, (HMENU)ID_STOP, 0, 0);
        g_status = CreateWindowA("STATIC", "Stopped.", WS_CHILD | WS_VISIBLE, 12, 126, 430, 24, hwnd, (HMENU)ID_STATUS, 0, 0);
        g_activity = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | WS_VSCROLL, 12, 152, 430, 78, hwnd, (HMENU)ID_ACTIVITY, 0, 0);
        set_running_ui(0);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wp) == ID_START) start_server();
        if (LOWORD(wp) == ID_STOP) stop_server();
        return 0;
    case WM_APP + 1:
        set_status("Start failed. Port may be in use.");
        SetWindowTextA(g_main, "PortBlaster - Start failed");
        goto not_running;
    case WM_APP + 2:
        set_running_status();
        set_running_ui(1);
        return 0;
    case WM_APP + 3:
        set_status("Stopped.");
        SetWindowTextA(g_main, "PortBlaster - Stopped");
not_running:
        KillTimer(hwnd, 1);
        g_started_tick = 0;
        set_running_ui(0);
        refresh_activity();
        return 0;
    case WM_APP + 4:
        append_log();
        refresh_activity();
        return 0;
    case WM_TIMER:
        if (wp == 1) refresh_activity();
        return 0;
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
        CW_USEDEFAULT, CW_USEDEFAULT, 470, 285, 0, 0, inst, 0);

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

#ifdef PORTBLASTER_CHECK
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
