#pragma once

#ifndef ARDUINO

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"
#include "soc/gpio_struct.h"
#include "hal/gpio_hal.h"
#include "hal/gpio_ll.h"
#include "soc/soc_caps.h"
#include "esp_intr_alloc.h"
#ifdef CONFIG_SPIRAM
#include "esp_psram.h"
#endif

// Arduino compatibility types
typedef uint8_t byte;
typedef bool boolean;

// Pin modes
#define INPUT GPIO_MODE_INPUT
#define OUTPUT GPIO_MODE_OUTPUT
#define INPUT_PULLUP GPIO_MODE_INPUT
#define OUTPUT_OPEN_DRAIN GPIO_MODE_OUTPUT_OD

// Pin levels
#define HIGH 1
#define LOW 0

// Digital I/O functions
inline void pinMode(uint8_t pin, uint8_t mode) {
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << pin);
    
    if (mode == OUTPUT) {
        io_conf.mode = GPIO_MODE_OUTPUT;
    } else if (mode == OUTPUT_OPEN_DRAIN) {
        io_conf.mode = GPIO_MODE_OUTPUT_OD;
    } else if (mode == INPUT) {
        io_conf.mode = GPIO_MODE_INPUT;
    } else if (mode == INPUT_PULLUP) {
        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    }
    
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);
}

inline void digitalWrite(uint8_t pin, uint8_t val) {
    gpio_set_level((gpio_num_t)pin, val);
}

inline int digitalRead(uint8_t pin) {
    return gpio_get_level((gpio_num_t)pin);
}

// NOP for timing loops
#define NOP() asm volatile ("nop")

// Timing functions - exact Arduino implementation
#ifndef USE_ESPHOME
inline unsigned long IRAM_ATTR micros() {
    return (unsigned long)(esp_timer_get_time());
}

inline unsigned long IRAM_ATTR millis() {
    return (unsigned long)(esp_timer_get_time() / 1000ULL);
}
#else
// For ESPHome, make sure we have proper declarations
namespace esphome {
    uint32_t micros();
    uint32_t millis();
}
// Import into global namespace
using esphome::micros;
using esphome::millis;
#endif

