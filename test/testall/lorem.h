#pragma once

class LoremIpsum {
private:
    const char *words_[95] { "ipsum", "semper", "habeo", "duo", "ut", "vis",
            "aliquyam", "eu", "splendide", "Ut", "mei", "eteu", "nec",
            "antiopam", "corpora", "kasd", "pretium", "cetero", "qui", "arcu",
            "assentior", "ei", "his", "usu", "invidunt", "kasd", "justo", "ne",
            "eleifend", "per", "ut", "eam", "graeci", "tincidunt", "impedit",
            "temporibus", "duo", "et", "facilisis", "insolens", "consequat",
            "cursus", "partiendo", "ullamcorper", "Vulputate", "facilisi",
            "donec", "aliquam", "labore", "inimicus", "voluptua", "penatibus",
            "sea", "vel", "amet", "his", "ius", "audire", "in", "mea",
            "repudiandae", "nullam", "sed", "assentior", "takimata", "eos",
            "at", "odio", "consequat", "iusto", "imperdiet", "dicunt",
            "abhorreant", "adipisci", "officiis", "rhoncus", "leo", "dicta",
            "vitae", "clita", "elementum", "mauris", "definiebas", "uonsetetur",
            "te", "inimicus", "nec", "mus", "usu", "duo", "aenean", "corrumpit",
            "aliquyam", "est", "eum" };

public:
    const char *word() const {
        #if defined(ARDUINO)
        auto n = random(0, sizeof(words_) / sizeof(const char *));
        #else
        auto n = random() % (sizeof(words_) / sizeof(const char *));
        #endif
        return words_[n];
    }

    size_t sentence(char *s, size_t n) const {
        #if defined(ARDUINO)
        auto nwords = random(4, 8);
        #else
        auto nwords = random() % 4 + 4;
        #endif
        auto len = (size_t)0;

        for (auto i = 0; i < nwords; ++i) {
            const char *w = word();
            auto wlen = strlen(w);

            if (len + wlen + 1 >= n) {
                break;
            }

            strcpy(s + len, w);
            len += wlen;
            s[len] = ' ';
            len += 1;
        }

        s[0] = toupper(s[0]);
        s[len - 1] = '.';
        s[len    ] = 0;

        return len;
    }

};
