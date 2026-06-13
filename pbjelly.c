#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>

#define RECV_MAX 8192
#define REQ_MAX 1024

static char g_recv[RECV_MAX];
static char g_req[REQ_MAX];
static char g_host[128] = "127.0.0.1";
static unsigned short g_port = 8083;
static int g_fail = 0;
static HANDLE g_report = INVALID_HANDLE_VALUE;

static int same(const char *a, const char *b) {
    while (*a && *b && *a == *b) {
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

static unsigned short parse_port(const char *s) {
    DWORD v = 0;
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (DWORD)(*s++ - '0');
    }
    if (*s || v < 1 || v > 65535) return 0;
    return (unsigned short)v;
}

static void print_u32(DWORD v) {
    char s[16];
    DWORD n = 0;
    if (!v) {
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), "0", 1, &n, 0);
        return;
    }
    while (v && n < sizeof(s)) {
        s[n++] = (char)('0' + v % 10);
        v /= 10;
    }
    while (n) {
        DWORD w;
        WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), &s[--n], 1, &w, 0);
    }
}

static void out(const char *s) {
    DWORD n;
    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), s, lstrlenA(s), &n, 0);
}

static void report(const char *s) {
    DWORD n;
    if (g_report != INVALID_HANDLE_VALUE) {
        WriteFile(g_report, s, lstrlenA(s), &n, 0);
    }
}

static void report_u32(DWORD v) {
    char s[16];
    DWORD n = 0;
    if (!v) {
        report("0");
        return;
    }
    while (v && n < sizeof(s)) {
        s[n++] = (char)('0' + v % 10);
        v /= 10;
    }
    while (n) {
        char c[2];
        c[0] = s[--n];
        c[1] = 0;
        report(c);
    }
}

static void report_esc(const char *s) {
    while (*s) {
        if (*s == '<') report("&lt;");
        else if (*s == '>') report("&gt;");
        else if (*s == '&') report("&amp;");
        else {
            char c[2];
            c[0] = *s;
            c[1] = 0;
            report(c);
        }
        s++;
    }
}

static void report_begin(void) {
    if (g_report == INVALID_HANDLE_VALUE) return;
    report("<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>PortBlaster Jelly Report</title><link rel=\"stylesheet\" href=\"/style.css\"></head><body><main><nav><a href=\"/\">home</a> <a href=\"/probe.html\">jelly</a></nav><h1>Jelly Report</h1><p>Target: <code>");
    report_esc(g_host);
    report(":");
    report_u32(g_port);
    report("</code></p><table><tr><th>Probe</th><th>Expect</th><th>Got</th><th>Time</th><th>Bytes</th><th>Result</th></tr>");
}

static void report_row(const char *name, DWORD expect, DWORD got, DWORD ms, DWORD bytes) {
    if (g_report == INVALID_HANDLE_VALUE) return;
    report("<tr><td>");
    report_esc(name);
    report("</td><td>");
    report_u32(expect);
    report("</td><td>");
    report_u32(got);
    report("</td><td>");
    report_u32(ms);
    report(" ms</td><td>");
    report_u32(bytes);
    report("</td><td>");
    report(expect == got ? "PASS" : "FAIL");
    report("</td></tr>");
}

static void report_load(DWORD count, DWORD ok, DWORD avg, DWORD max, DWORD bytes) {
    if (g_report == INVALID_HANDLE_VALUE) return;
    report("</table><h2>Load</h2><p>Requests: ");
    report_u32(count);
    report(" | OK: ");
    report_u32(ok);
    report(" | Avg: ");
    report_u32(avg);
    report(" ms | Max: ");
    report_u32(max);
    report(" ms | Bytes: ");
    report_u32(bytes);
    report("</p>");
}

static void report_end(void) {
    if (g_report == INVALID_HANDLE_VALUE) return;
    report("<h2>Result</h2><p>");
    report(g_fail ? "FAIL" : "PASS");
    report("</p></main></body></html>");
    CloseHandle(g_report);
    g_report = INVALID_HANDLE_VALUE;
}

static SOCKET connect_target(void) {
    SOCKET s;
    struct sockaddr_in a;
    DWORD timeout = 3000;
    s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return INVALID_SOCKET;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof(timeout));
    a.sin_family = AF_INET;
    a.sin_port = htons(g_port);
    a.sin_addr.s_addr = inet_addr(g_host);
    if (connect(s, (struct sockaddr *)&a, sizeof(a)) == SOCKET_ERROR) {
        closesocket(s);
        return INVALID_SOCKET;
    }
    return s;
}

static int status_code(void) {
    if (g_recv[0] != 'H' || g_recv[8] != ' ') return 0;
    return (g_recv[9] - '0') * 100 + (g_recv[10] - '0') * 10 + (g_recv[11] - '0');
}

