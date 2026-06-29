/**
 * @brief  OTA Bootloader — 最小跳转 + 外部 Flash 恢复 + UART OTA
 */
#include "stm32f103xb.h"
#include "core_cm3.h"
#include "ota_config.h"
#include "ota_flash.h"
#include "spi_flash.h"
#include "gpio.h"
#include "rcc.h"
#include "hmac_sha256.h"
#include "dwt_delay.h"
#include <string.h>

/* Dev signing key — must match tools/ota_sender.py DEFAULT_KEY (32 bytes) */
static const uint8_t OTA_SIGN_KEY[32] = "STM32F103-OTA-DEV-KEY-01234567";

static GpioPin      led;
static uint8_t      recovery_buf[1024];

static void led_init(void) {
    rcc_enable_gpio('C');
    GpioPin_ctor(&led, GPIOC, GPIO_PIN_13);
    gpio_set_mode(&led, GPIO_MODE_OUT_PP);
}
static void led_on(void)  { gpio_set(&led, 0); }
static void led_off(void) { gpio_set(&led, 1); }

static void jump_to_app(uint32_t addr) __attribute__((noreturn));
static void jump_to_app(uint32_t addr) {
    uint32_t *vec = (uint32_t *)addr;
    uint32_t sp = vec[0], pc = vec[1];
    if (sp < 0x20000000 || sp > 0x20005000) while(1) {}
    __disable_irq();
    SCB->VTOR = addr;
    __asm__ volatile("msr msp, %0" : : "r"(sp));
    ((void (*)(void))pc)();
    while(1) {}
}

/* ── LED 闪烁错误码 ─────────────────────────────────
 * Bootloader 不使用 OLED (节省 Flash + 移除 I2C 依赖)。
 * 错误通过 LED (PC13, 低电平亮) 闪烁次数传递:
 *   1 次 = HMAC 签名验证失败
 *   持续快闪 = 无可启动固件 (最终兜底)
 * ────────────────────────────────────────────────── */
static void led_blink_error(int count) {
    while (1) {
        for (int n = 0; n < count; n++) {
            led_on();  DWT_DELAY_MS(3);
            led_off(); DWT_DELAY_MS(3);
        }
        /* 长时间停顿分隔错误码重复 */
        DWT_DELAY_MS(20);
    }
}

