#include "crypto.hpp"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>
#include <fcntl.h>
#include <unistd.h>

#ifdef __APPLE__
#  include <CommonCrypto/CommonDigest.h>
#  include <CommonCrypto/CommonHMAC.h>
#endif

extern "C" {
#include "../quickjs/quickjs.h"
}

// ── base64 encode ─────────────────────────────────────────────────────────────
static std::string base64_encode(const uint8_t *data, size_t len) {
    static const char *T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    for (size_t i = 0; i < len; i += 3) {
        uint32_t v = (uint32_t)data[i] << 16;
        if (i+1 < len) v |= (uint32_t)data[i+1] << 8;
        if (i+2 < len) v |= (uint32_t)data[i+2];
        out += T[(v >> 18) & 63];
        out += T[(v >> 12) & 63];
        out += (i+1 < len) ? T[(v >> 6) & 63] : '=';
        out += (i+2 < len) ? T[v & 63]        : '=';
    }
    return out;
}

static std::string to_hex(const uint8_t *data, size_t len) {
    static const char hex[] = "0123456789abcdef";
    std::string out(len * 2, '\0');
    for (size_t i = 0; i < len; i++) {
        out[i*2]   = hex[data[i] >> 4];
        out[i*2+1] = hex[data[i] & 0xf];
    }
    return out;
}

