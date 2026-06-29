/**
 * @file    test_spi_flash.c
 * @brief   SPI Flash W25Qxx 单元测试 (host, mock SPI vtable + mock GPIO)
 */
#include "unity.h"
#include "mock_spi.h"
#include "mock_gpio.h"
#include "spi_flash.h"
#include <string.h>

static SpiFlash flash;
static MockSpiState spi_mock;
static MockGpioState cs_mock;

/* ── Setup ──────────────────────────────────────────────────── */
static void setup(void)
{
    mock_spi_reset(&spi_mock);
    mock_gpio_reset(&cs_mock);
    memset(&flash, 0, sizeof(flash));
    /* 设置全局 mock state — spi_flash_init 内部的构造函数会 */
    /* 自动绑定 mock vtable (因为链接的是 mock_spi.c)      */
    mock_spi_set_global(&spi_mock);
    mock_gpio_set_global(&cs_mock);
}

/* 初始化 W25Q128 (预设 JEDEC ID = 0xEF, 0x40, 0x18) */
static void setup_w25q128(void)
{
    setup();
    mock_spi_set_jedec_id(&spi_mock, 0xEF, 0x40, 0x18);
    /* mock 已全局激活 — spi_flash_init 全程走 mock */
    spi_flash_init(&flash, (void*)0x40013000, (void*)0x40010C00, GPIO_PIN_9);
}

static void setup_unknown(void)
{
    setup();
    mock_spi_set_jedec_id(&spi_mock, 0x00, 0x00, 0x00);
    spi_flash_init(&flash, (void*)0x40013000, (void*)0x40010C00, GPIO_PIN_9);
}

/* ── Init + JEDEC ID 测试 ──────────────────────────────────── */

TEST(spi_flash_init_w25q128)
{
    setup_w25q128();
    TEST_ASSERT_TRUE(flash.ready);
    TEST_ASSERT_TRUE(strstr(flash.info.name, "W25Q128") != NULL);
    TEST_ASSERT_EQUAL_U32(0x1000000, flash.info.capacity);  /* 16MB */
}

TEST(spi_flash_init_unknown_chip)
{
    setup_unknown();
    /* _detect_flash 已经在 spi_flash_init 中运行, 但识别失败 */
    TEST_ASSERT_FALSE(flash.ready);
}

TEST(spi_flash_read_jedec_id)
{
    setup_w25q128();
    /* reset mock for a fresh JEDEC read */
    mock_spi_reset(&spi_mock);
    mock_spi_set_jedec_id(&spi_mock, 0xEF, 0x40, 0x17); /* W25Q64 */
    mock_spi_inject(&flash.spi_port, &spi_mock);

    uint32_t id = spi_flash_read_jedec_id(&flash);
    TEST_ASSERT_EQUAL_U32(0xEF4017, id);
    /* 验证 SPI 发送了 JEDEC ID 命令 (0x9F) */
    TEST_ASSERT_EQUAL_U8(0x9F, spi_mock.tx_log[0]);
}

/* ── Status 测试 ────────────────────────────────────────────── */

TEST(spi_flash_read_status)
{
    setup_w25q128();
    /* 预设状态字节 = 0x03 (BUSY + WEL) */
    mock_spi_reset(&spi_mock);
    spi_mock.rx_data[0] = 0x03; spi_mock.rx_len = 1; spi_mock.rx_idx = 0;
    mock_spi_inject(&flash.spi_port, &spi_mock);

    uint8_t st = spi_flash_read_status(&flash);
    TEST_ASSERT_EQUAL_U8(0x03, st);
    TEST_ASSERT_EQUAL_U8(0x05, spi_mock.tx_log[0]); /* CMD_READ_STATUS */
}

/* ── Erase 测试 ─────────────────────────────────────────────── */

