#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <wiring_private.h>

#include "boards.h"

TwoWire Wire4and3{ &sercom2, 4, 3 };

extern "C" {

void SERCOM2_Handler(void) {
    Wire4and3.onService();
}

}

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

void Board::disable_cs(uint8_t pin) {
    pinMode(pin, INPUT);
}

void Board::enable_cs(uint8_t pin) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
}

void Board::low(uint8_t pin) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
}

void Board::high(uint8_t pin) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, HIGH);
}

SpiWrapper Board::spi() {
    return SpiWrapper::spi();
}

TwoWireWrapper Board::i2c1() {
    return TwoWireWrapper::i2c1();
}

TwoWireWrapper Board::i2c2() {
    return TwoWireWrapper::i2c2();
}

SpiWrapper SpiWrapper::spi() {
    return SpiWrapper { &SPI };
}

void SpiWrapper::begin() {
    reinterpret_cast<SPIClass*>(ptr_)->begin();
}

void SpiWrapper::end() {
    reinterpret_cast<SPIClass*>(ptr_)->end();

    pinMode(PIN_SPI_MISO, INPUT);
    pinMode(PIN_SPI_MOSI, INPUT);
    pinMode(PIN_SPI_SCK, INPUT);
}

TwoWireWrapper TwoWireWrapper::i2c1() {
    return { &Wire };
}

TwoWireWrapper TwoWireWrapper::i2c2() {
    return { &Wire4and3 };
}

void TwoWireWrapper::begin() {
    auto bus = reinterpret_cast<TwoWire*>(ptr_);

    bus->begin();

    if (bus == &Wire4and3) {
        pinPeripheral(4, PIO_SERCOM_ALT);
        pinPeripheral(3, PIO_SERCOM_ALT);
    }
}

void TwoWireWrapper::end() {
    auto bus = reinterpret_cast<TwoWire*>(ptr_);

    bus->end();
}

CoreBoard::CoreBoard(CoreBoardConfig config) : Board(config.config), config_(config) {
}

void CoreBoard::disable_everything() {
    Board::disable_everything();
    low(config_.gps_enable);
    low(config_.modules_enable);
    low(config_.wifi_enable);
}

void CoreBoard::enable_everything() {
    Board::enable_everything();
    high(config_.wifi_enable);
    high(config_.modules_enable);
    high(config_.gps_enable);
}

void CoreBoard::disable_gps() {
    low(config_.gps_enable);
}

void CoreBoard::enable_gps() {
    high(config_.gps_enable);
}

void CoreBoard::disable_wifi() {
    low(config_.wifi_enable);
}

void CoreBoard::enable_wifi() {
    high(config_.wifi_enable);
}
