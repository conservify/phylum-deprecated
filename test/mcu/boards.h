#ifndef __PHYLUM_BOARDS_H_INCLUDED
#define __PHYLUM_BOARDS_H_INCLUDED

#include <cinttypes>
#include <cstddef>

struct BoardConfig {
    uint8_t spi_enable;
    uint8_t spi_flash_cs;
    uint8_t all_spi_cs[4];
    uint8_t all_enables[4];
};

class SpiWrapper {
private:
    void *ptr_;

public:
    void begin();
    void end();

private:
    SpiWrapper(void *ptr) : ptr_(ptr) {
    }

public:
    static SpiWrapper spi();

};

class TwoWireWrapper {
private:
    void *ptr_;

public:
    void begin();
    void end();

private:
    TwoWireWrapper(void *ptr) : ptr_(ptr) {
    }

public:
    static TwoWireWrapper i2c1();
    static TwoWireWrapper i2c2();

};

class Board {
private:
    BoardConfig config_;

public:
    Board(BoardConfig config) : config_(config) {
    }

public:
    SpiWrapper spi();
    TwoWireWrapper i2c1();
    TwoWireWrapper i2c2();

public:
    uint8_t flash_cs() const {
        return config_.spi_flash_cs;
    }

    void disable_spi() {
        for (auto pin : config_.all_spi_cs) {
            if (pin > 0) {
                disable_cs(pin);
            }
        }
        low(config_.spi_enable);
        spi().end();
    }

    void enable_spi() {
        for (auto pin : config_.all_spi_cs) {
            if (pin > 0) {
                enable_cs(pin);
            }
        }
        high(config_.spi_enable);
        spi().begin();
    }

public:
    virtual void disable_everything() {
        disable_spi();
        for (auto pin : config_.all_enables) {
            if (pin > 0) {
                low(pin);
            }
        }
        i2c1().end();
    }

    virtual void enable_everything() {
        enable_spi();
        for (auto pin : config_.all_enables) {
            if (pin > 0) {
                high(pin);
            }
        }
        i2c1().begin();
    }

protected:
    void low(uint8_t pin);
    void high(uint8_t pin);
    void disable_cs(uint8_t pin);
    void enable_cs(uint8_t pin);
};

struct CoreBoardConfig {
    BoardConfig config;
    uint8_t spi_sd_cs;
    uint8_t spi_wifi_cs;
    uint8_t wifi_enable;
    uint8_t gps_enable;
    uint8_t modules_enable;
};

class CoreBoard : public Board {
private:
    CoreBoardConfig config_;

public:
    CoreBoard(CoreBoardConfig config);

public:
    void disable_everything() override;
    void enable_everything() override;
    void disable_gps();
    void enable_gps();
    void disable_wifi();
    void enable_wifi();

};

extern Board sonar_board;

extern CoreBoard core_board;

#endif // __PHYLUM_BOARDS_H_INCLUDED
