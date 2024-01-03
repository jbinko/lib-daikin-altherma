#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <string>
#include <vector>

#include "websockets_frame.h"
#include "trace.h"

static const uint8_t FIN_MASK           = 0b10000000;
static const uint8_t OPCODE_MASK        = 0b00001111;
static const uint8_t MASK_MASK          = 0b10000000;
static const uint8_t PAYLOADLEN_MASK    = 0b01111111;

static const uint8_t CONTROL_FRAME_MASK = 0b00001000;

// https://developer.ibm.com/articles/au-endianc/
static const uint16_t BIGENDIAN_TEST = 1;
#define IS_HOST_BIGENDIAN() ( (*(char*)&BIGENDIAN_TEST) == 0 )

typedef enum
{
    WS_OPC_CONT_FRAME   = 0x0,
    WS_OPC_TEXT_FRAME   = 0x1,
    WS_OPC_BIN_FRAME    = 0x2,
    WS_OPC_CLOSE_FRAME  = 0x8,
    WS_OPC_PING_FRAME   = 0x9,
    WS_OPC_PONG_FRAME   = 0xA,
} ws_opcode_t;

typedef struct {
    bool        fin;
    ws_opcode_t opcode;
    bool        mask;
    uint64_t    payload_len;
} ws_min_frame_t;

static uint16_t host_to_network_uint16(uint16_t v)
{
    if (IS_HOST_BIGENDIAN())
        return v;

    return ((v << 8) & 0xFF00) | ((v >> 8) & 0x00FF);
}

static uint16_t network_to_host_uint16(uint16_t v)
{
    return host_to_network_uint16(v);
}

static uint64_t host_to_network_uint64(uint64_t v)
{
    if (IS_HOST_BIGENDIAN())
        return v;

    uint64_t r;
    char* const d = (char* const)&r;
    char* const s = (char* const)&v;

    d[0] = s[7];
    d[1] = s[6];
    d[2] = s[5];
    d[3] = s[4];
    d[4] = s[3];
    d[5] = s[2];
    d[6] = s[1];
    d[7] = s[0];

    return r;
}

static uint64_t network_to_host_uint64(uint64_t v)
{
    return host_to_network_uint64(v);
}

/*static*/ uint32_t daikin_hal_tcp_IPv4(const char* const ipv4)
{
    LIBDAIKIN_ASSERT((ipv4 != NULL) && (strlen(ipv4) > 0));

    uint8_t count = 0, data[4];

    if (ipv4 != NULL)
    {
        const size_t len = strlen(ipv4);
        const char* p = ipv4;
        const char* const e = ipv4 + len;

        for (; *p && p < e && count < 4; ++p)
        {
            errno = 0;
            char* temp;
            unsigned long v = strtoul(p, &temp, 10);

            if (
                errno != 0 || // Conversion error
                p == temp || // Conversion error
                v > 0xFF || // Too big
                ((*temp) == '.' && count > 2) || // We expect only 3 dots
                ((*temp) != '.' && (*temp) != 0) // Or anything else unexpected
                )
            {
                count = 0; // Error flag
                break;
            }

            p = temp;
            data[count++] = (uint8_t)v;
        }

        if (p < e)
            count = 0; // We didn't reach the end
    }

    if (count != 4)
    {
        LIBDAIKIN_ERROR("Not valid IPv4 address: '%s'.\n", ipv4);
        return 0; // Error
    }

    return *((uint32_t*)data);
}

static bool ws_is_control_frame(ws_opcode_t opcode)
{
    return (((uint8_t)opcode) & CONTROL_FRAME_MASK) == CONTROL_FRAME_MASK;
}

static void ws_set_masking_key(
    char* const masking_key,
    uint8_t masking_key_len)
{
    LIBDAIKIN_ASSERT(masking_key != NULL);
    LIBDAIKIN_ASSERT(masking_key_len == 4);

    srand((unsigned)clock());

    for (uint8_t i = 0; i < masking_key_len; i++)
        masking_key[i] = (char)(rand() % 256);
}

static void ws_mask_payload(
    char* const payload,
    uint16_t payload_len,
    const char* const masking_key,
    uint8_t masking_key_len)
{
    LIBDAIKIN_ASSERT(payload != NULL);
    //LIBDAIKIN_ASSERT(payload_len > 0); // Request can have empty body
    LIBDAIKIN_ASSERT(masking_key != NULL);
    LIBDAIKIN_ASSERT(masking_key_len == 4);

    for (uint16_t i = 0; i < payload_len; i++)
        payload[i] = (char)(payload[i] ^ masking_key[i % 4]);
}

