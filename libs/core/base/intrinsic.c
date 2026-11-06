#include <phx/base/intrinsic.h>
#include <phx/detail.h>

PHX_DETAIL_WEAK_FUNCTION(
int, memcmp, (const void* s1, const void* s2, PHX_Size n),
{
    const unsigned char* p1 = (const unsigned char*)s1;
    const unsigned char* p2 = (const unsigned char*)s2;

    for (PHX_Size i = 0; i < n; i++)
    {
        if (p1[i] != p2[i])
            return (int)p1[i] - (int)p2[i];
    }

    return 0;
}
)

PHX_DETAIL_WEAK_FUNCTION(
void*, memcpy, (void* dst, const void* src, PHX_Size n),
{
    unsigned char* destination = (unsigned char*)dst;
    const unsigned char* source = (const unsigned char*)src;

    for (PHX_Size i = 0; i < n; i++)
        destination[i] = source[i];

    return dst;
}
)

PHX_DETAIL_WEAK_FUNCTION(
void*, memmove, (void* dst, const void* src, PHX_Size n),
{
    unsigned char* destination = (unsigned char*)dst;
    const unsigned char* source = (const unsigned char*)src;

    if (destination < source)
    {
        for (PHX_Size i = 0; i < n; i++)
            destination[i] = source[i];
    }
    else if (destination > source)
    {
        for (PHX_Size i = n; i > 0; i--)
            destination[i - 1] = source[i - 1];
    }

    return dst;
}
)

PHX_DETAIL_WEAK_FUNCTION(
void*, memset, (void* dst, int c, PHX_Size n),
{
    unsigned char* destination = (unsigned char*)dst;
    const unsigned char ch = (unsigned char)c;

    for (PHX_Size i = 0; i < n; i++)
        destination[i] = ch;

    return dst;
}
)

PHX_DETAIL_WEAK_FUNCTION(
int, strcmp, (const char* s1, const char* s2),
{
    while (*s1 && *s1 == *s2)
    {
        s1++;
        s2++;
    }

    return (int)(unsigned char)*s1 - (int)(unsigned char)*s2;
}
)

PHX_DETAIL_WEAK_FUNCTION(
PHX_Size, strlen, (const char* s),
{
    PHX_Size len = 0;
    while (s[len]) len++;
    return len;
}
)
