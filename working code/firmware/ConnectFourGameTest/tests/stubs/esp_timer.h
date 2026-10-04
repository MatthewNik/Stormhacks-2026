#pragma once
#include <stdint.h>
using esp_err_t = int;
constexpr esp_err_t ESP_OK = 0, ESP_FAIL = -1;
using esp_timer_cb_t = void (*)(void *);
struct esp_timer { int unused; };
using esp_timer_handle_t = esp_timer *;
struct esp_timer_create_args_t {
  esp_timer_cb_t callback; void *arg; int dispatch_method; const char *name; bool skip_unhandled_events;
};
inline esp_timer fakeTimer;
inline esp_timer_cb_t timerCallback = nullptr;
inline void *timerArg = nullptr;
inline uint64_t timerPeriodUs = 0;
inline bool timerCreateOK = true;
inline esp_err_t esp_timer_create(const esp_timer_create_args_t *args, esp_timer_handle_t *handle) {
  if (!timerCreateOK) return ESP_FAIL;
  timerCallback = args->callback; timerArg = args->arg; *handle = &fakeTimer; return ESP_OK;
}
inline esp_err_t esp_timer_start_periodic(esp_timer_handle_t, uint64_t periodUs) {
  timerPeriodUs = periodUs; return ESP_OK;
}
