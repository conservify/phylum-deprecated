#include "boards.h"

constexpr uint8_t FK_SONAR_PIN_PERIPHERALS_ENABLE = 8;
constexpr uint8_t FK_SONAR_PIN_FLASH_CS = 6;

Board sonar_board{
    {
        FK_SONAR_PIN_PERIPHERALS_ENABLE,
        FK_SONAR_PIN_FLASH_CS,
        {
            FK_SONAR_PIN_FLASH_CS,
            0,
            0,
            0,
        },
        {
            FK_SONAR_PIN_PERIPHERALS_ENABLE,
            0,
            0,
            0,
        }
    }
};

static constexpr uint8_t FK_CORE_WIFI_PIN_CS = 7;
static constexpr uint8_t FK_CORE_WIFI_PIN_IRQ = 16;
static constexpr uint8_t FK_CORE_WIFI_PIN_RST = 15;
static constexpr uint8_t FK_CORE_WIFI_PIN_EN = 38;
static constexpr uint8_t FK_CORE_WIFI_PIN_WAKE = 8;

static constexpr uint8_t FK_CORE_RFM95_PIN_CS = 5;
static constexpr uint8_t FK_CORE_RFM95_PIN_RESET = 3;
static constexpr uint8_t FK_CORE_RFM95_PIN_ENABLE = 0;
static constexpr uint8_t FK_CORE_RFM95_PIN_D0 = 2;

static constexpr uint8_t FK_CORE_FLASH_PIN_CS = (26u);           // PIN_LED_TXL
static constexpr uint8_t FK_CORE_PERIPHERALS_ENABLE_PIN = (25u); // PIN_LED_RXL
static constexpr uint8_t FK_CORE_MODULES_ENABLE_PIN = (19ul);    // A5 (Was 9ul)
static constexpr uint8_t FK_CORE_GPS_ENABLE_PIN = (18ul);        // A4
static constexpr uint8_t FK_CORE_SD_PIN_CS = 12;

CoreBoard board{
    {
        {
            FK_CORE_PERIPHERALS_ENABLE_PIN,
            FK_CORE_FLASH_PIN_CS,
            {
                FK_CORE_FLASH_PIN_CS,
                FK_CORE_WIFI_PIN_CS,
                FK_CORE_RFM95_PIN_CS,
                FK_CORE_SD_PIN_CS,
            },
            {
                FK_CORE_PERIPHERALS_ENABLE_PIN,
                FK_CORE_GPS_ENABLE_PIN,
                0,
                0,
            }
        },
        FK_CORE_SD_PIN_CS,
        FK_CORE_WIFI_PIN_CS,
        FK_CORE_WIFI_PIN_EN,
        FK_CORE_GPS_ENABLE_PIN,
        FK_CORE_MODULES_ENABLE_PIN,
    }
};


