#ifndef MEMBER_A_INTERNAL_H
#define MEMBER_A_INTERNAL_H

#include <stddef.h>

// 检查文本编码、长度和必填条件
int a_text_valid(const char *text, size_t capacity, size_t max_chars, int required);
// 检查容量后复制文本
int a_copy_text(char *destination, size_t capacity, const char *source, int required);
// 将文本转换为指定范围的整数
int a_parse_int(const char *text, int minimum, int maximum, int *out);
// 检查年月日格式和日期是否合法
int a_date_valid(const char *date);

#endif
