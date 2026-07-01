#pragma once

#ifdef _MSC_VER

#define __attribute__(x)
#define PACKED_BEGIN __pragma(pack(push, 1))
#define PACKED_END   __pragma(pack(pop))

#define UNREACHABLE __assume(0)

#else

#define PACKED_BEGIN
#define PACKED_END

#define UNREACHABLE __builtin_unreachable()

#endif
