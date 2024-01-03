#include <time.h>
#include <string.h>
#include <string>

#include "websockets.h"
#include "websockets_frame.h"
#include "trace.h"

#include "../include/libdaikinhal.h"

static const char BASE64_CHARS[] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
"abcdefghijklmnopqrstuvwxyz"
"0123456789+/";

static const char MAGIC_GUID[] =
"258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

// We compare in lower case only
static const char WS_RESPONSE_LINE1[] =
"http/1.1 101 switching protocols";
static const char WS_RESPONSE_LINE2[] =
"connection: upgrade";
static const char WS_RESPONSE_LINE3[] =
"upgrade: websocket";
static const char WS_RESPONSE_LINE4[] =
"sec-websocket-accept:";

static char* str_to_lower(
    char* const s)
{
    LIBDAIKIN_ASSERT((s != NULL) && (strlen(s) > 0));

    for (char* p = s; *p; ++p)
        *p = tolower(*p);
    return s;
}

static char* str_trim(
    char* const s)
{
    LIBDAIKIN_ASSERT((s != NULL) && (strlen(s) > 0));

    if (s == NULL || *s == 0)
        return s;

    char* b = s;
    while (*b && isspace(*b))
        ++b;

    char* e = s + strlen(s) - 1;
    for (; (e > b) && isspace(*e); --e)
        *e = 0;

    return b;
}

static uint16_t base64_encode_size(
    uint16_t len)
{
    LIBDAIKIN_ASSERT(len > 0);

    // Based on - https://github.com/joedf/base64.c/blob/master/base64.c
    uint16_t i, j = 0;
    for (i = 0; i < len; i++)
    {
        if (i % 3 == 0)
            j += 1;
    }
    return (4 * j);
}

static uint16_t base64_encode(
    const char* const in,
    uint16_t in_len,
    char* const out)
{
    LIBDAIKIN_ASSERT(in != NULL);
    LIBDAIKIN_ASSERT(in_len > 0);
    LIBDAIKIN_ASSERT(out != NULL);

    // Based on - https://github.com/joedf/base64.c/blob/master/base64.c
    uint16_t i = 0, j = 0, k = 0, s[3];

    for (i = 0; i < in_len; i++)
    {
        s[j++] = *(in + i);
        if (j == 3)
        {
            out[k + 0] = BASE64_CHARS[(s[0] & 255) >> 2];
            out[k + 1] = BASE64_CHARS[((s[0] & 0x03) << 4) + ((s[1] & 0xF0) >> 4)];
            out[k + 2] = BASE64_CHARS[((s[1] & 0x0F) << 2) + ((s[2] & 0xC0) >> 6)];
            out[k + 3] = BASE64_CHARS[s[2] & 0x3F];
            j = 0; k += 4;
        }
    }

    if (j)
    {
        if (j == 1)
            s[1] = 0;
        out[k + 0] = BASE64_CHARS[(s[0] & 255) >> 2];
        out[k + 1] = BASE64_CHARS[((s[0] & 0x03) << 4) + ((s[1] & 0xF0) >> 4)];
        if (j == 2)
            out[k + 2] = BASE64_CHARS[((s[1] & 0x0F) << 2)];
        else
            out[k + 2] = '=';
        out[k + 3] = '=';
        k += 4;
    }

    out[k] = '\0';
    return k;
}

static std::string base64_encode_to_string(
    const char* const buf,
    uint16_t len)
{
    LIBDAIKIN_ASSERT(buf != NULL);
    LIBDAIKIN_ASSERT(len > 0);

    const uint16_t base64_len =
        base64_encode_size(len);

    std::string ret;
    ret.resize(base64_len);

    const uint16_t base64_ret_len =
        base64_encode(buf, len, &ret[0]);

    if (base64_ret_len == base64_len)
        return ret;

    return ""; // Error
}

