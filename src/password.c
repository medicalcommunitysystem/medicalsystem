#include <stdint.h>
#include <string.h>
#include "auth.h"
#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#else
#include <openssl/evp.h>
#include <openssl/rand.h>
#endif

#define PASSWORD_ITERATIONS 600000
#define SALT_BYTES 16
#define KEY_BYTES 32
#define PAYLOAD_BYTES (SALT_BYTES + KEY_BYTES)
static const char prefix[]="pbkdf2-sha256$600000$";
static const char alphabet[]=
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// 清除内存中的敏感数据
void auth_secure_clear(void *memory, size_t size)
{
    volatile unsigned char *bytes=memory;
    if (bytes != NULL) {
        while (size-- != 0) { *bytes++=0; }
    }
}

// 根据密码和盐计算派生密钥
static int derive(const char *password, const unsigned char *salt, unsigned char *key)
{
#ifdef _WIN32
    BCRYPT_ALG_HANDLE algorithm=NULL;
    NTSTATUS status=BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,
                                               NULL,BCRYPT_ALG_HANDLE_HMAC_FLAG);
    if (status < 0) { return -1; }
    status=BCryptDeriveKeyPBKDF2(algorithm,(PUCHAR)password,(ULONG)strlen(password),
        (PUCHAR)salt,SALT_BYTES,PASSWORD_ITERATIONS,key,KEY_BYTES,0);
    BCryptCloseAlgorithmProvider(algorithm,0);
    return status < 0 ? -1 : 0;
#else
    return PKCS5_PBKDF2_HMAC(password,(int)strlen(password),salt,SALT_BYTES,
        PASSWORD_ITERATIONS,EVP_sha256(),KEY_BYTES,key)==1 ? 0 : -1;
#endif
}

// 生成随机盐并保存密码哈希
int auth_hash_password(const char *password, char *output, size_t output_size)
{
    unsigned char payload[PAYLOAD_BYTES]={0};
    size_t offset=sizeof prefix-1;
    int rc=-1;
    if (output == NULL || output_size == 0) { return -1; }
    output[0]='\0';
    if (password == NULL || password[0]=='\0' ||
        strlen(password)>AUTH_PASSWORD_MAX_BYTES || output_size<offset+65) { return -1; }
#ifdef _WIN32
    if (BCryptGenRandom(NULL,payload,SALT_BYTES,
                        BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0) { goto done; }
#else
    if (RAND_bytes(payload,SALT_BYTES)!=1) { goto done; }
#endif
    if (derive(password,payload,payload+SALT_BYTES)!=0) { goto done; }
    memcpy(output,prefix,offset);
    // 将盐和密钥编码为字符串
    for (size_t i=0;i<PAYLOAD_BYTES;i+=3) {
        uint32_t value=((uint32_t)payload[i]<<16)|((uint32_t)payload[i+1]<<8)|payload[i+2];
        output[offset++]=alphabet[(value>>18)&63];
        output[offset++]=alphabet[(value>>12)&63];
        output[offset++]=alphabet[(value>>6)&63];
        output[offset++]=alphabet[value&63];
    }
    output[offset]='\0';
    rc=0;
done:
    auth_secure_clear(payload,sizeof payload);
    return rc;
}

// 验证输入密码是否匹配已存哈希
int verify_password(const char *password, const char *stored_hash)
{
    unsigned char payload[PAYLOAD_BYTES]={0},key[KEY_BYTES]={0};
    unsigned int difference=0;
    size_t prefix_size=sizeof prefix-1;
    int result=-1;
    if (password == NULL || stored_hash == NULL || password[0]=='\0' ||
        strlen(password)>AUTH_PASSWORD_MAX_BYTES ||
        strlen(stored_hash)!=prefix_size+64 ||
        strncmp(stored_hash,prefix,prefix_size)!=0) { return -1; }
    for (size_t i=0;i<64;i+=4) {
        uint32_t value=0;
        for (size_t j=0;j<4;++j) {
            const char *position=strchr(alphabet,stored_hash[prefix_size+i+j]);
            if (position == NULL) { goto done; }
            value=(value<<6)|(uint32_t)(position-alphabet);
        }
        payload[i/4*3]=(unsigned char)(value>>16);
        payload[i/4*3+1]=(unsigned char)(value>>8);
        payload[i/4*3+2]=(unsigned char)value;
    }
    if (derive(password,payload,key)!=0) { goto done; }
    for (size_t i=0;i<KEY_BYTES;++i) { difference|=key[i]^payload[SALT_BYTES+i]; }
    result=difference==0 ? 1 : 0;
done:
    auth_secure_clear(payload,sizeof payload);
    auth_secure_clear(key,sizeof key);
    return result;
}