// ── Portable SHA-256 (used as fallback on non-Apple) ─────────────────────────
#ifndef __APPLE__
static const uint32_t SHA256_K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};
#define RR(v,n) (((v)>>(n))|((v)<<(32-(n))))
static void sha256_block(uint32_t h[8], const uint8_t block[64]) {
    uint32_t w[64], a,b,c,d,e,f,g,hh,t1,t2;
    for(int i=0;i<16;i++) w[i]=((uint32_t)block[i*4]<<24)|((uint32_t)block[i*4+1]<<16)|((uint32_t)block[i*4+2]<<8)|block[i*4+3];
    for(int i=16;i<64;i++) w[i]=(RR(w[i-2],17)^RR(w[i-2],19)^(w[i-2]>>10))+w[i-7]+(RR(w[i-15],7)^RR(w[i-15],18)^(w[i-15]>>3))+w[i-16];
    a=h[0];b=h[1];c=h[2];d=h[3];e=h[4];f=h[5];g=h[6];hh=h[7];
    for(int i=0;i<64;i++){
        t1=hh+(RR(e,6)^RR(e,11)^RR(e,25))+((e&f)^(~e&g))+SHA256_K[i]+w[i];
        t2=(RR(a,2)^RR(a,13)^RR(a,22))+((a&b)^(a&c)^(b&c));
        hh=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
    }
    h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=hh;
}
static void sha256(const uint8_t *data, size_t len, uint8_t out[32]) {
    uint32_t h[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    uint64_t bits = (uint64_t)len * 8;
    uint8_t block[64]; int bl=0;
    auto flush=[&](){ sha256_block(h,block); bl=0; };
    for(size_t i=0;i<len;i++){ block[bl++]=(uint8_t)data[i]; if(bl==64)flush(); }
    block[bl++]=0x80; if(bl>56){ memset(block+bl,0,64-bl); flush(); }
    memset(block+bl,0,56-bl);
    for(int i=7;i>=0;i--) block[56+7-i]=(uint8_t)(bits>>(i*8));
    flush();
    for(int i=0;i<8;i++){ out[i*4]=(uint8_t)(h[i]>>24);out[i*4+1]=(uint8_t)(h[i]>>16);out[i*4+2]=(uint8_t)(h[i]>>8);out[i*4+3]=(uint8_t)h[i]; }
}

// Simple MD5 (RFC 1321)
static const uint32_t MD5_T[64]={
    0xd76aa478,0xe8c7b756,0x242070db,0xc1bdceee,0xf57c0faf,0x4787c62a,0xa8304613,0xfd469501,
    0x698098d8,0x8b44f7af,0xffff5bb1,0x895cd7be,0x6b901122,0xfd987193,0xa679438e,0x49b40821,
    0xf61e2562,0xc040b340,0x265e5a51,0xe9b6c7aa,0xd62f105d,0x02441453,0xd8a1e681,0xe7d3fbc8,
    0x21e1cde6,0xc33707d6,0xf4d50d87,0x455a14ed,0xa9e3e905,0xfcefa3f8,0x676f02d9,0x8d2a4c8a,
    0xfffa3942,0x8771f681,0x6d9d6122,0xfde5380c,0xa4beea44,0x4bdecfa9,0xf6bb4b60,0xbebfbc70,
    0x289b7ec6,0xeaa127fa,0xd4ef3085,0x04881d05,0xd9d4d039,0xe6db99e5,0x1fa27cf8,0xc4ac5665,
    0xf4292244,0x432aff97,0xab9423a7,0xfc93a039,0x655b59c3,0x8f0ccc92,0xffeff47d,0x85845dd1,
    0x6fa87e4f,0xfe2ce6e0,0xa3014314,0x4e0811a1,0xf7537e82,0xbd3af235,0x2ad7d2bb,0xeb86d391
};
static const uint8_t MD5_S[64]={7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21};
#define RL(v,n) (((v)<<(n))|((v)>>(32-(n))))
static void md5(const uint8_t *data, size_t len, uint8_t out[16]) {
    uint32_t a0=0x67452301,b0=0xefcdab89,c0=0x98badcfe,d0=0x10325476;
    size_t padlen = ((len+8)/64+1)*64;
    std::string buf(padlen, 0);
    memcpy(&buf[0], data, len);
    buf[len] = (char)0x80;
    uint64_t bits=(uint64_t)len*8;
    for(int i=0;i<8;i++) buf[padlen-8+i]=(char)(bits>>(i*8));
    for(size_t off=0;off<padlen;off+=64){
        const uint8_t *blk=(const uint8_t*)&buf[off];
        uint32_t M[16],a=a0,b=b0,c=c0,d=d0;
        for(int i=0;i<16;i++) M[i]=((uint32_t)blk[i*4])|(((uint32_t)blk[i*4+1])<<8)|(((uint32_t)blk[i*4+2])<<16)|(((uint32_t)blk[i*4+3])<<24);
        for(int i=0;i<64;i++){
            uint32_t F,g;
            if(i<16){F=(b&c)|(~b&d);g=(uint32_t)i;}
            else if(i<32){F=(d&b)|(~d&c);g=(5*(uint32_t)i+1)%16;}
            else if(i<48){F=b^c^d;g=(3*(uint32_t)i+5)%16;}
            else{F=c^(b|~d);g=(7*(uint32_t)i)%16;}
            F+=a+MD5_T[i]+M[g]; a=d;d=c;c=b;b+=RL(F,MD5_S[i]);
        }
        a0+=a;b0+=b;c0+=c;d0+=d;
    }
    uint32_t r[4]={a0,b0,c0,d0};
    for(int i=0;i<4;i++) for(int j=0;j<4;j++) out[i*4+j]=(uint8_t)(r[i]>>(j*8));
}
#endif // !__APPLE__

// ── __crypto_hash(algo, data, encoding) ──────────────────────────────────────
static JSValue js_crypto_hash(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
    if (argc < 2) return JS_ThrowTypeError(ctx, "crypto_hash: needs (algo, data, encoding)");
    const char *algo = JS_ToCString(ctx, argv[0]);
    const char *data = JS_ToCString(ctx, argv[1]);
    const char *enc  = argc >= 3 ? JS_ToCString(ctx, argv[2]) : nullptr;
    if (!algo || !data) { JS_FreeCString(ctx, algo); JS_FreeCString(ctx, data); return JS_EXCEPTION; }

    bool use_base64 = enc && strcmp(enc, "base64") == 0;
    JS_FreeCString(ctx, enc);

    size_t dlen = strlen(data);
    std::string result;

#ifdef __APPLE__
    if (strcmp(algo, "md5") == 0) {
        uint8_t out[CC_MD5_DIGEST_LENGTH];
        CC_MD5(data, (CC_LONG)dlen, out);
        result = use_base64 ? base64_encode(out, sizeof(out)) : to_hex(out, sizeof(out));
    } else if (strcmp(algo, "sha1") == 0) {
        uint8_t out[CC_SHA1_DIGEST_LENGTH];
        CC_SHA1(data, (CC_LONG)dlen, out);
        result = use_base64 ? base64_encode(out, sizeof(out)) : to_hex(out, sizeof(out));
    } else if (strcmp(algo, "sha256") == 0) {
        uint8_t out[CC_SHA256_DIGEST_LENGTH];
        CC_SHA256(data, (CC_LONG)dlen, out);
        result = use_base64 ? base64_encode(out, sizeof(out)) : to_hex(out, sizeof(out));
    } else if (strcmp(algo, "sha512") == 0) {
        uint8_t out[CC_SHA512_DIGEST_LENGTH];
        CC_SHA512(data, (CC_LONG)dlen, out);
        result = use_base64 ? base64_encode(out, sizeof(out)) : to_hex(out, sizeof(out));
    } else {
        JS_FreeCString(ctx, algo); JS_FreeCString(ctx, data);
        return JS_ThrowTypeError(ctx, "Unknown hash algorithm: %s", algo);
    }
#else
    if (strcmp(algo, "sha256") == 0) {
        uint8_t out[32]; sha256((const uint8_t*)data, dlen, out);
        result = use_base64 ? base64_encode(out, 32) : to_hex(out, 32);
    } else if (strcmp(algo, "md5") == 0) {
        uint8_t out[16]; md5((const uint8_t*)data, dlen, out);
        result = use_base64 ? base64_encode(out, 16) : to_hex(out, 16);
    } else {
        JS_FreeCString(ctx, algo); JS_FreeCString(ctx, data);
        return JS_ThrowTypeError(ctx, "Unknown hash algorithm: %s", algo);
    }
#endif

    JS_FreeCString(ctx, algo);
    JS_FreeCString(ctx, data);
    return JS_NewStringLen(ctx, result.c_str(), result.size());
}

// ── __crypto_hmac(algo, key, data, encoding) ──────────────────────────────────
static JSValue js_crypto_hmac(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
    if (argc < 3) return JS_ThrowTypeError(ctx, "crypto_hmac: needs (algo, key, data)");
    const char *algo = JS_ToCString(ctx, argv[0]);
    const char *key  = JS_ToCString(ctx, argv[1]);
    const char *data = JS_ToCString(ctx, argv[2]);
    const char *enc  = argc >= 4 ? JS_ToCString(ctx, argv[3]) : nullptr;
    if (!algo||!key||!data) {
        JS_FreeCString(ctx,algo); JS_FreeCString(ctx,key);
        JS_FreeCString(ctx,data); JS_FreeCString(ctx,enc);
        return JS_EXCEPTION;
    }
    bool use_base64 = enc && strcmp(enc, "base64") == 0;
    JS_FreeCString(ctx, enc);

    std::string result;
#ifdef __APPLE__
    CCHmacAlgorithm cc_algo;
    size_t outlen;
    if      (strcmp(algo,"md5")    ==0){cc_algo=kCCHmacAlgMD5;   outlen=CC_MD5_DIGEST_LENGTH;}
    else if (strcmp(algo,"sha1")   ==0){cc_algo=kCCHmacAlgSHA1;  outlen=CC_SHA1_DIGEST_LENGTH;}
    else if (strcmp(algo,"sha256") ==0){cc_algo=kCCHmacAlgSHA256;outlen=CC_SHA256_DIGEST_LENGTH;}
    else if (strcmp(algo,"sha512") ==0){cc_algo=kCCHmacAlgSHA512;outlen=CC_SHA512_DIGEST_LENGTH;}
    else { JS_FreeCString(ctx,algo);JS_FreeCString(ctx,key);JS_FreeCString(ctx,data);
           return JS_ThrowTypeError(ctx,"Unknown hmac algo: %s",algo); }
    std::vector<uint8_t> out(outlen);
    CCHmac(cc_algo, key, strlen(key), data, strlen(data), out.data());
    result = use_base64 ? base64_encode(out.data(), outlen) : to_hex(out.data(), outlen);
#else
    // Simple HMAC-SHA256 fallback
    const uint8_t *kptr = (const uint8_t*)key;
    size_t klen = strlen(key);
    uint8_t k[64]={};
    if (klen > 64) { sha256(kptr, klen, k); klen=32; }
    else           { memcpy(k, kptr, klen); }
    uint8_t ipad[64], opad[64];
    for(int i=0;i<64;i++){ipad[i]=k[i]^0x36;opad[i]=k[i]^0x5c;}
    std::string inner(64 + strlen(data), '\0');
    memcpy(&inner[0], ipad, 64);
    memcpy(&inner[64], data, strlen(data));
    uint8_t ih[32]; sha256((const uint8_t*)inner.data(), inner.size(), ih);
    std::string outer(64+32,'\0');
    memcpy(&outer[0], opad, 64);
    memcpy(&outer[64], ih, 32);
    uint8_t oh[32]; sha256((const uint8_t*)outer.data(), outer.size(), oh);
    result = use_base64 ? base64_encode(oh,32) : to_hex(oh,32);
#endif
    JS_FreeCString(ctx,algo); JS_FreeCString(ctx,key); JS_FreeCString(ctx,data);
    return JS_NewStringLen(ctx, result.c_str(), result.size());
}

// ── __crypto_random_bytes(size) → hex string ──────────────────────────────────
static JSValue js_crypto_random_bytes(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
    int32_t size = 16;
    if (argc >= 1) JS_ToInt32(ctx, &size, argv[0]);
    if (size <= 0 || size > 65536)
        return JS_ThrowRangeError(ctx, "randomBytes: size must be 1-65536");

    std::string buf(size, '\0');
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) return JS_ThrowInternalError(ctx, "Cannot open /dev/urandom");
    read(fd, &buf[0], size);
    close(fd);

    std::string hex = to_hex((const uint8_t*)buf.data(), size);
    return JS_NewStringLen(ctx, hex.c_str(), hex.size());
}

