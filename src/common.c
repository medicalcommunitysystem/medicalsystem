#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>
#include "member_b.h"

int common_valid_text(const char *s, size_t max_chars, int required)
{
    size_t chars = 0;
    const unsigned char *p = (const unsigned char *)s;
    if (!s || (required && !s[0])) return 0;
    while (*p) {
        unsigned int cp;
        unsigned int n, i;
        if (*p < 0x80) { cp=*p; n=1; }
        else if (*p >= 0xc2 && *p <= 0xdf) { cp=*p & 31; n=2; }
        else if (*p >= 0xe0 && *p <= 0xef) { cp=*p & 15; n=3; }
        else if (*p >= 0xf0 && *p <= 0xf4) { cp=*p & 7; n=4; }
        else return 0;
        for (i=1; i<n; i++) {
            if (!p[i] || (p[i] & 0xc0) != 0x80) return 0;
            cp = (cp << 6) | (p[i] & 63);
        }
        if ((n==2 && cp<0x80) || (n==3 && cp<0x800) || (n==4 && cp<0x10000) ||
            (cp>=0xd800 && cp<=0xdfff) || cp>0x10ffff ||
            (cp<32 && cp!='\n' && cp!='\t') || cp==127) return 0;
        if (++chars > max_chars) return 0;
        p += n;
    }
    return 1;
}

void common_copy(char *out, size_t cap, const char *s)
{
    if (out && cap) snprintf(out, cap, "%s", s ? s : "");
}

int common_read_line(const char *prompt, char *out, size_t cap)
{
    int c;
    size_t len;
    if (!out || cap<2 || cap>INT_MAX) return ERR_PARAM;
    if (prompt) { fputs(prompt, stdout); fflush(stdout); }
    if (!fgets(out, (int)cap, stdin)) return B_ERR_INPUT;
    len=strlen(out);
    if (len && out[len-1]=='\n') out[--len]='\0';
    else {
        c=getchar();
        if (c!='\n' && c!=EOF) {
            while ((c=getchar())!='\n' && c!=EOF) {}
            out[0]='\0'; return B_ERR_LIMIT;
        }
    }
    if (len && out[len-1]=='\r') out[len-1]='\0';
    return ERR_OK;
}

int common_read_int(const char *prompt, int min, int max, int *out)
{
    char s[80], *end;
    long n;
    int rc;
    if (!out || min>max) return ERR_PARAM;
    if ((rc=common_read_line(prompt,s,sizeof(s)))<0) return rc;
    errno=0; n=strtol(s,&end,10);
    if (end==s) return ERR_PARAM;
    while (isspace((unsigned char)*end)) end++;
    if (errno || *end || n<min || n>max) return ERR_PARAM;
    *out=(int)n; return ERR_OK;
}

int common_read_double(const char *prompt, double min, double max, double *out)
{
    char s[80], *end;
    double n;
    int rc;
    if (!out || min>max) return ERR_PARAM;
    if ((rc=common_read_line(prompt,s,sizeof(s)))<0) return rc;
    errno=0; n=strtod(s,&end);
    if (end==s) return ERR_PARAM;
    while (isspace((unsigned char)*end)) end++;
    if (errno || *end || !isfinite(n) || n<min || n>max) return ERR_PARAM;
    *out=n; return ERR_OK;
}

const char *b_error_message(int code)
{
    switch (code) {
    case ERR_OK: return "成功";
    case ERR_PARAM: return "参数无效、必填项为空或字段超长";
    case ERR_NOMEM: return "内存不足";
    case ERR_NOT_FOUND: return "记录不存在";
    case ERR_DB_DUP: return "编号、账号或关联记录重复";
    case ERR_NO_PERMISSION: return "无操作权限或账号已停用";
    case ERR_INVALID_STATE: return "当前状态不允许操作";
    case B_ERR_LIMIT: return "输入、编号范围或查询结果超过限制";
    case B_ERR_EXPIRED: return "病历已超过创建后二十四小时，禁止修改";
    case B_ERR_AUTH: return "账号、密码或角色不正确";
    case B_ERR_INPUT: return "输入已结束";
    default: return "数据库操作失败（查看详细错误）";
    }
}
