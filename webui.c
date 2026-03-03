/**
 * @file webui.c
 * @brief Device-hosted Web UI HTTP server implementation for Loki
 *
 * Serves a minimal dashboard at GET /  and JSON status at GET /api/status.
 * Runs in a dedicated pthread so it never blocks the main application loop.
 * Uses only POSIX sockets and pthreads — no external dependencies.
 */

#include "webui.h"

#if WEBUI_ENABLED

#include "log.h"
#include "board_config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* ===== INTERNAL HELPERS ===== */

/* Convert the port integer to a string literal for embedding in HTML */
#define _WEBUI_STR(x)  #x
#define _WEBUI_XSTR(x) _WEBUI_STR(x)
#define WEBUI_PORT_STR _WEBUI_XSTR(WEBUI_PORT)

/* Maximum size of an incoming HTTP request line we bother reading */
#define REQUEST_BUF_SIZE 2048

/* ===== MODULE STATE ===== */

static pthread_t   s_thread;
static volatile int s_running = 0;
static int          s_listen_fd = -1;
static time_t       s_start_time;

/* ===== HTTP RESPONSE HELPERS ===== */

/** Send all bytes in buf, handling partial writes. */
static void send_all(int fd, const char *buf, size_t len)
{
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = write(fd, buf + sent, len - sent);
        if (n <= 0) {
            break;
        }
        sent += (size_t)n;
    }
}

/** Send a NUL-terminated string. */
static void send_str(int fd, const char *s)
{
    send_all(fd, s, strlen(s));
}

/* ===== ROUTE HANDLERS ===== */

/**
 * GET /  — HTML dashboard
 */
static void handle_root(int client_fd)
{
    static const char *HEADERS =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Connection: close\r\n\r\n";

    static const char *BODY =
        "<!DOCTYPE html>\n"
        "<html lang=\"en\">\n"
        "<head>\n"
        "  <meta charset=\"UTF-8\">\n"
        "  <meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
        "  <title>Loki &mdash; " BOARD_NAME "</title>\n"
        "  <style>\n"
        "    body{font-family:monospace;background:#1a1a2e;color:#e0e0e0;"
        "         margin:2em auto;max-width:720px;padding:0 1em}\n"
        "    h1{color:#00d4ff;margin-bottom:0.3em}\n"
        "    h2{color:#88aaff;margin:0.4em 0 0.2em}\n"
        "    .card{background:#16213e;border-radius:6px;padding:1em;margin:0.8em 0}\n"
        "    .ok{color:#00ff88} .label{color:#aaa}\n"
        "    a{color:#00d4ff}\n"
        "  </style>\n"
        "</head>\n"
        "<body>\n"
        "  <h1>&#9889; Loki System Dashboard</h1>\n"
        "  <div class=\"card\">\n"
        "    <h2>System</h2>\n"
        "    <p><span class=\"label\">Board:</span> "
        "       <span class=\"ok\">" BOARD_NAME " v" BOARD_VERSION "</span></p>\n"
        "    <p><span class=\"label\">Model:</span> " BOARD_MODEL "</p>\n"
        "    <p><span class=\"label\">Status:</span> "
        "       <span class=\"ok\">&#10003; Running</span></p>\n"
        "  </div>\n"
        "  <div class=\"card\">\n"
        "    <h2>Hardware</h2>\n"
        "    <p><span class=\"label\">Display:</span> " TFT_TYPE " ("
        "       " _WEBUI_XSTR(TFT_WIDTH) "&times;" _WEBUI_XSTR(TFT_HEIGHT) " px)</p>\n"
        "    <p><span class=\"label\">Flash:</span> " FLASH_IC_TYPE "</p>\n"
        "    <p><span class=\"label\">EEPROM:</span> " EEPROM_IC_TYPE "</p>\n"
        "  </div>\n"
        "  <div class=\"card\">\n"
        "    <h2>API Endpoints</h2>\n"
        "    <p><a href=\"/api/status\">/api/status</a> &mdash; JSON status</p>\n"
        "  </div>\n"
        "  <div class=\"card\">\n"
        "    <p><small>Friendly URL: "
        "       <a href=\"http://loki.local:" WEBUI_PORT_STR "/\">"
        "http://loki.local:" WEBUI_PORT_STR "/</a></small></p>\n"
        "  </div>\n"
        "</body>\n"
        "</html>\n";

    send_str(client_fd, HEADERS);
    send_str(client_fd, BODY);
}

/**
 * GET /api/status  — JSON status object
 */
