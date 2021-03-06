/*
 * Copyright (C) 2018-2020 The LineageOS Project
 * Copyright (C) 2020      Andreas Schneider <asn@cryptomilk.org>
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "power@1.0-exynos7904"

#include <android-base/logging.h>

#include <android-base/file.h>
#include <android-base/properties.h>
#include <android-base/strings.h>

#include "Power.h"
#include <tsp.h>

#include <iostream>

namespace android {
namespace hardware {
namespace power {
namespace V1_0 {
namespace implementation {

using ::android::hardware::power::V1_0::Feature;
using ::android::hardware::power::V1_0::PowerHint;
using ::android::hardware::power::V1_0::PowerStatePlatformSleepState;
using ::android::hardware::power::V1_0::Status;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;

#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))

static std::string interactive_node_paths[] = {
    "/sys/class/sec/tsp/input/enabled",
    "/sys/class/power_supply/battery/lcd"
};

template <typename T>
static void writeNode(const std::string& path, const T& value)
{
    std::ofstream node(path);
    if (!node) {
        PLOG(ERROR) << "Failed to open: " << path;
        return;
    }

    LOG(DEBUG) << "writeNode node: " << path << " value: " << value;

    node << value << std::endl;
    if (!node) {
        PLOG(ERROR) << "Failed to write: " << value;
    }
}

static bool doesNodeExist(const std::string& path)
{
    std::ifstream f(path.c_str());
    if (f.good()) {
        LOG(DEBUG) << "Found node: " << path;
    }
    return f.good();
}

Power::Power() {
    size_t inode_size = ARRAY_SIZE(interactive_node_paths);

    //android::base::SetMinimumLogSeverity(android::base::VERBOSE);

    mInteractionHandler.Init();
    tsp_init();

    LOG(DEBUG) << "Looking for touchsceen/lcd nodes";
    for (size_t i = 0; i < inode_size; i++) {
        std::string node_path = interactive_node_paths[i];

        if (doesNodeExist(node_path)) {
            mInteractiveNodes.push_back(node_path);
        }
    }

    mCurrentProfile = PowerProfile::BALANCED;
}

// Methods from ::android::hardware::power::V1_0::IPower follow.
Return<void> Power::setInteractive(bool interactive) {
    /*
     * Scaling the BIG cluster doesn't seem to work. So only
     * scale down the default one.
     */
    writeNode("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq",
              interactive ? "1586000" : "1014000");

    /* Enable dt2w before turning TSP off */
    if (mDoubleTapEnabled && !interactive) {
        tsp_enable_doubletap();
    }

    for (size_t i = 0; i < mInteractiveNodes.size(); i++) {
        writeNode(mInteractiveNodes[i], interactive ? "1" : "0");
    }

    /* Disable dt2w after turning TSP back on */
    if (mDoubleTapEnabled && interactive) {
        tsp_disable_doubletap();
    }

    return Void();
}

Return<void> Power::powerHint(PowerHint hint, int32_t data __unused) {
    /* Bail out if low-power mode is active */
    if (mCurrentProfile == PowerProfile::POWER_SAVE && hint != PowerHint::LOW_POWER &&
        hint != static_cast<PowerHint>(LineagePowerHint::SET_PROFILE)) {
        LOG(VERBOSE) << "PROFILE_POWER_SAVE active, ignoring hint " << static_cast<int32_t>(hint);
        return Void();
    }

    switch (hint) {
#if 0
    /*
     * Setting CPU frequencies don't really seem to work. The same issues exist
     * with as CPU hot plugging. You turn CPU off and it wont boot again.
     */
    case PowerHint::INTERACTION:
        mInteractionHandler.Acquire(data);
        break;
#endif
    case PowerHint::LOW_POWER:
        LOG(DEBUG) << "PowerHint: LOW_POWER";
        break;
    default:
        if (hint == static_cast<PowerHint>(LineagePowerHint::SET_PROFILE)) {
            LOG(DEBUG) << "LineagePowerHint: SET_PROFILE=" << data;
        }
        break;
    }

    return Void();
}

Return<void> Power::setFeature(Feature feature, bool activate)
{
    switch (feature) {
    case Feature::POWER_FEATURE_DOUBLE_TAP_TO_WAKE:
        if (activate) {
            LOG(INFO) << "Enable double tap to wake";
            mDoubleTapEnabled = true;
        } else {
            LOG(INFO) << "Disable double tap to wake";
            mDoubleTapEnabled = false;
        }
        break;
    default:
        break;
    }

    return Void();
}

Return<void> Power::getPlatformLowPowerStats(getPlatformLowPowerStats_cb _hidl_cb) {
    hidl_vec<PowerStatePlatformSleepState> states;
    _hidl_cb(states, Status::SUCCESS);
    return Void();
}

Return<int32_t> Power::getFeature(LineageFeature feature) {
    switch (feature) {
    case LineageFeature::SUPPORTED_PROFILES:
        return static_cast<int32_t>(PowerProfile::MAX);
    default:
        break;
    }

    return -1;
}

void Power::setProfile(PowerProfile profile) {
    if (mCurrentProfile == profile) {
        return;
    }

    switch (profile) {
    case PowerProfile::POWER_SAVE:
        writeNode("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq",
                  "1144000");
        break;
    case PowerProfile::BALANCED:
    case PowerProfile::HIGH_PERFORMANCE:
        writeNode("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq",
                  "1586000");
        break;
    default:
        break;
    }
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace power
}  // namespace hardware
}  // namespace android
