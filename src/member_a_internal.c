#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "member_a_internal.h"
#include "common.h"

// 检查文本编码、长度和必填条件
int a_text_valid(const char *text, size_t capacity, size_t max_chars, int required)
{
    const char *end;
    size_t length, offset = 0, count = 0;
    if (text == NULL || capacity == 0 || (end = memchr(text, '\0', capacity)) == NULL) {
        return 0;
    }
    length = (size_t)(end - text);
    if (required && length == 0) {
        return 0;
    }
    while (offset < length) {
        unsigned char first = (unsigned char)text[offset];
        uint32_t point;
        size_t width;
        if (first <= 0x7f) { width = 1; point = first; }
        else if (first >= 0xc2 && first <= 0xdf) { width = 2; point = first & 0x1f; }
        else if (first >= 0xe0 && first <= 0xef) { width = 3; point = first & 0x0f; }
        else if (first >= 0xf0 && first <= 0xf4) { width = 4; point = first & 0x07; }
        else { return 0; }
        if (width > length - offset) { return 0; }
        for (size_t i = 1; i < width; ++i) {
            unsigned char next = (unsigned char)text[offset + i];
            if ((next & 0xc0) != 0x80) { return 0; }
            point = (point << 6) | (next & 0x3f);
        }
        if ((width == 3 && point < 0x800) || (width == 4 && point < 0x10000) ||
            (point >= 0xd800 && point <= 0xdfff) || point > 0x10ffff ||
            ++count > max_chars) { return 0; }
        offset += width;
    }
    return 1;
}

// 检查容量后复制文本
int a_copy_text(char *destination, size_t capacity, const char *source, int required)
{
    size_t length;
    if (destination == NULL || capacity == 0) { return ERR_PARAM; }
    if (source == NULL) { source = ""; }
    length = strlen(source);
    if ((required && length == 0) || length >= capacity) { return ERR_DB_RESULT; }
    memcpy(destination, source, length + 1);
    return ERR_OK;
}

// 将文本转换为指定范围的整数
int a_parse_int(const char *text, int minimum, int maximum, int *out)
{
    char *end;
    long value;
    if (out == NULL || minimum > maximum) { return ERR_PARAM; }
    if (text == NULL || text[0] == '\0') { return ERR_DB_RESULT; }
    errno = 0;
    value = strtol(text, &end, 10);
    if (end == text || *end != '\0' || errno == ERANGE ||
        value < minimum || value > maximum) { return ERR_DB_RESULT; }
    *out = (int)value;
    return ERR_OK;
}

// 检查日期格式及闰年天数
int a_date_valid(const char *date)
{
    static const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    int year, month, day, limit;
    if (date == NULL || strlen(date) != 10 || date[4] != '-' || date[7] != '-') {
        return 0;
    }
    for (int i = 0; i < 10; ++i) {
        if (i != 4 && i != 7 && (date[i] < '0' || date[i] > '9')) { return 0; }
    }
    year = (date[0]-'0')*1000 + (date[1]-'0')*100 + (date[2]-'0')*10 + date[3]-'0';
    month = (date[5]-'0')*10 + date[6]-'0';
    day = (date[8]-'0')*10 + date[9]-'0';
    if (year < 1000 || month < 1 || month > 12) { return 0; }
    limit = days[month-1] + (month == 2 && year%4 == 0 && (year%100 != 0 || year%400 == 0));
    return day >= 1 && day <= limit;
}