static void handle_api_status(int client_fd)
{
    static const char *HEADERS =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: application/json\r\n"
        "Connection: close\r\n\r\n";

    time_t now    = time(NULL);
    long   uptime = (long)(now - s_start_time);

    char body[512];
    snprintf(body, sizeof(body),
        "{\n"
        "  \"app\": \"Loki\",\n"
        "  \"version\": \"%s\",\n"
        "  \"board\": \"%s\",\n"
        "  \"model\": \"%s\",\n"
        "  \"uptime_seconds\": %ld,\n"
        "  \"build\": \"%s\",\n"
        "  \"mood\": \"calm\",\n"
        "  \"state\": \"running\"\n"
        "}\n",
        BOARD_VERSION,
        BOARD_NAME,
        BOARD_MODEL,
        uptime,
#ifdef DEBUG
        "debug"
#else
        "release"
#endif
    );

    send_str(client_fd, HEADERS);
    send_str(client_fd, body);
}

/**
 * 404 response for unrecognised paths.
 */
static void handle_not_found(int client_fd)
{
    static const char *RESPONSE =
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Type: text/plain\r\n"
        "Connection: close\r\n\r\n"
        "404 Not Found\n";
    send_str(client_fd, RESPONSE);
}

/* ===== CONNECTION HANDLER ===== */

/**
 * Read the first line of an HTTP request and dispatch to the correct handler.
 */
static void handle_connection(int client_fd)
{
    char buf[REQUEST_BUF_SIZE];
    ssize_t n = recv(client_fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) {
        return;
    }
    buf[n] = '\0';

    /* Parse: "METHOD /path HTTP/1.x\r\n..." — only need method + path */
    char method[16] = {0};
    char path[256]  = {0};
    sscanf(buf, "%15s %255s", method, path);

    if (strcmp(method, "GET") == 0) {
        if (strcmp(path, "/") == 0) {
            handle_root(client_fd);
        } else if (strcmp(path, "/api/status") == 0) {
            handle_api_status(client_fd);
        } else {
            handle_not_found(client_fd);
        }
    } else {
        /* Method Not Allowed */
        static const char *RESP =
            "HTTP/1.1 405 Method Not Allowed\r\n"
            "Connection: close\r\n\r\n";
        send_str(client_fd, RESP);
    }
}

/* ===== SERVER THREAD ===== */

static void *webui_thread_func(void *arg)
{
    (void)arg;

    while (s_running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(s_listen_fd,
                               (struct sockaddr *)&client_addr,
                               &client_len);
        if (client_fd < 0) {
            if (s_running) {
                LOG_WARN("webui: accept() error: %s", strerror(errno));
            }
            break;
        }

        handle_connection(client_fd);
        close(client_fd);
    }

    return NULL;
}

/* ===== PUBLIC API ===== */

int webui_start(void)
{
    s_start_time = time(NULL);

    /* Create TCP socket */
    s_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (s_listen_fd < 0) {
        LOG_ERROR("webui: socket() failed: %s", strerror(errno));
        return -1;
    }

    /* Allow fast restart after crash */
    int opt = 1;
    setsockopt(s_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* Bind to WEBUI_BIND_ADDR:WEBUI_PORT */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(WEBUI_PORT);
    addr.sin_addr.s_addr = inet_addr(WEBUI_BIND_ADDR);

    if (bind(s_listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        LOG_ERROR("webui: bind() to %s:%d failed: %s",
                  WEBUI_BIND_ADDR, WEBUI_PORT, strerror(errno));
        close(s_listen_fd);
        s_listen_fd = -1;
        return -1;
    }

    if (listen(s_listen_fd, 8) < 0) {
        LOG_ERROR("webui: listen() failed: %s", strerror(errno));
        close(s_listen_fd);
        s_listen_fd = -1;
        return -1;
    }

    s_running = 1;

    if (pthread_create(&s_thread, NULL, webui_thread_func, NULL) != 0) {
        LOG_ERROR("webui: pthread_create() failed: %s", strerror(errno));
        s_running = 0;
        close(s_listen_fd);
        s_listen_fd = -1;
        return -1;
    }

    LOG_INFO("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    LOG_INFO("Web UI started on port %d", WEBUI_PORT);
    LOG_INFO("  Local network : http://<device-ip>:%d/", WEBUI_PORT);
    LOG_INFO("  Friendly URL  : http://loki.local:%d/  (requires mDNS)", WEBUI_PORT);
    LOG_INFO("  API status    : http://loki.local:%d/api/status", WEBUI_PORT);
    LOG_INFO("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");

    return 0;
}

void webui_stop(void)
{
    if (!s_running) {
        return;
    }

    s_running = 0;

    /* Closing the listening socket unblocks the accept() call in the thread */
    if (s_listen_fd >= 0) {
        close(s_listen_fd);
        s_listen_fd = -1;
    }

    pthread_join(s_thread, NULL);
    LOG_INFO("Web UI server stopped");
}

#endif /* WEBUI_ENABLED */
