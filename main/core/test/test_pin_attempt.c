#include "core/pin_attempt.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

static int failures = 0;

#define CHECK(actual, expected, message)                                       \
  do {                                                                         \
    if ((actual) != (expected)) {                                              \
      fprintf(stderr, "FAIL: %s\n", message);                                  \
      failures++;                                                              \
    }                                                                          \
  } while (0)

int main(void) {
  const uint8_t max_fail = 10;

  CHECK(pin_attempt_decide(true, max_fail - 1, max_fail), PIN_ATTEMPT_ACCEPT,
        "correct PIN before threshold unlocks");
  CHECK(pin_attempt_decide(false, max_fail - 1, max_fail), PIN_ATTEMPT_RETRY,
        "incorrect PIN before threshold retries");
  CHECK(pin_attempt_decide(true, max_fail, max_fail), PIN_ATTEMPT_ACCEPT,
        "correct PIN at threshold unlocks");
  CHECK(pin_attempt_decide(false, max_fail, max_fail), PIN_ATTEMPT_WIPE,
        "incorrect PIN at threshold wipes");
  CHECK(pin_attempt_decide(true, 1, 0), PIN_ATTEMPT_WIPE,
        "correct PIN with zero threshold wipes");
  CHECK(pin_attempt_decide(false, 1, 0), PIN_ATTEMPT_WIPE,
        "incorrect PIN with zero threshold wipes");
  CHECK(pin_attempt_decide(true, 1, 1), PIN_ATTEMPT_WIPE,
        "correct PIN with threshold 1 wipes");
  CHECK(pin_attempt_decide(false, 1, 1), PIN_ATTEMPT_WIPE,
        "incorrect PIN with threshold 1 wipes");
  CHECK(pin_attempt_decide(true, 1, 4), PIN_ATTEMPT_WIPE,
        "correct PIN with threshold 4 wipes");
  CHECK(pin_attempt_decide(true, 1, 51), PIN_ATTEMPT_WIPE,
        "correct PIN with threshold 51 wipes");
  CHECK(pin_attempt_decide(false, 1, 51), PIN_ATTEMPT_WIPE,
        "incorrect PIN with threshold 51 wipes");
  CHECK(pin_attempt_decide(true, 1, 255), PIN_ATTEMPT_WIPE,
        "correct PIN with threshold 255 wipes");

  CHECK(pin_attempt_decide(true, 5, 5), PIN_ATTEMPT_ACCEPT,
        "correct PIN at minimum valid threshold unlocks");
  CHECK(pin_attempt_decide(false, 4, 5), PIN_ATTEMPT_RETRY,
        "incorrect PIN before minimum valid threshold retries");
  CHECK(pin_attempt_decide(false, 5, 5), PIN_ATTEMPT_WIPE,
        "incorrect PIN at minimum valid threshold wipes");
  CHECK(pin_attempt_decide(true, 50, 50), PIN_ATTEMPT_ACCEPT,
        "correct PIN at maximum valid threshold unlocks");
  CHECK(pin_attempt_decide(false, 49, 50), PIN_ATTEMPT_RETRY,
        "incorrect PIN before maximum valid threshold retries");
  CHECK(pin_attempt_decide(false, 50, 50), PIN_ATTEMPT_WIPE,
        "incorrect PIN at maximum valid threshold wipes");

  for (uint16_t max_failures = 0; max_failures <= UINT8_MAX; max_failures++) {
    if (max_failures >= 5 && max_failures <= 50)
      continue;
    CHECK(pin_attempt_decide(true, 1, (uint8_t)max_failures), PIN_ATTEMPT_WIPE,
          "all invalid thresholds wipe for a correct PIN");
    CHECK(pin_attempt_decide(false, 1, (uint8_t)max_failures), PIN_ATTEMPT_WIPE,
          "all invalid thresholds wipe for an incorrect PIN");
  }

  if (failures) {
    fprintf(stderr, "%d PIN attempt decision test(s) failed\n", failures);
    return 1;
  }

  puts("All PIN attempt decision tests passed");
  return 0;
}
