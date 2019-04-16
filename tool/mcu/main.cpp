#include <Arduino.h>

#include <phylum/phylum.h>
#include <backends/arduino_sd/arduino_sd.h>

using namespace phylum;

constexpr const char LogName[] = "Read";

using Log = SimpleLog<LogName>;

static void fail() {
    Serial.println("Fail!");
    while (true) {
        delay(100);
    }
}

extern "C" char *sbrk(int32_t i);

static uint32_t free_memory() {
    char stack_dummy = 0;
    return &stack_dummy - sbrk(0);
}

struct PrettyBytes {
    uint64_t bytes;
};

inline ostreamtype& operator<<(ostreamtype& os, const PrettyBytes &b) {
    constexpr uint64_t OneMegabyte = (1024 * 1024);
    constexpr uint64_t OneKilobyte = (1024);

    int32_t mbs = 0;
    int32_t kbs = 0;

    if (b.bytes > OneMegabyte) {
        mbs = b.bytes / OneMegabyte;
        os << mbs << "MB";
    }
    else if (b.bytes > OneKilobyte) {
        kbs = b.bytes / OneKilobyte;
        os << kbs << "k";
    }
    else {
        os << b.bytes << "bytes";
    }

    return os;
}

constexpr uint8_t WIFI_PIN_CS = 7;
constexpr uint8_t SD_PIN_CS = 12;
constexpr uint8_t FLASH_PIN_CS = 26u;
constexpr uint8_t PERIPHERALS_ENABLE_PIN = 25u;
constexpr uint8_t RFM95_PIN_CS = 5;

void board() {
    pinMode(PERIPHERALS_ENABLE_PIN, OUTPUT);
    digitalWrite(PERIPHERALS_ENABLE_PIN, LOW);

    pinMode(FLASH_PIN_CS, OUTPUT);
    pinMode(SD_PIN_CS, OUTPUT);
    pinMode(WIFI_PIN_CS, OUTPUT);
    pinMode(RFM95_PIN_CS, OUTPUT);

    digitalWrite(FLASH_PIN_CS, HIGH);
    digitalWrite(SD_PIN_CS, HIGH);
    digitalWrite(WIFI_PIN_CS, HIGH);
    digitalWrite(RFM95_PIN_CS, HIGH);

    digitalWrite(PERIPHERALS_ENABLE_PIN, LOW);
    delay(100);
    digitalWrite(PERIPHERALS_ENABLE_PIN, HIGH);
}

void setup() {
    Serial.begin(115200);

    board();

    while (!Serial) {
        delay(10);
    }

    sdebug() << "Starting: " << free_memory() << endl;

    #if defined(PHYLUM_READ_ONLY)
    sdebug() << "Read Only" << endl;
    #endif

    Geometry g{ 0, 4, 4, SectorSize };
    ArduinoSdBackend storage;
    if (!storage.initialize(g, 12)) {
        sdebug() << "Failed, trying CS = 4" << endl;
        if (!storage.initialize(g, 4)) {
            fail();
        }
    }

    if (!storage.open()) {
        fail();
    }

    sdebug() << "Initialize FS" << endl;

    FileDescriptor file_system_area_fd = { "system",          100  };
    FileDescriptor file_emergency_fd   = { "emergency.fklog", 100  };
    FileDescriptor file_logs_a_fd =      { "logs-a.fklog",    2048 };
    FileDescriptor file_logs_b_fd =      { "logs-b.fklog",    2048 };
    FileDescriptor file_data_fk =        { "data.fk",         0    };

    static FileDescriptor* files[] = {
        &file_system_area_fd,
        &file_emergency_fd,
        &file_logs_a_fd,
        &file_logs_b_fd,
        &file_data_fk
    };

    FileLayout<5> fs{ storage };
    if (!fs.mount(files)) {
        sdebug() << "Mounting failed!" << endl;
        fail();
    }

    for (auto fd : files) {
        sdebug() << "Opening: " << fd->name << endl;

        auto opened = fs.open(*fd);
        if (opened) {
            sdebug() << "File: " << fd->name << " size = " << opened.size() << " maximum = " << fd->maximum_size << endl;

            if (!opened.seek(UINT64_MAX)) {
            }

            if (!opened.seek(0)) {
            }

            auto total_read = (uint32_t)0;
            auto started = millis();

            while (true) {
                uint8_t buffer[1024];

                auto read = opened.read(buffer, sizeof(buffer));
                if (read == 0){
                    break;
                }

                total_read += read;
            }

            auto elapsed = millis() - started;

            sdebug() << "Read: " << total_read << " bytes " << elapsed << "ms" << endl;
        }
    }

    sdebug() << "Done!" << endl;

    while (true) {
        delay(10);
    }
}

void loop() {
}