/* ── 主入口 ──────────────────────────── */
int main(void) {
    rcc_set_sysclk(RCC_PLL, 9);
    led_init(); led_on();

    /* ① 检查 metadata — 已验证固件直接启动 */
    OtaMetadata meta;
    ota_flash_read(OTA_METADATA_START, (uint8_t*)&meta, sizeof(meta));
    if (meta.magic == OTA_MAGIC && meta.state == OTA_STATE_VERIFIED) {
        meta.state = OTA_STATE_BOOTED;
        ota_flash_init(); ota_flash_erase_page(OTA_METADATA_START);
        ota_flash_write(OTA_METADATA_START, (const uint8_t*)&meta, sizeof(meta));
        ota_flash_lock();
        led_off(); jump_to_app(OTA_APP_START);
    }

    /* UART OTA 由 CLI `ota-start` 处理, bootloader 只做恢复/跳转 */

    /* ② 外部 Flash 恢复 */
    rcc_enable_spi(1); rcc_enable_gpio('A'); rcc_enable_gpio('B');
    { GpioPin s; GpioPin_ctor(&s,GPIOA,GPIO_PIN_5); gpio_set_mode(&s,0x0B|GPIO_MODE_OUT_50MHZ);
      GpioPin_ctor(&s,GPIOA,GPIO_PIN_6); gpio_set_mode(&s,0x04|GPIO_MODE_IN);
      GpioPin_ctor(&s,GPIOA,GPIO_PIN_7); gpio_set_mode(&s,0x0B|GPIO_MODE_OUT_50MHZ);
      GpioPin_ctor(&s,GPIOB,GPIO_PIN_9); gpio_set_mode(&s,GPIO_MODE_OUT_PP); gpio_set(&s,1); }
    SpiFlash ext;
    if (spi_flash_init(&ext, SPI1, GPIOB, GPIO_PIN_9)) {
        uint32_t v[2]; spi_flash_read(&ext, 0, (uint8_t*)v, 8);
        if (v[0]>=0x20000000 && v[0]<=0x20005000 && v[1]>=0x08000000 && v[1]<=0x08010000) {
            uint32_t sz=0;
            for (uint32_t o=0;o<OTA_APP_SIZE;o+=1024) {
                spi_flash_read(&ext,o,recovery_buf,1024);
                int ff=1; for(int i=0;i<1024;i++) if(recovery_buf[i]!=0xFF){ff=0;break;}
                if(ff){sz=o;break;}
            } if(!sz) sz=OTA_APP_SIZE;

            /* 读取并验证 HMAC 签名 (末尾 36 bytes) */
            uint32_t real_size = sz;
            bool sig_ok = false;
            if (sz > 36) {
                uint8_t sig_buf[36];
                spi_flash_read(&ext, sz-36, sig_buf, 36);
                real_size = sig_buf[0]|((uint32_t)sig_buf[1]<<8)|((uint32_t)sig_buf[2]<<16)|((uint32_t)sig_buf[3]<<24);
                if (real_size > 0 && real_size <= OTA_APP_SIZE) {
                    /* Incremental HMAC — read firmware in chunks from ext Flash */
                    hmac_ctx hctx; uint8_t computed[32];
                    hmac_sha256_init(&hctx, OTA_SIGN_KEY, 32);
                    for (uint32_t off=0; off<real_size; off+=sizeof(recovery_buf)) {
                        size_t chunk = real_size-off;
                        if (chunk>sizeof(recovery_buf)) chunk=sizeof(recovery_buf);
                        spi_flash_read(&ext, off, recovery_buf, chunk);
                        hmac_sha256_update(&hctx, recovery_buf, chunk);
                    }
                    hmac_sha256_final(&hctx, computed);
                    sig_ok = true;
                    for (int i=0;i<32;i++) if (computed[i]!=sig_buf[4+i]) { sig_ok=false; break; }
                }
            }

            if (!sig_ok) led_blink_error(1);  /* 1 blink = HMAC signature failure */
            led_on();
            ota_flash_init(); ota_flash_erase_range(OTA_APP_START, real_size);
            for(uint32_t o=0;o<real_size;o+=sizeof(recovery_buf)){
                size_t c=real_size-o; if(c>sizeof(recovery_buf))c=sizeof(recovery_buf);
                spi_flash_read(&ext,o,recovery_buf,c);
                ota_flash_write(OTA_APP_START+o,recovery_buf,c); }
            OtaMetadata m; memset(&m,0,sizeof(m));
            m.magic=OTA_MAGIC; m.firmware_size=real_size; m.state=OTA_STATE_VERIFIED;
            ota_flash_erase_page(OTA_METADATA_START);
            ota_flash_write(OTA_METADATA_START,(const uint8_t*)&m,sizeof(m));
            ota_flash_lock();
            led_off(); jump_to_app(OTA_APP_START);
        }
    }

    /* ③ 旧固件 (BOOTED 状态) */
    if (meta.magic == OTA_MAGIC && meta.state == OTA_STATE_BOOTED)
        { led_off(); jump_to_app(OTA_APP_START); }

    /* ④ 直接向量表检查 (无 metadata 直接烧录的固件) */
    { uint32_t *av=(uint32_t*)OTA_APP_START;
      if (av[0]>=0x20000000 && av[0]<=0x20005000 && av[1]>=0x08000000 && av[1]<=0x08010000)
          { led_off(); jump_to_app(OTA_APP_START); } }

    /* ⑤ 全部失败 — 持续快闪 LED (无可启动固件) */
    while(1) { led_on(); DWT_DELAY_MS(2);
               led_off(); DWT_DELAY_MS(2); }
}
