#ifndef PHX_DETAIL_H
#define PHX_DETAIL_H

#if defined(__cplusplus)
    #define PHX_DETAIL_CPP


    #if __cplusplus >= 202302L
        #define PHX_DETAIL_CPP23
    #endif

    #if __cplusplus >= 202002L
        #define PHX_DETAIL_CPP20
    #endif

    #if __cplusplus >= 201703L
        #define PHX_DETAIL_CPP17
    #endif

    #if __cplusplus >= 201402L
        #define PHX_DETAIL_CPP14
    #endif

    #if __cplusplus >= 201103L
        #define PHX_DETAIL_CPP11
    #endif

    #define PHX_DETAIL_CPP98

#elif defined(__STDC_VERSION__)
    #define PHX_DETAIL_C


    #if __STDC_VERSION__ >= 202311L
        #define PHX_DETAIL_C23
    #endif

    #if __STDC_VERSION__ >= 201710L
        #define PHX_DETAIL_C17
    #endif

    #if __STDC_VERSION__ >= 201112L
        #define PHX_DETAIL_C11
    #endif

    #if __STDC_VERSION__ >= 199901L
        #define PHX_DETAIL_C99
    #endif

#else
    #error "Invalid language or C89/C90"
#endif


#if defined(__clang__)
    #define PHX_DETAIL_CLANG

#elif defined(_MSC_VER)
    #define PHX_DETAIL_MSVC

#elif defined(__GNUC__)
    #define PHX_DETAIL_GCC

#else
    #error "Unsupported toolchain"
#endif


#if defined(__i386__) || defined(_M_IX86)
    #define PHX_DETAIL_X86

#elif defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64)
    #define PHX_DETAIL_X86_64

#elif defined(__arm__) || defined(_M_ARM)
    #define PHX_DETAIL_ARM32

#elif defined(__aarch64__) || defined(_M_ARM64)
    #define PHX_DETAIL_ARM64
    
#else
    #error "Unsupported architecture"
#endif


#if defined(_WIN32) || defined(_WIN64)
    #define PHX_DETAIL_WINDOWS

#elif defined(__APPLE__)
    #define PHX_DETAIL_MACOS

#elif defined(__linux__)
    #define PHX_DETAIL_LINUX

#else
    #error "Unsupported platform"
#endif


#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define PHX_DETAIL_LITTLE_ENDIAN
#elif defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define PHX_DETAIL_BIG_ENDIAN
#else
#error "Unsupported endianness (or toolchain)"
#endif




#define PHX_INTERNAL_CONCAT_(a, b) a##b
#define PHX_INTERNAL_CONCAT(a, b) PHX_INTERNAL_CONCAT_(a, b)

#define PHX_INTERNAL_UNIQUE_NAME(base) \
    PHX_INTERNAL_CONCAT(PHX_INTERNAL_CONCAT(PHX_INTERNAL_UNIQUE_NAME_, base), __LINE__)




#if defined(PHX_DETAIL_GCC) || defined (PHX_DETAIL_CLANG)
    #define PHX_DETAIL_WEAK_FUNCTION(ret, name, params, body) \
        __attribute__((weak)) ret name params body

#elif defined(PHX_DETAIL_MSVC)

#   if defined(PHX_DETAIL_X86) || defined(PHX_DETAIL_ARM32)
#       define PHX_DETAIL_WEAK_FUNCTION(ret, name, params, body) \
            __pragma(comment(linker, "/alternatename:_" #name "=_" #name "_default")) \
            ret name##_default params body

#   elif defined(PHX_DETAIL_X86_64) || defined(PHX_DETAIL_ARM64)
#       define PHX_DETAIL_WEAK_FUNCTION(ret, name, params, body) \
            __pragma(comment(linker, "/alternatename:" #name "=" #name "_default")) \
            ret name##_default params body

#   endif

#endif

#if defined(PHX_DETAIL_CPP11)

#   define PHX_DETAIL_STATIC_ASSERT(cond, msg) static_assert(cond, msg)

#elif defined(PHX_DETAIL_C11)

#   define PHX_DETAIL_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)

#else

#   define PHX_DETAIL_STATIC_ASSERT(cond, msg) typedef char PHX_INTERNAL_UNIQUE_NAME(STATIC_ASSERT_FAILED)[(cond) ? 1 : -1]

#endif



#endif
