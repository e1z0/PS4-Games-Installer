#pragma once

namespace psgi {
    /**
     * @brief Platform-agnostic debug logging.
     * Routes to sceKernelDebugOutText on PS4, or stdout on host/desktop builds.
     */
    void klog(const char *fmt, ...);
}
