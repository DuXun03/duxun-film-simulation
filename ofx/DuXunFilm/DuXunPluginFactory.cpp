/**
 * DuXunPluginFactory.cpp
 * ======================
 * Plugin factory registration for 独寻胶片模拟.
 */

#include "DuXunPlugin.h"

// Plugin is registered in DuXunPlugin.cpp via static init.
// This file ensures the object file is linked.
extern "C" {
    // Force symbol export to prevent dead-code elimination
    void __du_xun_factory_ref() {}
}