static int32_t sha1_digest(
    char* const digest,
    uint16_t digest_len,
    const char* const data,
    uint16_t len)
{
    LIBDAIKIN_ASSERT(digest != NULL);
    LIBDAIKIN_ASSERT(digest_len == 20);
    LIBDAIKIN_ASSERT(data != NULL);
    LIBDAIKIN_ASSERT(len > 0);

    // Based on - https://github.com/CTrabant/teeny-sha1/blob/main/teeny-sha1.c

#define SHA1ROTATELEFT(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))

    uint32_t W[80];
    uint32_t H[] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };
    uint32_t c, d, idx, lidx, widx, temp;
    uint32_t didx = 0;

    int32_t wcount;
    uint64_t databits = ((uint64_t)len) * 8;
    uint32_t loopcount = (len + 8) / 64 + 1;
    uint32_t tailbytes = 64 * loopcount - len;
    uint8_t datatail[128] = { 0 };

    if (!digest)
        return -1;

    if (!data)
        return -1;

    /* Pre-processing of data tail (includes padding to fill out 512-bit chunk):
       Add bit '1' to end of message (big-endian)
       Add 64-bit message length in bits at very end (big-endian) */
    datatail[0] = 0x80;
    datatail[tailbytes - 8] = (uint8_t)(databits >> 56 & 0xFF);
    datatail[tailbytes - 7] = (uint8_t)(databits >> 48 & 0xFF);
    datatail[tailbytes - 6] = (uint8_t)(databits >> 40 & 0xFF);
    datatail[tailbytes - 5] = (uint8_t)(databits >> 32 & 0xFF);
    datatail[tailbytes - 4] = (uint8_t)(databits >> 24 & 0xFF);
    datatail[tailbytes - 3] = (uint8_t)(databits >> 16 & 0xFF);
    datatail[tailbytes - 2] = (uint8_t)(databits >> 8 & 0xFF);
    datatail[tailbytes - 1] = (uint8_t)(databits >> 0 & 0xFF);

    /* Process each 512-bit chunk */
    for (lidx = 0; lidx < loopcount; lidx++)
    {
        /* Compute all elements in W */
        memset(W, 0, 80 * sizeof(uint32_t));

        /* Break 512-bit chunk into sixteen 32-bit, big endian words */
        for (widx = 0; widx <= 15; widx++)
        {
            wcount = 24;

            /* Copy byte-per byte from specified buffer */
            while (didx < len && wcount >= 0)
            {
                W[widx] += (((uint32_t)data[didx]) << wcount);
                didx++;
                wcount -= 8;
            }
            /* Fill out W with padding as needed */
            while (wcount >= 0)
            {
                W[widx] += (((uint32_t)datatail[didx - len]) << wcount);
                didx++;
                wcount -= 8;
            }
        }

        /* Extend the sixteen 32-bit words into eighty 32-bit words, with potential optimization from:
           "Improving the Performance of the Secure Hash Algorithm (SHA-1)" by Max Locktyukhin */
        for (widx = 16; widx <= 31; widx++)
        {
            W[widx] = SHA1ROTATELEFT((W[widx - 3] ^ W[widx - 8] ^ W[widx - 14] ^ W[widx - 16]), 1);
        }
        for (widx = 32; widx <= 79; widx++)
        {
            W[widx] = SHA1ROTATELEFT((W[widx - 6] ^ W[widx - 16] ^ W[widx - 28] ^ W[widx - 32]), 2);
        }

        /* Main loop */
        uint32_t a = H[0];
        uint32_t b = H[1];
        c = H[2];
        d = H[3];
        uint32_t e = H[4];

        uint32_t f, k;

        for (idx = 0; idx <= 79; idx++)
        {
            if (idx <= 19)
            {
                f = (b & c) | ((~b) & d);
                k = 0x5A827999;
            }
            else if (idx <= 39)
            {
                f = b ^ c ^ d;
                k = 0x6ED9EBA1;
            }
            else if (idx <= 59)
            {
                f = (b & c) | (b & d) | (c & d);
                k = 0x8F1BBCDC;
            }
            else // if (idx <= 79)
            {
                f = b ^ c ^ d;
                k = 0xCA62C1D6;
            }
            temp = SHA1ROTATELEFT(a, 5) + f + e + k + W[idx];
            e = d;
            d = c;
            c = SHA1ROTATELEFT(b, 30);
            b = a;
            a = temp;
        }

        H[0] += a;
        H[1] += b;
        H[2] += c;
        H[3] += d;
        H[4] += e;
    }

    for (idx = 0; idx < 5; idx++)
    {
        digest[idx * 4 + 0] = (uint8_t)(H[idx] >> 24);
        digest[idx * 4 + 1] = (uint8_t)(H[idx] >> 16);
        digest[idx * 4 + 2] = (uint8_t)(H[idx] >> 8);
        digest[idx * 4 + 3] = (uint8_t)(H[idx]);
    }

    return 0;
}

static std::string ws_create_key()
{
    srand((unsigned)clock());

    char buf[16];
    const int16_t len = sizeof(buf);
    for (int16_t i = 0; i < len; i++)
        buf[i] = (char)(rand() % 256);

    return base64_encode_to_string(buf, len);
}

static std::string ws_create_expected_hash(
    const std::string& key)
{
    LIBDAIKIN_ASSERT(key.size() > 0);

    std::string key_with_magic_guid = key + MAGIC_GUID;

    char digest[20]; // 160 bits Hash
    if (sha1_digest(digest, sizeof(digest), key_with_magic_guid.c_str(), (uint16_t)key_with_magic_guid.length()) != 0)
    {
        LIBDAIKIN_ERROR("sha1_digest failed.\n");
        return "";
    }

    std::string expected_hash_base64 =
        base64_encode_to_string(digest, sizeof(digest));
    str_to_lower(&expected_hash_base64[0]);
    return expected_hash_base64;
}

static std::string ws_create_handshake_request(const std::string& key)
{
    LIBDAIKIN_ASSERT(key.size() > 0);

    std::string req = std::string("GET /mca HTTP/1.1\r\n");
    req += "Host: ";
    req += DAIKIN_REMOTE_IP;
    req += ":";
    req += std::to_string(DAIKIN_REMOTE_PORT);
    req += "\r\n";
    req += "Upgrade: websocket\r\n";
    req += "Connection: Upgrade\r\n";
    req += "Sec-WebSocket-Key: ";
    req += key;
    req += "\r\n";
    req += "Sec-WebSocket-Version: 13\r\n";
    req += "\r\n";

    LIBDAIKIN_TRACE("WS REQUEST:\n%s", req.c_str());
    return req;
}