TEST(spi_flash_erase_sector)
{
    setup_w25q128();
    mock_spi_reset(&spi_mock);
    /* 预设: WRITE_ENABLE 响应 + 状态返回 READY */
    spi_mock.rx_len = 0; /* 不需要特定响应 — 只验证发送的命令 */
    mock_spi_inject(&flash.spi_port, &spi_mock);

    spi_flash_erase_sector(&flash, 0x1000);

    /* 验证发送的命令序列 (地址 MSB-first):
     *   tx[0]=0x06 (WRITE_ENABLE)
     *   tx[1]=0x20 (CMD_SECTOR_ERASE)
     *   tx[2]=0x00 (addr[23:16])
     *   tx[3]=0x10 (addr[15:8])
     *   tx[4]=0x00 (addr[7:0])
     */
    TEST_ASSERT_EQUAL_U8(0x06, spi_mock.tx_log[0]);
    TEST_ASSERT_EQUAL_U8(0x20, spi_mock.tx_log[1]);
    TEST_ASSERT_EQUAL_U8(0x00, spi_mock.tx_log[2]); /* addr MSB */
    TEST_ASSERT_EQUAL_U8(0x10, spi_mock.tx_log[3]);
    TEST_ASSERT_EQUAL_U8(0x00, spi_mock.tx_log[4]); /* addr LSB */
}

/* ── Write 测试 ─────────────────────────────────────────────── */

TEST(spi_flash_write_page)
{
    setup_w25q128();
    mock_spi_reset(&spi_mock);
    spi_mock.rx_len = 0;
    mock_spi_inject(&flash.spi_port, &spi_mock);

    uint8_t data[] = {0xAA, 0xBB, 0xCC};
    spi_flash_write(&flash, 0x200, data, 3);

    /* Page Program (地址 MSB-first): 0x06 + 0x02 + addr[3] + data[3] */
    TEST_ASSERT_EQUAL_U8(0x06, spi_mock.tx_log[0]); /* WRITE_ENABLE */
    TEST_ASSERT_EQUAL_U8(0x02, spi_mock.tx_log[1]); /* PAGE_PROGRAM */
    TEST_ASSERT_EQUAL_U8(0x00, spi_mock.tx_log[2]); /* addr[23:16] */
    TEST_ASSERT_EQUAL_U8(0x02, spi_mock.tx_log[3]); /* addr[15:8] */
    TEST_ASSERT_EQUAL_U8(0x00, spi_mock.tx_log[4]); /* addr[7:0] */
    TEST_ASSERT_EQUAL_U8(0xAA, spi_mock.tx_log[5]); /* data[0] */
    TEST_ASSERT_EQUAL_U8(0xBB, spi_mock.tx_log[6]); /* data[1] */
    TEST_ASSERT_EQUAL_U8(0xCC, spi_mock.tx_log[7]); /* data[2] */
}

/* ── Get Info 测试 ──────────────────────────────────────────── */

TEST(spi_flash_get_info)
{
    setup_w25q128();
    const SpiFlashInfo *info = spi_flash_get_info(&flash);
    TEST_ASSERT_NOT_NULL(info);
    TEST_ASSERT_EQUAL_U32(256, info->page_size);
    TEST_ASSERT_EQUAL_U32(4096, info->sector_size);
}

TEST(spi_flash_cs_asserts_correctly)
{
    setup_w25q128();
    mock_spi_reset(&spi_mock);
    mock_gpio_reset(&cs_mock);
    spi_mock.rx_len = 0;
    mock_spi_inject(&flash.spi_port, &spi_mock);
    mock_gpio_inject(&flash.cs, &cs_mock);

    spi_flash_read_status(&flash);

    /* CS 应该先拉低再拉高 — 检查 gpio_set 调用次数 ≥2 */
    TEST_ASSERT_TRUE(cs_mock.set_count >= 2);
}

int main(void)
{
    unity_init();
    RUN_TEST(spi_flash_init_w25q128);
    RUN_TEST(spi_flash_init_unknown_chip);
    RUN_TEST(spi_flash_read_jedec_id);
    RUN_TEST(spi_flash_read_status);
    RUN_TEST(spi_flash_erase_sector);
    RUN_TEST(spi_flash_write_page);
    RUN_TEST(spi_flash_get_info);
    RUN_TEST(spi_flash_cs_asserts_correctly);
    unity_summary();
    return Unity.failed ? 1 : 0;
}
