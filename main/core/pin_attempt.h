#ifndef PIN_ATTEMPT_H
#define PIN_ATTEMPT_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  PIN_ATTEMPT_ACCEPT,
  PIN_ATTEMPT_RETRY,
  PIN_ATTEMPT_WIPE,
} pin_attempt_decision_t;

/* Decide the outcome only after both PIN hashes were computed and compared. */
static inline pin_attempt_decision_t pin_attempt_decide(bool pin_matches,
                                                        uint8_t pending_count,
                                                        uint8_t max_failures) {
  if (max_failures < 5 || max_failures > 50)
    return PIN_ATTEMPT_WIPE;
  if (pin_matches)
    return PIN_ATTEMPT_ACCEPT;
  if (pending_count >= max_failures)
    return PIN_ATTEMPT_WIPE;
  return PIN_ATTEMPT_RETRY;
}

#endif // PIN_ATTEMPT_H
