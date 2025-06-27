#ifndef ARDUINO

#include "esp_idf_compat.h"

// Global ESP instance
ESPClass ESP;

// For ESPHome builds that don't have their own micros/millis
#ifndef USE_ESPHOME
// Already defined inline in header
#endif

#endif // ARDUINO