// ── __crypto_random_uuid() → UUID v4 string ───────────────────────────────────
static JSValue js_crypto_random_uuid(JSContext *ctx, JSValueConst, int, JSValueConst *) {
    uint8_t b[16];
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) { read(fd, b, 16); close(fd); }
    b[6] = (b[6] & 0x0f) | 0x40;   // version 4
    b[8] = (b[8] & 0x3f) | 0x80;   // variant bits

    char uuid[37];
    snprintf(uuid, sizeof(uuid),
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7],
        b[8],b[9],b[10],b[11],b[12],b[13],b[14],b[15]);
    return JS_NewString(ctx, uuid);
}

// ── install ───────────────────────────────────────────────────────────────────
void js_init_crypto(JSContext *ctx, JSValue global) {
    JS_SetPropertyStr(ctx, global, "__crypto_hash",
        JS_NewCFunction(ctx, js_crypto_hash,         "__crypto_hash",         3));
    JS_SetPropertyStr(ctx, global, "__crypto_hmac",
        JS_NewCFunction(ctx, js_crypto_hmac,         "__crypto_hmac",         4));
    JS_SetPropertyStr(ctx, global, "__crypto_random_bytes",
        JS_NewCFunction(ctx, js_crypto_random_bytes, "__crypto_random_bytes", 1));
    JS_SetPropertyStr(ctx, global, "__crypto_random_uuid",
        JS_NewCFunction(ctx, js_crypto_random_uuid,  "__crypto_random_uuid",  0));
}
