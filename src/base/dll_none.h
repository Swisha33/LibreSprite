// LibreSprite Base Library
// Copyright (c) 2026 LibreSprite contributors
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

// Fallback for platforms without dynamic library loading (e.g. PS Vita).

#include "base/string.h"

namespace base {

dll load_dll(const std::string& filename)
{
  return nullptr;
}

void unload_dll(dll lib)
{
}

dll_proc get_dll_proc_base(dll lib, const char* procName)
{
  return nullptr;
}

} // namespace base
