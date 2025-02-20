#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/watchdog.h"
#include "pico/bootrom.h"
#include "pico/binary_info.h"
#include "hardware/flash.h"
#include "hardware/gpio.h"

#define LED_PIN 25
#define MAX_FLASH_SIZE (16 * 1024 * 1024)  // Max expected flash (16MB)
#define FLASH_TARGET_OFFSET (1 * 1024 * 1024) // Start testing at 1MB offset
#define SPI_FLASH_SECTOR_SIZE (4 * 1024) // 4KB per flash sector
#define TOTAL_RAM_TEST_SIZE (64 * 1024) // 64KB memory test

// Function to continuously blink result
void continuous_blink(int blinks) {
    while (1) {
        for (int i = 0; i < blinks; i++) {
            gpio_put(LED_PIN, 1);
            sleep_ms(250);
            gpio_put(LED_PIN, 0);
            sleep_ms(250);
        }
        sleep_ms(1000);  // Pause before repeating
    }
}

// Wait for USB serial connection
void wait_for_usb() {
    for (int i = 0; i < 10; i++) {
        printf("Waiting for USB...\r\n");
        fflush(stdout);
        sleep_ms(500);
    }
}

// Memory Test (64KB)
bool memory_test() {
    printf("Starting memory test (64KB)...\r\n");
    fflush(stdout);

    uint8_t *test_memory = malloc(TOTAL_RAM_TEST_SIZE);
    if (!test_memory) {
        printf("❌ Memory allocation failed\r\n");
        fflush(stdout);
        return false;
    }

    // Write pattern
    for (int i = 0; i < TOTAL_RAM_TEST_SIZE; i++) {
        test_memory[i] = i % 256;
    }

    // Verify pattern
    for (int i = 0; i < TOTAL_RAM_TEST_SIZE; i++) {
        if (test_memory[i] != (i % 256)) {
            printf("❌ Memory corruption at byte %d\r\n", i);
            fflush(stdout);
            free(test_memory);
            return false;
        }
    }

    printf("✅ Memory test PASSED (64KB verified)\r\n");
    fflush(stdout);
    free(test_memory);
    return true;
}

// Flash Test (Runs Once & Stops)
bool flash_test() {
    printf("Starting full flash test...\r\n");
    fflush(stdout);

    uint32_t tested_size = 0;
    uint8_t test_data[SPI_FLASH_SECTOR_SIZE];
    uint8_t readback[SPI_FLASH_SECTOR_SIZE];

    for (uint32_t addr = FLASH_TARGET_OFFSET; addr < MAX_FLASH_SIZE; addr += SPI_FLASH_SECTOR_SIZE) {
        printf("Testing flash at %.2f MB (0x%x)...\r\n", addr / (1024.0 * 1024.0), addr);
        fflush(stdout);

        flash_range_erase(addr, SPI_FLASH_SECTOR_SIZE);

        for (int i = 0; i < SPI_FLASH_SECTOR_SIZE; i++) {
            test_data[i] = i % 256;
        }

        flash_range_program(addr, test_data, SPI_FLASH_SECTOR_SIZE);
        printf("Wrote sector at %.2f MB...\r\n", addr / (1024.0 * 1024.0));
        fflush(stdout);

        memcpy(readback, (void*)(XIP_BASE + addr), SPI_FLASH_SECTOR_SIZE);
        for (int i = 0; i < SPI_FLASH_SECTOR_SIZE; i++) {
            if (readback[i] != test_data[i]) {
                printf("❌ Flash corruption at %.2f MB (0x%x), offset %d\r\n", addr / (1024.0 * 1024.0), addr, i);
                fflush(stdout);
                printf("Detected actual flash size: %.2f MB\r\n", tested_size / (1024.0 * 1024.0));
                return false;
            }
        }

        tested_size += SPI_FLASH_SECTOR_SIZE;
        printf("✅ Verified %.2f MB of flash\r\n", tested_size / (1024.0 * 1024.0));
        fflush(stdout);
    }

    printf("✅ Full flash test PASSED! Total verified flash: %.2f MB\r\n", tested_size / (1024.0 * 1024.0));
    fflush(stdout);
    return true;
}

int main() {
    stdio_init_all();
    wait_for_usb();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    printf("🚀 Starting Torture Test (One-Time Execution)\r\n");
    fflush(stdout);

    bool mem_result = memory_test();
    bool flash_result = flash_test();

    // Final result output & continuous LED blink
    if (!mem_result) {
        printf("❌ Memory test failed. System halted.\r\n");
        fflush(stdout);
        continuous_blink(1);  // Blink 1x continuously for memory failure
    } else if (!flash_result) {
        printf("❌ Flash test failed. System halted.\r\n");
        fflush(stdout);
        continuous_blink(2);  // Blink 2x continuously for flash failure
    } else {
        printf("✅ Torture Test COMPLETE. Memory and Flash OK!\r\n");
        fflush(stdout);
        continuous_blink(3);  // Blink 3x continuously if all passed
    }
}
