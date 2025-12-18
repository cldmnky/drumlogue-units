#pragma once
// Shim to neutralize unit header section attributes for native builds

#define __unit_callback __attribute__((used))
#define __unit_header __attribute__((used))
