/**
 * @brief  OTA Bootloader — 最小跳转 + 外部 Flash 恢复 + UART OTA
 */
#include "stm32f103xb.h"
#include "ota_config.h"
#include "ota_flash.h"
#include "ota_transport.h"
#include "ota_transport_uart.h"
#include "ota_client.h"
#include "spi_flash.h"
#include "gpio.h"
#include "rcc.h"
#include <string.h>

static UartXportCtx uart_ctx;
static OtaTransport transport;
static OtaClient    ota;
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
    SCB_VTOR = addr;
    __asm__ volatile("msr msp, %0" : : "r"(sp));
    ((void (*)(void))pc)();
    while(1) {}
}

/* ── 精简 OLED 错误显示 ──────────────────────── */
static void oled_write(uint8_t *buf, int len) {
    I2C_Type *i=I2C1; uint32_t to;
    i->CR1|=I2C_CR1_START; to=100000; while(!(i->SR1&I2C_SR1_SB)&&--to){}
    i->DR=(uint8_t)(0x3C<<1); to=100000; while(!(i->SR1&I2C_SR1_ADDR)&&--to){}
    (void)i->SR2;
    for(int n=0;n<len;n++){ to=100000; while(!(i->SR1&I2C_SR1_TXE)&&--to){} i->DR=buf[n]; }
    to=100000; while(!(i->SR1&I2C_SR1_BTF)&&--to){}
    i->CR1|=I2C_CR1_STOP;
}
static void oled_cmd(uint8_t c) { uint8_t b[2]={0,c}; oled_write(b,2); }
static void oled_data(const uint8_t *d, int n) {
    for(int o=0;o<n;o+=128){ int c=(n-o>128)?128:(n-o);
        uint8_t b[129]; b[0]=0x40; memcpy(b+1,d+o,c); oled_write(b,c+1); }
}
static void oled_init_show(const char *line1, const char *line2) {
    rcc_enable_gpio('B'); rcc_enable_i2c(1);
    { GpioPin s; GpioPin_ctor(&s,GPIOB,GPIO_PIN_6); gpio_set_mode(&s,GPIO_CNF_ALT_OD|GPIO_MODE_OUT_50MHZ);
      GpioPin_ctor(&s,GPIOB,GPIO_PIN_7); gpio_set_mode(&s,GPIO_CNF_ALT_OD|GPIO_MODE_OUT_50MHZ); }
    I2C1->CR1|=I2C_CR1_SWRST; I2C1->CR1&=~I2C_CR1_SWRST;
    I2C1->CR2=36; I2C1->CCR=180; I2C1->TRISE=37; I2C1->CR1|=I2C_CR1_PE;
    uint8_t c[]={0xAE,0x20,0x02,0xA8,0x3F,0xD3,0x00,0x40,0xA1,0xC8,
      0xDA,0x12,0x81,0xCF,0xD9,0xF1,0xDB,0x30,0xA4,0xA6,0x8D,0x14};
    for(int i=0;i<(int)sizeof(c);i++) oled_cmd(c[i]);
    /* 清屏 */
    uint8_t z[128]; memset(z,0,128);
    for(int p=0;p<8;p++){ oled_cmd(0xB0|p); oled_cmd(0); oled_cmd(0x10); oled_data(z,128); }
    oled_cmd(0xAF);

    /* 5x7 字库 (仅需字符) */
    static const uint8_t f[][5]={ {0},{0x7C,0x12,0x11,0x12,0x7C},
      {0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
      {0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},
      {0x7F,0x09,0x09,0x01,0x01},{0x3E,0x41,0x49,0x49,0x7A},
      {0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},
      {0x3E,0x41,0x41,0x41,0x3E},{0x7F,0x09,0x19,0x29,0x46},
      {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7F,0x01,0x01},
      {0x3F,0x40,0x40,0x40,0x3F},{0x7F,0x20,0x18,0x20,0x7F},
      {0x20,0x54,0x54,0x54,0x78},{0x38,0x44,0x44,0x44,0x38},
      {0x7C,0x14,0x14,0x14,0x08},{0x00,0x41,0x7F,0x40,0x00} };
    #define IDX(c) ((c)==' '?0:(c)>='A'&&(c)<='I'?(c)-'A'+1:(c)=='O'?10:(c)=='R'?11:(c)=='S'?12:(c)=='T'?13:(c)=='U'?14:(c)=='W'?15:(c)=='a'?16:(c)=='o'?17:(c)=='p'?18:(c)=='l'?19:0)

    int x1=(128-(int)strlen(line1)*6)/2, x2=(128-(int)strlen(line2)*6)/2;
    for(const char *s=line1;*s;s++,x1+=6){ int i=IDX(*s); oled_cmd(0xB0|1);oled_cmd(x1&0xF);oled_cmd(0x10|(x1>>4));
        uint8_t d[6]={0x40,f[i][0],f[i][1],f[i][2],f[i][3],0}; oled_write(d,6);
        oled_cmd(0xB0|2);oled_cmd(x1&0xF);oled_cmd(0x10|(x1>>4));
        uint8_t d2[3]={0x40,f[i][4]}; oled_write(d2,3); }
    for(const char *s=line2;*s;s++,x2+=6){ int i=IDX(*s); oled_cmd(0xB0|4);oled_cmd(x2&0xF);oled_cmd(0x10|(x2>>4));
        uint8_t d[6]={0x40,f[i][0],f[i][1],f[i][2],f[i][3],0}; oled_write(d,6);
        oled_cmd(0xB0|5);oled_cmd(x2&0xF);oled_cmd(0x10|(x2>>4));
        uint8_t d2[3]={0x40,f[i][4]}; oled_write(d2,3); }
}

/* ── 主入口 ──────────────────────────── */
int main(void) {
    rcc_set_sysclk(RCC_PLL, 9);
    led_init(); led_on();

    /* ① 检查 metadata */
    OtaMetadata meta;
    ota_flash_read(OTA_METADATA_START, (uint8_t*)&meta, sizeof(meta));
    if (meta.magic == OTA_MAGIC && meta.state == OTA_STATE_VERIFIED) {
        meta.state = OTA_STATE_BOOTED;
        ota_flash_init(); ota_flash_erase_page(OTA_METADATA_START);
        ota_flash_write(OTA_METADATA_START, (const uint8_t*)&meta, sizeof(meta));
        ota_flash_lock();
        led_off(); jump_to_app(OTA_APP_START);
    }

    /* ② 等待 UART OTA (3秒) */
    ota_transport_uart_create(&transport, &uart_ctx);
    ota_xport_init(&transport);
    ota_client_init(&ota, &transport);
    for (int i=0;i<30;i++) {
        uint8_t buf[OTA_FRAME_MAX_SIZE];
        size_t n = ota_xport_recv(&transport, buf, sizeof(buf), 100);
        if (n>0) { uint8_t t,s; const uint8_t *p; size_t pl;
            if (ota_frame_decode(buf,n,&t,&s,&p,&pl) && t == OTA_CMD_HELLO) goto start_ota; }
        if (i&1) led_on(); else led_off();
    }

    /* ③ 外部 Flash 恢复 */
    rcc_enable_spi(1); rcc_enable_gpio('A'); rcc_enable_gpio('B');
    { GpioPin s; GpioPin_ctor(&s,GPIOA,GPIO_PIN_5); gpio_set_mode(&s,GPIO_CNF_ALT_PP|GPIO_MODE_OUT_50MHZ);
      GpioPin_ctor(&s,GPIOA,GPIO_PIN_6); gpio_set_mode(&s,GPIO_CNF_FLOAT|GPIO_MODE_IN);
      GpioPin_ctor(&s,GPIOA,GPIO_PIN_7); gpio_set_mode(&s,GPIO_CNF_ALT_PP|GPIO_MODE_OUT_50MHZ);
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
            led_on();
            ota_flash_init(); ota_flash_erase_range(OTA_APP_START,sz);
            for(uint32_t o=0;o<sz;o+=sizeof(recovery_buf)){
                size_t c=sz-o; if(c>sizeof(recovery_buf))c=sizeof(recovery_buf);
                spi_flash_read(&ext,o,recovery_buf,c);
                ota_flash_write(OTA_APP_START+o,recovery_buf,c); }
            OtaMetadata m; memset(&m,0,sizeof(m));
            m.magic=OTA_MAGIC; m.firmware_size=sz; m.state=OTA_STATE_VERIFIED;
            ota_flash_erase_page(OTA_METADATA_START);
            ota_flash_write(OTA_METADATA_START,(const uint8_t*)&m,sizeof(m));
            ota_flash_lock();
            led_off(); jump_to_app(OTA_APP_START);
        }
    }

    /* ④ 旧固件 */
    if (meta.magic == OTA_MAGIC && meta.state == OTA_STATE_BOOTED)
        { led_off(); jump_to_app(OTA_APP_START); }

    /* ⑤ 直接向量表检查 */
    { uint32_t *av=(uint32_t*)OTA_APP_START;
      if (av[0]>=0x20000000 && av[0]<=0x20005000 && av[1]>=0x08000000 && av[1]<=0x08010000)
          { led_off(); jump_to_app(OTA_APP_START); } }

    /* ⑥ 全部失败 — OLED 报错 */
    oled_init_show("BOOT ERROR","UART OTA");
    led_on();
    while(1) {
        uint8_t b[OTA_FRAME_MAX_SIZE];
        size_t n=ota_xport_recv(&transport,b,sizeof(b),1000);
        if(n>0){uint8_t t,s;const uint8_t*p;size_t pl;
            if(ota_frame_decode(b,n,&t,&s,&p,&pl)&&t==OTA_CMD_HELLO) goto start_ota;}
    }

start_ota:
    led_on(); ota_client_start(&ota);
    while(ota_client_poll(&ota)){}
    if (ota_client_get_state(&ota) == OTA_STATE_COMPLETE) ota_client_boot(&ota);
    led_off(); while(1){ led_on(); for(volatile int i=0;i<500000;i++)__asm__("nop");
                          led_off(); for(volatile int i=0;i<500000;i++)__asm__("nop"); }
}
