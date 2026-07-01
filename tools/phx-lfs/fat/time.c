#include "fat.h"

#define SECS_PER_DAY 86400
#define SECS_PER_HOUR 3600
#define SECS_PER_MIN 60

static inline int is_leap_year(uint32_t year) {
    return (year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0));
}

void FAT_EncodeTime(int64_t epoch, uint16_t* fat_date, uint16_t* fat_time, uint8_t* tenths)
{
    const int64_t FAT_EPOCH = 315532800ULL;
    if (epoch < FAT_EPOCH) epoch = FAT_EPOCH;

    uint64_t secs = (uint64_t)(epoch - FAT_EPOCH);

    uint32_t days = (uint32_t)(secs / SECS_PER_DAY);
    uint32_t rem = (uint32_t)(secs % SECS_PER_DAY);

    uint16_t hour = (uint16_t)(rem / SECS_PER_HOUR); rem %= SECS_PER_HOUR;
    uint16_t min  = (uint16_t)(rem / SECS_PER_MIN); rem %= SECS_PER_MIN;
    uint16_t sec  = (uint16_t)rem;

    *fat_time = (uint16_t)((hour << 11) | (min << 5) | (sec / 2));
    *tenths   = (sec % 2) ? 50 : 0;

    uint32_t y = 1980;
    while (1) {
        uint32_t days_in_year = is_leap_year(y) ? 366 : 365;
        if (days < days_in_year) break;
        days -= days_in_year;
        y++;
    }

    static const uint8_t dim[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    uint32_t m = 0;
    while (1) {
        uint32_t dim_month = dim[m];
        if (m == 1 && is_leap_year(y)) dim_month++;
        if (days < dim_month) break;
        days -= dim_month;
        m++;
    }
    uint32_t d = days + 1;

    *fat_date = (uint16_t)(((y - 1980) << 9) | ((m + 1) << 5) | d);
}
