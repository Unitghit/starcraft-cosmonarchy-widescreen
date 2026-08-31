#pragma once

#include <cstdio>

namespace runtime_diagnostics
{
    constexpr bool Enabled()
    {
        return false;
    }

    constexpr FILE *Open(const char *, const char *)
    {
        return nullptr;
    }
}
