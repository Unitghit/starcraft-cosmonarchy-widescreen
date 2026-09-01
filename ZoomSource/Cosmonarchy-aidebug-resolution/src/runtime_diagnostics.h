#pragma once

#include <cstdio>

#define RUNTIME_DIAGNOSTICS_ENABLED 0

namespace runtime_diagnostics
{
    constexpr bool Enabled()
    {
        return false;
    }

    inline FILE *Open(const char *path, const char *mode)
    {
        (void)path;
        (void)mode;
        return nullptr;
    }
}
