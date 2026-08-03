/**
 * @file ai_client.c
 * @brief AI API client implementation using libcurl
 *
 * Sends a user prompt to an OpenAI-compatible chat-completions endpoint and
 * extracts the plain-text reply from the JSON response.
 *
 * Build:
 *   gcc -I../.. -c ai_client.c -o ai_client.o
 *   # Then link the final binary with -lcurl
 *
 * When AI_ENABLED is 0 in board_config.h all public functions return HAL_OK
 * immediately (stub mode) so the binary can be built without libcurl and
 * without a live API endpoint.
 */

#include "ai_client.h"
#include "../../utils/log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===== STUB MODE ===== */
/* When AI is disabled at compile time, provide lightweight stubs so that    */
/* callers can be linked and deployed without the libcurl dependency.         */
#if !AI_ENABLED

hal_status_t ai_client_init(const ai_client_config_t *config)
{
    (void)config;
    LOG_INFO("ai_client: AI_ENABLED=0 – running in stub mode (no API calls)");
    return HAL_OK;
}

hal_status_t ai_query(const char *prompt, char *response, size_t response_size)
{
    (void)prompt;
    if (response && response_size > 0) {
        strncpy(response, "[AI disabled – set AI_ENABLED=1 in board_config.h]",
                response_size - 1);
        response[response_size - 1] = '\0';
    }
    return HAL_OK;
}

hal_status_t ai_client_deinit(void)
{
    return HAL_OK;
}

#else /* AI_ENABLED */

/* libcurl is required for HTTP support */
#include <curl/curl.h>

/* ===== INTERNAL STATE ===== */

static struct {
    uint8_t  initialized;
    char     api_url[256];
    char     api_key[256];
    char     model[64];
    uint32_t timeout_s;
} ai_state = {0};

/* ===== CURL WRITE CALLBACK ===== */

/** @brief Growing buffer used by the libcurl write callback. */
typedef struct {
    char  *data;
    size_t size;
} curl_buf_t;

/**
 * @brief libcurl write callback – appends received bytes to a heap buffer.
 */
static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t      new_bytes = size * nmemb;
    curl_buf_t *buf       = (curl_buf_t *)userp;
    char       *ptr       = realloc(buf->data, buf->size + new_bytes + 1);

    if (ptr == NULL) {
        LOG_ERROR("ai_client: out of memory in write callback");
        return 0; /* Tell curl to abort */
    }

    buf->data = ptr;
    memcpy(buf->data + buf->size, contents, new_bytes);
    buf->size += new_bytes;
    buf->data[buf->size] = '\0';
    return new_bytes;
}

/* ===== JSON HELPERS ===== */

/**
 * @brief Escape a plain string for embedding in a JSON string value.
 *
 * Handles backslash, double-quote, and common control characters.
 */
static void json_escape(const char *src, char *dst, size_t dst_size)
{
    size_t di = 0;

    for (size_t si = 0; src[si] != '\0' && di + 2 < dst_size; si++) {
        unsigned char c = (unsigned char)src[si];
        if (c == '"' || c == '\\') {
            dst[di++] = '\\';
            dst[di++] = (char)c;
        } else if (c == '\n') {
            dst[di++] = '\\'; dst[di++] = 'n';
        } else if (c == '\r') {
            dst[di++] = '\\'; dst[di++] = 'r';
        } else if (c == '\t') {
            dst[di++] = '\\'; dst[di++] = 't';
        } else {
            dst[di++] = (char)c;
        }
    }
    dst[di] = '\0';
}

/**
 * @brief Extract the value of a JSON string field by key.
 *
 * Searches for @p key in @p json and copies the corresponding string value
 * into @p value_out.
 *
 * @return 1 on success, 0 if the key was not found.
 */
static int json_extract_string(const char *json, const char *key,
                               char *value_out, size_t value_size)
{
    char search[128];
    snprintf(search, sizeof(search), "\"%s\"", key);

    const char *pos = strstr(json, search);
    if (pos == NULL) return 0;

    pos += strlen(search);
    while (*pos == ' ' || *pos == ':' || *pos == '\t' || *pos == '\n') pos++;
    if (*pos != '"') return 0;
    pos++; /* skip opening quote */

    size_t vi = 0;
    while (*pos != '\0' && vi + 1 < value_size) {
        if (*pos == '\\' && *(pos + 1) != '\0') {
            pos++;
            switch (*pos) {
                case '"':  value_out[vi++] = '"';  break;
                case '\\': value_out[vi++] = '\\'; break;
                case '/':  value_out[vi++] = '/';  break;
                case 'n':  value_out[vi++] = '\n'; break;
                case 'r':  value_out[vi++] = '\r'; break;
                case 't':  value_out[vi++] = '\t'; break;
                default:   value_out[vi++] = *pos; break;
            }
        } else if (*pos == '"') {
            break; /* closing quote */
        } else {
            value_out[vi++] = *pos;
        }
        pos++;
    }
    value_out[vi] = '\0';
    return 1;
}

