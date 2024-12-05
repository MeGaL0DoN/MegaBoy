#pragma once

#define ST_WRITE(var) st.write(reinterpret_cast<const char*>(&var), sizeof(var))
#define ST_WRITE_ARR(var) st.write(reinterpret_cast<const char*>(var.data()), sizeof(var))

#define ST_READ(var) st.read(reinterpret_cast<char*>(&var), sizeof(var))
#define ST_READ_ARR(var) st.read(reinterpret_cast<char*>(var.data()), sizeof(var))

#include <cassert>

#if defined(_MSC_VER)
#define UNREACHABLE() assert(false); __assume(false);
#elif defined(__GNUC__) || defined(__clang__)
#define UNREACHABLE() assert(false); __builtin_unreachable();
#else
#define UNREACHABLE() assert(false)
#endif

#ifdef _WIN32
#define STR(s) L##s
#else
#define STR(s) s
#endif