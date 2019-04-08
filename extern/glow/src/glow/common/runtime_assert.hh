#pragma once

#include <cassert>
#include "log.hh"

/// Special macro for checking runtime invariants
/// Executes REACTION parameter on error
#define GLOW_RUNTIME_ASSERT(CONDITION, MSG, REACTION)      \
    do                                                     \
    {                                                      \
        if (!(CONDITION))                                  \
        {                                                  \
            glow::error() << "Runtime violation: " << MSG; \
            assert(0);                                     \
            REACTION;                                      \
        }                                                  \
    } while (0)