static uint8_t ws_set_client_header(
    char* const header,
    uint8_t hdr_max_len,
    ws_opcode_t opcode,
    uint16_t payload_len)
{
    LIBDAIKIN_ASSERT(header != NULL);
    LIBDAIKIN_ASSERT(hdr_max_len == 8);
    LIBDAIKIN_ASSERT(
        (opcode == ws_opcode_t::WS_OPC_TEXT_FRAME) ||
        (opcode == ws_opcode_t::WS_OPC_CLOSE_FRAME)); // Currently supported only those
    //LIBDAIKIN_ASSERT(payload_len > 0); // Request can have empty body

    // We do not support payload_len > 0xFFFF
    uint8_t hdr_payload_len = (uint8_t)payload_len;
    //uint8_t hdr_ext_payload_len = 0;

    if (payload_len > 125)
    {
        hdr_payload_len = 126;
        //hdr_ext_payload_len = 2;
    }

    const bool fin = true; // Currently supported only true
    header[0] = (char)((fin ? FIN_MASK : 0) | (((char)opcode) & OPCODE_MASK));
    header[1] = (char)(MASK_MASK | (hdr_payload_len & PAYLOADLEN_MASK)); // Client always masks, server never

    if (hdr_payload_len <= 125)
    {
        // input masking_key will be moved 2 bytes ahead
        memcpy(&header[2], &header[4], 4);
        return 2 + 4; // Shorter version of the hdr
    }

    uint16_t temp = host_to_network_uint16(payload_len);
    memcpy(&header[2], (char*)(&temp), 2);

    // input masking_key will stay untouched
    return 2 + 2 + 4; // Medium version of the hdr
}

static bool ws_read_payload_len(
    const daikin_hal_tcp_t* const tcp,
    char* payload_len,
    uint16_t payload_len_len)
{
    LIBDAIKIN_ASSERT(tcp != NULL);
    LIBDAIKIN_ASSERT(payload_len != NULL);
    LIBDAIKIN_ASSERT(payload_len_len > 0);

    int32_t ret = daikin_hal_tcp_read(tcp, payload_len, payload_len_len);
    if (ret < ((int32_t)payload_len_len))
    {
        LIBDAIKIN_ERROR("daikin_hal_tcp_read (payload_len) failed: %d.\n", ret);
        return false;
    }

    return true;
}

static bool ws_write_frame(
    const daikin_hal_tcp_t* const tcp,
    ws_opcode_t opcode,
    char* const payload, // We are modifying payload via masking
    uint16_t payload_len)
{
    LIBDAIKIN_ASSERT(tcp != NULL);
    LIBDAIKIN_ASSERT(
        (opcode == ws_opcode_t::WS_OPC_TEXT_FRAME) ||
        (opcode == ws_opcode_t::WS_OPC_CLOSE_FRAME)); // Currently supported only those
    LIBDAIKIN_ASSERT(payload != NULL);
    //LIBDAIKIN_ASSERT(payload_len > 0); // Request can have empty body

    char hdr[8];
    const uint8_t hdr_max_len = sizeof(hdr);
    const uint8_t masking_key_len = 4;

    ws_set_masking_key(&hdr[4], hdr_max_len - masking_key_len);
    uint8_t hdr_len = ws_set_client_header(hdr, hdr_max_len, opcode, payload_len);

    ws_mask_payload(payload, payload_len, &hdr[hdr_len - masking_key_len], masking_key_len);

    int32_t ret = daikin_hal_tcp_write(tcp, hdr, hdr_len);
    if (ret < 1)
    {
        LIBDAIKIN_ERROR("daikin_hal_tcp_write (header) failed.\n");
        return false;
    }

    if (payload_len > 0)
    {
        ret = daikin_hal_tcp_write(tcp, payload, payload_len);
        if (ret < 1)
        {
            LIBDAIKIN_ERROR("daikin_hal_tcp_write (payload) failed.\n");
            return false;
        }
    }

    return true;
}

