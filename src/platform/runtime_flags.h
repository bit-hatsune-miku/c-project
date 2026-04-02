#pragma once

namespace platform::runtime {

struct Flags {
    bool skipAnimationsAndWaits = false;
};

inline Flags& mutableFlags() {
    static Flags flags;
    return flags;
}

inline void setSkipAnimationsAndWaitsEnabled(bool enabled) {
    mutableFlags().skipAnimationsAndWaits = enabled;
}

inline bool skipAnimationsAndWaitsEnabled() {
    return mutableFlags().skipAnimationsAndWaits;
}

} // namespace platform::runtime
