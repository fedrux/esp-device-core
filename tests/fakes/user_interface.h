#pragma once
#include <stdint.h>
inline uint32_t os_random() { static uint32_t counter=100;return ++counter; }
