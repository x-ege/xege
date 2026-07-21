#pragma once

#ifndef EGE_ENABLE_PERFORMANCE_DIAGNOSTICS
#define EGE_ENABLE_PERFORMANCE_DIAGNOSTICS 0
#endif

namespace ege {
namespace detail {

enum class PerformanceDiagnosticCode {
    LegacyWritableBufferFullUpload,
    RepeatedGpuReadback
};

struct PerformanceDiagnosticContext {
    int width;
    int height;
    unsigned int occurrenceCount;
    unsigned int intervalMilliseconds;

    PerformanceDiagnosticContext(
        int imageWidth, int imageHeight,
        unsigned int occurrences = 0,
        unsigned int intervalMs = 0)
        : width(imageWidth), height(imageHeight),
          occurrenceCount(occurrences),
          intervalMilliseconds(intervalMs) {}
};

bool performanceDiagnosticsCompiled();
void reportPerformanceDiagnostic(
    PerformanceDiagnosticCode code,
    const PerformanceDiagnosticContext& context);

} // namespace detail
} // namespace ege