inline void delay(uint32_t ms) {
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

// Arduino's exact delayMicroseconds implementation
inline void IRAM_ATTR delayMicroseconds(uint32_t us) {
    uint64_t m = (uint64_t)esp_timer_get_time();
    if (us) {
        uint64_t e = (m + us);
        if (m > e) { //overflow
            while ((uint64_t)esp_timer_get_time() > e) {
                NOP();
            }
        }
        while ((uint64_t)esp_timer_get_time() < e) {
            NOP();
        }
    }
}

// Interrupt functions
#define CHANGE GPIO_INTR_ANYEDGE
#define FALLING GPIO_INTR_NEGEDGE
#define RISING GPIO_INTR_POSEDGE

inline int digitalPinToInterrupt(uint8_t pin) {
    return pin;
}

// Arduino ISR flag
#ifdef CONFIG_ESP_SYSTEM_CHECK_INT_LEVEL_4
#define ARDUINO_ISR_FLAG ESP_INTR_FLAG_IRAM
#else  
#define ARDUINO_ISR_FLAG (0)
#endif

inline void attachInterruptArg(uint8_t pin, void (*handler)(void*), void* arg, int mode) {
    static bool interrupt_initialized = false;
    
    if (pin >= SOC_GPIO_PIN_COUNT) {
        return;
    }
    
    if (!interrupt_initialized) {
        esp_err_t err = gpio_install_isr_service((int)ARDUINO_ISR_FLAG);
        interrupt_initialized = (err == ESP_OK) || (err == ESP_ERR_INVALID_STATE);
    }
    if (!interrupt_initialized) {
        return;
    }
    
    gpio_set_intr_type((gpio_num_t)pin, (gpio_int_type_t)mode);
    gpio_isr_handler_add((gpio_num_t)pin, handler, arg);
    
    // Enable input in GPIO register (important for peripherals outputs)
    gpio_hal_context_t gpiohal;
    gpiohal.dev = GPIO_LL_GET_HW(GPIO_PORT_0);
    gpio_hal_input_enable(&gpiohal, pin);
}

inline void detachInterrupt(uint8_t pin) {
    gpio_intr_disable((gpio_num_t)pin);
    gpio_isr_handler_remove((gpio_num_t)pin);
}

// ESP specific functions
#include "esp_cpu.h"

class ESPClass {
public:
    inline uint32_t IRAM_ATTR getCycleCount() __attribute__((always_inline)) {
        return (uint32_t)esp_cpu_get_cycle_count();
    }
    
    inline uint32_t getCpuFreqMHz() {
        return CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;
    }
};

extern ESPClass ESP;

// PSRAM detection
inline bool psramFound() {
#ifdef CONFIG_SPIRAM
    return esp_psram_is_initialized();
#else
    return false;
#endif
}

// Stream base class replacement
class Print {
public:
    virtual size_t write(uint8_t) = 0;
    virtual size_t write(const uint8_t *buffer, size_t size) {
        size_t n = 0;
        while (size--) {
            if (write(*buffer++)) n++;
            else break;
        }
        return n;
    }
    
    size_t write(const char *str) {
        if (str == NULL) return 0;
        return write((const uint8_t *)str, strlen(str));
    }
    
    size_t print(const char str[]) { return write(str); }
    size_t println(const char str[]) {
        size_t n = write(str);
        n += write("\r\n");
        return n;
    }
};

class Stream : public Print {
protected:
    unsigned long _timeout = 1000;  // Default timeout of 1 second
    
public:
    Stream() {}
    
    virtual int available() = 0;
    virtual int read() = 0;
    virtual int peek() = 0;
    virtual void flush() = 0;
    
    void setTimeout(unsigned long timeout) {
        _timeout = timeout;
    }
    
    virtual size_t readBytes(uint8_t *buffer, size_t length) {
        size_t count = 0;
        while (count < length) {
            int c = read();
            if (c < 0) break;
            *buffer++ = (uint8_t)c;
            count++;
        }
        return count;
    }
    
    virtual size_t readBytes(char *buffer, size_t length) {
        return readBytes((uint8_t*)buffer, length);
    }
};

// IRAM attributes
#ifndef IRAM_ATTR
#define IRAM_ATTR __attribute__((section(".iram1")))
#endif
#define ALWAYS_INLINE_ATTR inline __attribute__((always_inline))

// Yield function
inline void yield() {
    vTaskDelay(0);
}

inline void optimistic_yield(uint32_t interval_us) {
    static uint32_t last_yield = 0;
    uint32_t now = micros();
    if (now - last_yield > interval_us) {
        yield();
        last_yield = now;
    }
}

// Memory reading
#define pgm_read_byte(addr) (*(const uint8_t *)(addr))
#define PSTR(str) (str)

// GPIO register access functions
inline volatile uint32_t* portOutputRegister(uint8_t port) {
    return (volatile uint32_t*)&GPIO.out;
}

inline volatile uint32_t* portInputRegister(uint8_t port) {
    return (volatile uint32_t*)&GPIO.in;
}

inline uint32_t digitalPinToBitMask(uint8_t pin) {
    if (pin < 32) {
        return (1UL << pin);
    } else {
        return (1UL << (pin - 32));
    }
}

inline uint8_t digitalPinToPort(uint8_t pin) {
    return pin < 32 ? 0 : 1;
}

// ESP8266 compatibility
#ifdef ESP8266
// ESP8266 GPIO registers
#define GPOS   (*(volatile uint32_t*)0x60000304)
#define GPOC   (*(volatile uint32_t*)0x60000308)
#define GP16O  (*(volatile uint32_t*)0x60000768)
#else
// ESP32 specific functions
#ifndef ESP32
#define ESP32
#endif

// Critical section handling
typedef portMUX_TYPE portMUX_TYPE;
#ifndef portMUX_INITIALIZER_UNLOCKED
#define portMUX_INITIALIZER_UNLOCKED SPINLOCK_INITIALIZER
#endif

// ESP8266 emulation functions for ESP32
inline uint32_t xt_rsil(uint32_t level) {
    uint32_t old_ps;
    __asm__ __volatile__ ("rsil %0, %1" : "=a" (old_ps) : "I" (level));
    return old_ps;
}

inline void xt_wsr_ps(uint32_t ps) {
    __asm__ __volatile__ ("wsr %0, ps" : : "a" (ps));
}
#endif

// Timing conversion
#ifdef F_CPU
#undef F_CPU
#endif
#define F_CPU (CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ * 1000000L)

#endif // ARDUINO