static bool ws_read_parse_frame(
    const daikin_hal_tcp_t* const tcp,
    ws_min_frame_t* const frame,
    bool expect_fin,
    ws_opcode_t expect_opcode,
    std::vector<char> &payload
)
{
    LIBDAIKIN_ASSERT(tcp != NULL);
    LIBDAIKIN_ASSERT(frame != NULL);

    memset(frame, 0, sizeof(ws_min_frame_t));

    char hdr[2];
    const uint8_t hdr_min_len = sizeof(hdr);

    int32_t ret = daikin_hal_tcp_read(tcp, hdr, hdr_min_len);
    if (ret < ((int32_t)hdr_min_len))
    {
        LIBDAIKIN_ERROR("Unexpected WS Frame len (header): %d.\n", ret);
        return false;
    }

    frame->fin = (hdr[0] & FIN_MASK) == FIN_MASK;
    frame->opcode = (ws_opcode_t)(hdr[0] & OPCODE_MASK);
    frame->mask = (hdr[1] & MASK_MASK) == MASK_MASK;
    uint8_t temp_len = (uint8_t)(hdr[1] & PAYLOADLEN_MASK);

    if ((!frame->fin) && expect_fin)
    {
        LIBDAIKIN_ERROR("Expected FIN in the WS Frame.\n");
        return false;
    }

    if (frame->mask)
    {
        LIBDAIKIN_ERROR("Unexpected MASK flag from the server in the WS Frame.\n");
        return false;
    }

    if (frame->opcode != expect_opcode)
    {
        LIBDAIKIN_ERROR("Unexpected OPCODE flag: %d from the server in the WS Frame.\n",
            (int32_t) frame->opcode);
        return false;
    }

    if (ws_is_control_frame(frame->opcode) && frame->fin == false)
    {
        LIBDAIKIN_ERROR("Control frame must have FIN flag.\n");
        return false;
    }

    if (temp_len <= 125)
        frame->payload_len = temp_len;
    else
    {
        if (temp_len == 126)
        {
            char ext_payload_len[2];
            if (ws_read_payload_len(tcp, ext_payload_len, sizeof(ext_payload_len)) == false)
                return false; // No extra error info needed
            frame->payload_len =
                network_to_host_uint16(*((uint16_t*)ext_payload_len));
        }
        else
        {
            char ext_payload_len_cont[8];
            if (ws_read_payload_len(tcp, ext_payload_len_cont, sizeof(ext_payload_len_cont)) == false)
                return false; // No extra error info needed
            frame->payload_len =
                network_to_host_uint64(*((uint64_t*)ext_payload_len_cont));
        }
    }

    if (ws_is_control_frame(frame->opcode) && frame->payload_len > 125)
    {
        LIBDAIKIN_ERROR("Control frame max payload length (125) exceeded. Total length: %u.\n",
            (uint16_t)frame->payload_len);
        return false;
    }

    payload.resize(frame->payload_len);
    if (frame->payload_len > 0)
    {
        ret = daikin_hal_tcp_read(tcp, &payload[0], (uint16_t)payload.size());
        if (ret < (int32_t)payload.size())
        {
            LIBDAIKIN_ERROR("daikin_hal_tcp_read (payload) failed: %d.\n", ret);
            return false;
        }
    }

    return true;
}

bool ws_write_close_frame(
    const daikin_hal_tcp_t* const tcp,
    uint16_t status_code,
    const char* const reason)
{
    LIBDAIKIN_ASSERT(tcp != NULL);
    LIBDAIKIN_ASSERT(status_code == WS_SC_NORMAL_CLOSURE); // Currently supported only this
    //LIBDAIKIN_ASSERT(reason != NULL); // Request can have empty body

    std::vector<char> payload = std::vector<char>();

    uint16_t temp = host_to_network_uint16(status_code);
    const char* const p = (const char* const)(&temp);
    payload.insert(payload.end(), p, p + sizeof(temp));

    if (reason != NULL)
        payload.insert(payload.end(), reason, reason + strlen(reason));

    uint16_t len = (uint16_t)payload.size();
    if (len > 125)
    {
        LIBDAIKIN_ERROR("Control frame max payload length (125) exceeded. Total length: %u.\n", len);
        return false;
    }

    return ws_write_frame(tcp, ws_opcode_t::WS_OPC_CLOSE_FRAME, &payload[0], len);
}

bool ws_wait_for_close_frame(
    const daikin_hal_tcp_t* const tcp
)
{
    LIBDAIKIN_ASSERT(tcp != NULL);

    ws_min_frame_t f;
    std::vector<char> payload = std::vector<char>();
    if (ws_read_parse_frame(tcp, &f, true, ws_opcode_t::WS_OPC_CLOSE_FRAME, payload) == false)
    {
        LIBDAIKIN_ERROR("ws_read_parse_frame failed.\n");
        return false;
    }

    if (payload.size() > 1) // At least two bytes for status code
    {
        LIBDAIKIN_TRACE("CLOSE FRAME - STATUS CODE: '%u'.\n",
            network_to_host_uint16(*((uint16_t*)(&payload[0]))));
    }

    if (payload.size() > 2) // At least three bytes for reason (previous two are for status code)
    {
        LIBDAIKIN_TRACE("CLOSE FRAME - REASON: '%s'.\n",
            std::string(&payload[2], payload.size() - 2).c_str());
    }

    return true;
}

bool ws_write_text_frame(
    const daikin_hal_tcp_t* const tcp,
    const std::string& request
)
{
    LIBDAIKIN_ASSERT(tcp != NULL);
    LIBDAIKIN_ASSERT(request.length() > 0);

    // Create copy, because we need to modify original data (masking data)
    std::string data = request;
    return ws_write_frame(tcp, ws_opcode_t::WS_OPC_TEXT_FRAME, &data[0], (uint16_t)data.length());
}

bool ws_wait_for_text_frame(
    const daikin_hal_tcp_t* const tcp,
    std::string& response
)
{
    LIBDAIKIN_ASSERT(tcp != NULL);

    ws_min_frame_t f;
    std::vector<char> payload = std::vector<char>();
    if (ws_read_parse_frame(tcp, &f, true, ws_opcode_t::WS_OPC_TEXT_FRAME, payload) == false)
    {
        LIBDAIKIN_ERROR("ws_read_parse_frame failed.\n");
        return false;
    }

    response = std::string(&payload[0], payload.size());
    return true;
}