/* ===== PUBLIC IMPLEMENTATION ===== */

hal_status_t ai_client_init(const ai_client_config_t *config)
{
    if (ai_state.initialized) {
        LOG_INFO("ai_client: already initialized");
        return HAL_OK;
    }

    /* Apply runtime config or fall back to board_config.h defaults */
    const char *url = (config && config->api_url && config->api_url[0])
                      ? config->api_url : AI_API_URL;
    const char *key = (config && config->api_key && config->api_key[0])
                      ? config->api_key : AI_API_KEY;
    const char *mdl = (config && config->model && config->model[0])
                      ? config->model : AI_MODEL;

    strncpy(ai_state.api_url, url, sizeof(ai_state.api_url) - 1);
    ai_state.api_url[sizeof(ai_state.api_url) - 1] = '\0';
    strncpy(ai_state.api_key, key, sizeof(ai_state.api_key) - 1);
    ai_state.api_key[sizeof(ai_state.api_key) - 1] = '\0';
    strncpy(ai_state.model, mdl, sizeof(ai_state.model) - 1);
    ai_state.model[sizeof(ai_state.model) - 1] = '\0';
    ai_state.timeout_s = (config && config->timeout_s > 0)
                         ? config->timeout_s : (uint32_t)AI_TIMEOUT_SECONDS;

    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        LOG_ERROR("ai_client: curl_global_init failed");
        return HAL_ERROR;
    }

    ai_state.initialized = 1;

    if (ai_state.api_key[0] == '\0') {
        LOG_WARN("ai_client: initialized but AI_API_KEY is empty – set it in board_config.h");
    } else {
        LOG_INFO("ai_client: initialized (url=%s, model=%s)", ai_state.api_url, ai_state.model);
    }

    return HAL_OK;
}

hal_status_t ai_query(const char *prompt, char *response, size_t response_size)
{
    if (!ai_state.initialized) {
        LOG_ERROR("ai_client: not initialized – call ai_client_init() first");
        return HAL_ERROR;
    }
    if (prompt == NULL || response == NULL || response_size == 0) {
        return HAL_INVALID_PARAM;
    }
    if (strlen(prompt) > 2048) {
        LOG_ERROR("ai_client: prompt exceeds 2048 characters");
        return HAL_INVALID_PARAM;
    }
    if (ai_state.api_key[0] == '\0') {
        LOG_ERROR("ai_client: AI_API_KEY is not set");
        return HAL_ERROR;
    }

    response[0] = '\0';

    CURL *curl = curl_easy_init();
    if (curl == NULL) {
        LOG_ERROR("ai_client: curl_easy_init failed");
        return HAL_ERROR;
    }

    /* Build JSON request body */
    char escaped_prompt[4096];
    json_escape(prompt, escaped_prompt, sizeof(escaped_prompt));

    char request_body[5120];
    snprintf(request_body, sizeof(request_body),
             "{\"model\":\"%s\",\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}]}",
             ai_state.model, escaped_prompt);

    /* Authorization header – "Authorization: Bearer " (22) + key (255) + NUL */
    char auth_header[280];
    snprintf(auth_header, sizeof(auth_header),
             "Authorization: ******", ai_state.api_key);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, auth_header);

    curl_buf_t response_buf = {NULL, 0};

    curl_easy_setopt(curl, CURLOPT_URL,           ai_state.api_url);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER,    headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS,    request_body);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA,     &response_buf);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,       (long)ai_state.timeout_s);

    LOG_DEBUG("ai_client: sending request to %s", ai_state.api_url);

    CURLcode res = curl_easy_perform(curl);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res == CURLE_OPERATION_TIMEDOUT) {
        LOG_ERROR("ai_client: request timed out after %u seconds", ai_state.timeout_s);
        free(response_buf.data);
        return HAL_TIMEOUT;
    }

    if (res != CURLE_OK) {
        LOG_ERROR("ai_client: curl error: %s", curl_easy_strerror(res));
        free(response_buf.data);
        return HAL_ERROR;
    }

    LOG_DEBUG("ai_client: received %zu bytes", response_buf.size);

    if (response_buf.data == NULL ||
        !json_extract_string(response_buf.data, "content", response, response_size)) {
        LOG_ERROR("ai_client: could not parse 'content' from response");
        LOG_DEBUG("ai_client: raw response: %s",
                  response_buf.data ? response_buf.data : "(empty)");
        free(response_buf.data);
        return HAL_ERROR;
    }

    free(response_buf.data);
    LOG_INFO("ai_client: received reply (%zu chars)", strlen(response));
    return HAL_OK;
}

hal_status_t ai_client_deinit(void)
{
    if (!ai_state.initialized) {
        return HAL_OK;
    }

    curl_global_cleanup();
    memset(&ai_state, 0, sizeof(ai_state));
    LOG_INFO("ai_client: deinitialized");
    return HAL_OK;
}

#endif /* AI_ENABLED */
