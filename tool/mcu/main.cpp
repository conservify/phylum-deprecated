#include <Arduino.h>

#include <phylum/phylum.h>
#include <backends/arduino_sd/arduino_sd.h>

using namespace phylum;

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

void setup() {
    Serial.begin(115200);

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

    auto mountStarted = millis();
    FileLayout<5> fs{ storage };
    if (!fs.mount(files)) {
        sdebug() << "Mount failed!" << endl;
        fail();
    }
    auto mountFinished = millis();

    sdebug() << "Mounted in " << (mountFinished - mountStarted) << "ms" << endl;

    while (true) {
        delay(10);
    }
}

void loop() {
}