static bool ws_handshake_validate_response(
    std::string& response,
    const std::string& expected_hash_base64)
{
    LIBDAIKIN_ASSERT(response.length() > 0);
    LIBDAIKIN_ASSERT(expected_hash_base64.length() > 0);

    LIBDAIKIN_TRACE("WS RESPONSE:\n%s", response.c_str());

    int successCount = 0;
    const int MIN_SUCCESS_COUNT = 4;
    const size_t line4_prefix_len = strlen(WS_RESPONSE_LINE4);

    // scan each line \r\n in response
    char* e, *h;
    char* line = &response[0];
    while (((e = strstr(line, "\r\n")) != NULL) && (successCount < MIN_SUCCESS_COUNT))
    {
        *e = 0; ++e; // \r
        *e = 0; ++e; // \n

        if (
            (strcmp(line, WS_RESPONSE_LINE1) == 0) ||
            (strcmp(line, WS_RESPONSE_LINE2) == 0) ||
            (strcmp(line, WS_RESPONSE_LINE3) == 0))
        {
            successCount++;
        }
        else if ((h = strstr(line, WS_RESPONSE_LINE4)) != NULL)
        {
            char* hash_base64 = str_trim(h + line4_prefix_len);
            bool hashEqual = (expected_hash_base64 == hash_base64);
            LIBDAIKIN_TRACE("hash compare (%s): '%s' VS '%s'\n",
                hashEqual == true ? "EQUAL" : "NOT EQUAL!",
                hash_base64,
                expected_hash_base64.c_str());
            if (hashEqual)
                successCount++;
        }

        line = e;
    }

    if (successCount >= MIN_SUCCESS_COUNT)
        return true;

    return false;
}

bool daikin_ws_open(daikin_t* const daikin)
{
    LIBDAIKIN_ASSERT(daikin != NULL);

    if (daikin->is_open)
        return true;

    if (daikin_hal_tcp_open(&daikin->tcp) == false)
    {
        LIBDAIKIN_ERROR("daikin_hal_tcp_open failed.\n");
        return false;
    }

    std::string key =
        ws_create_key();
    std::string request =
        ws_create_handshake_request(key);

    int32_t ret = daikin_hal_tcp_write(&daikin->tcp, &request[0], (uint16_t)request.length());
    if (ret < 1)
    {
        LIBDAIKIN_ERROR("daikin_hal_tcp_write failed.\n");
        return false;
    }

    char data[256];
    uint16_t len = sizeof(data);

    ret = daikin_hal_tcp_read(&daikin->tcp, data, len);
    if (ret < 1)
    {
        LIBDAIKIN_ERROR("daikin_hal_tcp_read failed.\n");
        return false;
    }

    len = ret;

    // We need to terminate string
    if ((len + 1) > (uint16_t)sizeof(data))
    {
        LIBDAIKIN_ERROR("buffer too small to terminate response string.\n");
        return false;
    }

    // Terminate string with EOF string and make it lower case for later parsing
    data[len] = 0;
    str_to_lower(data);

    std::string response = std::string(data, len);
    std::string expected_hash_base64 = ws_create_expected_hash(key);
    if (ws_handshake_validate_response(response, expected_hash_base64) == false)
    {
        LIBDAIKIN_ERROR("ws_handshake_validate_response failed.\n");
        return false;
    }

    daikin->is_open = true;
    return true;
}

bool daikin_ws_request(
    const daikin_t* const daikin,
    const std::string& request,
    std::string& response)
{
    LIBDAIKIN_ASSERT(daikin != NULL);
    LIBDAIKIN_ASSERT(request.length() > 0);
    //LIBDAIKIN_ASSERT(response.length() > 0);

    LIBDAIKIN_TRACE("WS TEXT FRAME REQUEST: %s\n", request.c_str());

    if (ws_write_text_frame(&daikin->tcp, request) == false)
    {
        LIBDAIKIN_ERROR("ws_write_text_frame failed.\n");
        return false;
    }

    if (ws_wait_for_text_frame(&daikin->tcp, response) == false)
    {
        LIBDAIKIN_ERROR("ws_wait_for_text_frame failed.\n");
        return false;
    }

    LIBDAIKIN_TRACE("WS TEXT FRAME RESPONSE: %s\n", response.c_str());
    return true;
}

void daikin_ws_close(
    daikin_t* const daikin)
{
    LIBDAIKIN_ASSERT(daikin != NULL);

    if (daikin->is_open == true)
    {
        if (ws_write_close_frame(&daikin->tcp, WS_SC_NORMAL_CLOSURE, NULL))
            ws_wait_for_close_frame(&daikin->tcp);
        daikin->is_open = false;
    }

    daikin_hal_tcp_close(&daikin->tcp);
}
