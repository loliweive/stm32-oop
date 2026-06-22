/**
 * @file    test_ota_protocol.c
 * @brief   OTA 协议层单元测试 — CRC16 + 帧编解码
 *
 * 在 host 端验证:
 *   1. CRC16 查表正确性 (与已知校验值对比)
 *   2. 帧编码/解码往返
 *   3. CRC 错误检测
 *   4. 边界条件 (空负载, 最大负载, 无效帧)
 */
#include "unity.h"
#include "ota_protocol.h"
#include <string.h>

static uint8_t buf[OTA_FRAME_MAX_SIZE * 2];

/* ── CRC16 ────────────────────────────────────────────────────── */
TEST(crc16_known_vector)
{
    /* "123456789" → CRC-CCITT = 0x29B1 (industry standard test vector) */
    uint16_t crc = ota_crc16((const uint8_t *)"123456789", 9);
    TEST_ASSERT_EQUAL_U16(0x29B1, crc);
}

TEST(crc16_empty)
{
    uint16_t crc = ota_crc16(NULL, 0);
    TEST_ASSERT_EQUAL_U16(0xFFFF, crc); /* 初始值 */
}

TEST(crc16_zero_data)
{
    uint8_t zeros[4] = {0, 0, 0, 0};
    uint16_t crc = ota_crc16(zeros, 4);
    TEST_ASSERT_NOT_EQUAL(0xFFFF, crc); /* 不应等于初始值 */
}

/* ── 帧编码/解码 ──────────────────────────────────────────────── */
TEST(frame_encode_decode_roundtrip)
{
    uint8_t payload[] = "Hello OTA!";
    size_t len = ota_frame_encode(buf, sizeof(buf), OTA_CMD_DATA, 42,
                                  payload, strlen((char *)payload));

    TEST_ASSERT_TRUE(len > 0);
    TEST_ASSERT_EQUAL_U8(OTA_STX, buf[0]);

    uint8_t type = 0, seq = 0;
    const uint8_t *decoded = NULL;
    size_t decoded_len = 0;

    bool ok = ota_frame_decode(buf, len, &type, &seq, &decoded, &decoded_len);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_U8(OTA_CMD_DATA, type);
    TEST_ASSERT_EQUAL_U8(42, seq);
    TEST_ASSERT_EQUAL_SIZE(strlen((char *)payload), decoded_len);
    TEST_ASSERT_EQUAL_U8('H', decoded[0]);
    TEST_ASSERT_EQUAL_U8('!', decoded[decoded_len - 1]);
}

TEST(frame_empty_payload)
{
    size_t len = ota_frame_encode(buf, sizeof(buf), OTA_CMD_BOOT, 0, NULL, 0);
    TEST_ASSERT_TRUE(len > 0);

    uint8_t type, seq;
    const uint8_t *payload;
    size_t plen;
    bool ok = ota_frame_decode(buf, len, &type, &seq, &payload, &plen);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_U8(OTA_CMD_BOOT, type);
    TEST_ASSERT_EQUAL_SIZE(0, plen);
}

TEST(frame_crc_error_detection)
{
    uint8_t data[] = {1, 2, 3, 4};
    size_t len = ota_frame_encode(buf, sizeof(buf), OTA_CMD_DATA, 0, data, 4);

    /* 破坏 CRC (最后一字节) */
    buf[len - 1] ^= 0xFF;

    uint8_t type, seq;
    const uint8_t *payload;
    size_t plen;
    bool ok = ota_frame_decode(buf, len, &type, &seq, &payload, &plen);
    TEST_ASSERT_FALSE(ok); /* CRC 校验应失败 */
}

TEST(frame_invalid_stx)
{
    uint8_t data[] = {1, 2, 3, 4};
    size_t len = ota_frame_encode(buf, sizeof(buf), OTA_CMD_DATA, 0, data, 4);
    buf[0] = 0xBB; /* 错误的 STX */

    bool ok = ota_frame_decode(buf, len, NULL, NULL, NULL, NULL);
    TEST_ASSERT_FALSE(ok);
}

TEST(frame_too_short)
{
    bool ok = ota_frame_decode(buf, 3, NULL, NULL, NULL, NULL);
    TEST_ASSERT_FALSE(ok); /* 不足最小帧长 */
}

TEST(build_hello)
{
    size_t len = ota_build_hello(buf, sizeof(buf), 5);
    TEST_ASSERT_TRUE(len > 0);

    uint8_t type, seq;
    const uint8_t *payload;
    size_t plen;
    bool ok = ota_frame_decode(buf, len, &type, &seq, &payload, &plen);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_U8(OTA_CMD_HELLO, type);
    TEST_ASSERT_EQUAL_U8(5, seq);
    TEST_ASSERT_EQUAL_SIZE(4, plen); /* version + max_payload(2) + max_size(1) */
}

TEST(build_ack)
{
    size_t len = ota_build_ack(buf, sizeof(buf), OTA_CMD_DATA, 7);
    uint8_t type, seq;
    bool ok = ota_frame_decode(buf, len, &type, &seq, NULL, NULL);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_U8(OTA_CMD_DATA_ACK, type);
    TEST_ASSERT_EQUAL_U8(7, seq);
}

TEST(build_error)
{
    size_t len = ota_build_error(buf, sizeof(buf), 3, OTA_ERR_FLASH_WRITE);
    uint8_t type, seq;
    const uint8_t *payload;
    size_t plen;
    bool ok = ota_frame_decode(buf, len, &type, &seq, &payload, &plen);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_U8(OTA_CMD_ERROR, type);
    TEST_ASSERT_EQUAL_SIZE(1, plen);
    TEST_ASSERT_EQUAL_U8(OTA_ERR_FLASH_WRITE, payload[0]);
}

TEST(frame_max_payload)
{
    uint8_t large[OTA_MAX_PAYLOAD_SIZE];
    memset(large, 0xAB, sizeof(large));

    size_t len = ota_frame_encode(buf, sizeof(buf), OTA_CMD_DATA, 1, large, sizeof(large));
    TEST_ASSERT_TRUE(len > 0);

    uint8_t type, seq;
    const uint8_t *payload;
    size_t plen;
    bool ok = ota_frame_decode(buf, len, &type, &seq, &payload, &plen);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_SIZE(OTA_MAX_PAYLOAD_SIZE, plen);
    TEST_ASSERT_EQUAL_U8(0xAB, payload[0]);
    TEST_ASSERT_EQUAL_U8(0xAB, payload[plen - 1]);
}

/* ── Runner ─────────────────────────────────────────────────── */
int main(void)
{
    unity_init();
    RUN_TEST(crc16_known_vector);
    RUN_TEST(crc16_empty);
    RUN_TEST(crc16_zero_data);
    RUN_TEST(frame_encode_decode_roundtrip);
    RUN_TEST(frame_empty_payload);
    RUN_TEST(frame_crc_error_detection);
    RUN_TEST(frame_invalid_stx);
    RUN_TEST(frame_too_short);
    RUN_TEST(build_hello);
    RUN_TEST(build_ack);
    RUN_TEST(build_error);
    RUN_TEST(frame_max_payload);
    unity_summary();
    return Unity.failed ? 1 : 0;
}