static int request_raw(const char *raw, DWORD *ms, DWORD *bytes) {
    SOCKET s = connect_target();
    DWORD start;
    int total = 0;
    int n;
    if (s == INVALID_SOCKET) return 0;
    start = GetTickCount();
    send(s, raw, lstrlenA(raw), 0);
    shutdown(s, SD_SEND);
    while (total < RECV_MAX - 1) {
        n = recv(s, g_recv + total, RECV_MAX - 1 - total, 0);
        if (n <= 0) break;
        total += n;
    }
    closesocket(s);
    g_recv[total] = 0;
    *ms = GetTickCount() - start;
    *bytes = (DWORD)total;
    return status_code();
}

static void make_req(const char *method, const char *path) {
    wsprintfA(g_req, "%s %s HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n", method, path);
}

static void probe(const char *name, const char *method, const char *path, int expect) {
    DWORD ms = 0;
    DWORD bytes = 0;
    int code;
    make_req(method, path);
    code = request_raw(g_req, &ms, &bytes);
    out(code == expect ? "PASS " : "FAIL ");
    out(name);
    out(" -> ");
    print_u32((DWORD)code);
    out(" ");
    print_u32(ms);
    out("ms ");
    print_u32(bytes);
    out("B\r\n");
    report_row(name, (DWORD)expect, (DWORD)code, ms, bytes);
    if (code != expect) g_fail++;
}

static void load(const char *path, DWORD count) {
    DWORD i;
    DWORD total = 0;
    DWORD max = 0;
    DWORD ok = 0;
    DWORD bytes = 0;
    DWORD got;
    DWORD ms;
    int code;
    make_req("GET", path);
    for (i = 0; i < count; i++) {
        code = request_raw(g_req, &ms, &got);
        total += ms;
        if (ms > max) max = ms;
        bytes += got;
        if (code == 200) ok++;
    }
    out("LOAD requests=");
    print_u32(count);
    out(" ok=");
    print_u32(ok);
    out(" avg_ms=");
    print_u32(count ? total / count : 0);
    out(" max_ms=");
    print_u32(max);
    out(" bytes=");
    print_u32(bytes);
    out("\r\n");
    report_load(count, ok, count ? total / count : 0, max, bytes);
    if (ok != count) g_fail++;
}

int main(int argc, char **argv) {
    WSADATA wsa;
    DWORD ms;
    DWORD bytes;
    int code;

    if (argc > 1) lstrcpynA(g_host, argv[1], sizeof(g_host));
    if (argc > 2) {
        g_port = parse_port(argv[2]);
        if (!g_port) {
            out("bad port\r\n");
            return 2;
        }
    }
    if (argc > 3) {
        g_report = CreateFileA(argv[3], GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
        if (g_report == INVALID_HANDLE_VALUE) {
            out("could not write report\r\n");
            return 2;
        }
    }
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return 2;

    out("pbjelly target=");
    out(g_host);
    out(":");
    print_u32(g_port);
    out("\r\n");
    report_begin();

    probe("home", "GET", "/", 200);
    probe("head", "HEAD", "/", 200);
    probe("css", "GET", "/style.css", 200);
    probe("missing", "GET", "/missing-nope.html", 404);
    probe("post", "POST", "/", 405);
    probe("traversal", "GET", "/../portblaster.c", 403);
    probe("encoded_traversal", "GET", "/%2e%2e/portblaster.c", 403);
    probe("backslash", "GET", "/..\\portblaster.c", 403);
    probe("drive", "GET", "/C:/Windows/win.ini", 403);
    code = request_raw("GET /\r\n\r\n", &ms, &bytes);
    out(code == 400 ? "PASS no_version -> " : "FAIL no_version -> ");
    print_u32((DWORD)code);
    out("\r\n");
    report_row("no_version", 400, (DWORD)code, ms, bytes);
    if (code != 400) g_fail++;
    code = request_raw("GET / HTTP/9.9\r\nHost: x\r\n\r\n", &ms, &bytes);
    out(code == 400 ? "PASS bad_version -> " : "FAIL bad_version -> ");
    print_u32((DWORD)code);
    out("\r\n");
    report_row("bad_version", 400, (DWORD)code, ms, bytes);
    if (code != 400) g_fail++;
    code = request_raw("GET / HTTP/1.1\r\nHost: x\r\nX-A: AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\r\nConnection: close\r\n\r\n", &ms, &bytes);
    out(code == 200 ? "PASS long_header -> " : "FAIL long_header -> ");
    print_u32((DWORD)code);
    out("\r\n");
    report_row("long_header", 200, (DWORD)code, ms, bytes);
    if (code != 200) g_fail++;
    load("/", 100);

    WSACleanup();
    out(g_fail ? "RESULT fail\r\n" : "RESULT pass\r\n");
    report_end();
    return g_fail ? 1 : 0;
}
