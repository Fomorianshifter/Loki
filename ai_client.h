#ifndef AI_CLIENT_H
#define AI_CLIENT_H

/**
 * @file ai_client.h
 * @brief AI API client for Loki system
 *
 * Provides a simple interface to query AI services over HTTP.
 * Designed to work with OpenAI-compatible REST APIs (OpenAI, Ollama,
 * local LLM servers, etc.).
 *
 * Quick-start:
 *   1. Set AI_ENABLED to 1 in board_config.h
 *   2. Set AI_API_URL to your endpoint (default: OpenAI chat completions)
 *   3. Set AI_API_KEY to your bearer token / API key
 *   4. Optionally change AI_MODEL to the model you want to use
 *
 * Usage:
 *   ai_client_init(NULL);  // NULL uses values from board_config.h
 *   char reply[AI_MAX_RESPONSE_LEN];
 *   ai_query("Say hello!", reply, sizeof(reply));
 *   printf("%s\n", reply);
 *   ai_client_deinit();
 *
 * Requires: libcurl  (install: sudo apt-get install libcurl4-openssl-dev)
 */

#include "types.h"
#include "board_config.h"

/* ===== AI CLIENT CONFIGURATION ===== */

/**
 * @brief Runtime AI client configuration.
 *
 * Pass a pointer to ai_client_init() to override the board_config.h defaults
 * at runtime, or pass NULL to use the compile-time defaults.
 */
typedef struct {
    const char *api_url;    /**< Full endpoint URL (e.g. "https://api.openai.com/v1/chat/completions") */
    const char *api_key;    /**< Bearer token / API key */
    const char *model;      /**< Model name (e.g. "gpt-4o-mini", "llama3") */
    uint32_t    timeout_s;  /**< HTTP request timeout in seconds */
} ai_client_config_t;

/* ===== PUBLIC API ===== */

/**
 * @brief Initialize the AI client.
 *
 * Must be called before ai_query().
 *
 * @param[in] config  Optional runtime configuration. Pass NULL to use the
 *                    compile-time defaults from board_config.h.
 * @return HAL_OK on success, HAL_ERROR on failure.
 */
hal_status_t ai_client_init(const ai_client_config_t *config);

/**
 * @brief Send a plain-text prompt to the configured AI API.
 *
 * Blocks until a response is received or the request times out.
 *
 * @param[in]  prompt        Null-terminated prompt string.
 * @param[out] response      Buffer to write the model's reply into.
 * @param[in]  response_size Size of the response buffer in bytes.
 * @return HAL_OK on success, HAL_ERROR on failure, HAL_TIMEOUT on timeout,
 *         HAL_INVALID_PARAM if any argument is NULL/zero or the prompt exceeds 2048 chars.
 */
hal_status_t ai_query(const char *prompt, char *response, size_t response_size);

/**
 * @brief Deinitialize the AI client and free resources.
 *
 * @return HAL_OK on success.
 */
hal_status_t ai_client_deinit(void);

#endif /* AI_CLIENT_H */
