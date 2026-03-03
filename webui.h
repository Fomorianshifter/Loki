/**
 * @file webui.h
 * @brief Device-hosted Web UI server for Loki
 *
 * Implements a minimal HTTP server that runs in a background thread
 * and serves a system dashboard on the local network.
 *
 * Default access: http://loki.local:8080/  (requires mDNS/Avahi)
 *
 * Configuration macros (override before including this header or via -D flag):
 *   WEBUI_ENABLED     - Set to 0 to compile out the Web UI (default: 1)
 *   WEBUI_PORT        - TCP port to listen on (default: 8080)
 *   WEBUI_BIND_ADDR   - IPv4 address to bind (default: "0.0.0.0")
 */

#ifndef WEBUI_H
#define WEBUI_H

/* ===== CONFIGURATION ===== */

#ifndef WEBUI_ENABLED
#define WEBUI_ENABLED 1
#endif

#ifndef WEBUI_PORT
#define WEBUI_PORT 8080
#endif

#ifndef WEBUI_BIND_ADDR
#define WEBUI_BIND_ADDR "0.0.0.0"
#endif

/* ===== PUBLIC API ===== */

/**
 * @brief Start the Web UI HTTP server in a background thread.
 *
 * Binds to WEBUI_BIND_ADDR:WEBUI_PORT and spawns a pthread to handle
 * incoming HTTP connections.  Non-blocking: returns immediately after
 * the thread is created and the socket is listening.
 *
 * @return 0 on success, non-zero on error.
 */
int webui_start(void);

/**
 * @brief Stop the Web UI HTTP server and join the background thread.
 */
void webui_stop(void);

#endif /* WEBUI_H */
