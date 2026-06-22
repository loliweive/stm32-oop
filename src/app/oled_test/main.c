/**
 * OLED test — 8x16 font, dynamic counter, verified on DShanMCU-F103
 */
#include "stm32f103xb.h"
#include "rcc.h"
#include "gpio.h"
#include "i2c.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* ── LED ───────────────────────────────────────────────── */
static GpioPin _led;
static void L_ON(void)  { gpio_set(&_led, 0); }
static void L_OFF(void) { gpio_set(&_led, 1); }
static void dly(uint32_t n) { while (n--) __asm__("nop"); }

/* ── I2C ───────────────────────────────────────────────── */
#define I2C_TO   100000
#define OLED_ADR 0x3C
static I2C_Type *g_i2c;

static bool i2c_send(const uint8_t *buf, size_t len)
{
    uint32_t to;
    to=I2C_TO; while((g_i2c->SR2&I2C_SR2_BUSY)&&--to){}
    g_i2c->CR1|=I2C_CR1_START;
    to=I2C_TO; while(!(g_i2c->SR1&I2C_SR1_SB)&&--to){}
    if(!to){g_i2c->CR1|=I2C_CR1_STOP;return false;}
    g_i2c->DR=(uint8_t)(OLED_ADR<<1);
    to=I2C_TO; while(!(g_i2c->SR1&(I2C_SR1_ADDR|I2C_SR1_AF))&&--to){}
    if(!to||(g_i2c->SR1&I2C_SR1_AF)){(void)g_i2c->SR2;g_i2c->CR1|=I2C_CR1_STOP;return false;}
    (void)g_i2c->SR2;
    for(size_t n=0;n<len;n++){to=I2C_TO;while(!(g_i2c->SR1&I2C_SR1_TXE)&&--to){}if(!to){g_i2c->CR1|=I2C_CR1_STOP;return false;}g_i2c->DR=buf[n];}
    to=I2C_TO;while(!(g_i2c->SR1&I2C_SR1_BTF)&&--to){}
    g_i2c->CR1|=I2C_CR1_STOP;
    return true;
}

static void oled_cmd(uint8_t c)  { uint8_t b[2]={0,c}; i2c_send(b,2); }
static void oled_data(const uint8_t *d, size_t n) {
    /* Send in chunks with 0x40 prefix */
    uint8_t hdr[129]; hdr[0]=0x40;
    for(size_t o=0;o<n;o+=128){
        size_t chunk=(n-o>128)?128:(n-o);
        memcpy(hdr+1,d+o,chunk);
        i2c_send(hdr,chunk+1);
    }
}

/* ── 8x16 font ─────────────────────────────────────────── */
extern const uint8_t ascii_font[][16];

static void draw_char(int page, int col, char c)
{
    unsigned uc = ((unsigned)c<32||(unsigned)c>127)?32:(unsigned)c;
    oled_cmd(0xB0+page); oled_cmd(col&0x0F); oled_cmd(0x10|(col>>4));
    i2c_send((uint8_t*)&ascii_font[uc][0],8);  /* need to wrap in 0x40 prefix... */
}

/* Actually use proper data send for each char half */
static void put_char(int page, int col, char c)
{
    unsigned uc = ((unsigned)c<32||(unsigned)c>127)?32:(unsigned)c;
    /* Upper half */
    { uint8_t b[9]; b[0]=0x40; for(int i=0;i<8;i++)b[1+i]=ascii_font[uc][i];
      oled_cmd(0xB0+page); oled_cmd(col&0x0F); oled_cmd(0x10|(col>>4));
      i2c_send(b,9); }
    /* Lower half */
    { uint8_t b[9]; b[0]=0x40; for(int i=0;i<8;i++)b[1+i]=ascii_font[uc][i+8];
      oled_cmd(0xB0+page+1); oled_cmd(col&0x0F); oled_cmd(0x10|(col>>4));
      i2c_send(b,9); }
}

static void put_str(int page, const char *s)
{
    int col=0; while(*s){put_char(page,col,*s++);col+=8;}
}

static void clear_page(int page)
{
    oled_cmd(0xB0+page); oled_cmd(0); oled_cmd(0x10);
    uint8_t z[129]; z[0]=0x40; for(int i=1;i<129;i++)z[i]=0;
    i2c_send(z,129);
}

/* ═══════════════════════════════════════════════════════ */
int main(void)
{
    rcc_set_sysclk(RCC_HSI,0);
    rcc_enable_gpio('B'); rcc_enable_gpio('C'); rcc_enable_i2c(1);

    /* LED */
    GpioPin_ctor(&_led,GPIOC,GPIO_PIN_13); gpio_set_mode(&_led,GPIO_MODE_OUT_PP); L_OFF();

    /* I2C GPIO */
    { GpioPin s,s2; GpioPin_ctor(&s,GPIOB,GPIO_PIN_6); gpio_set_mode(&s,GPIO_CNF_ALT_OD|GPIO_MODE_OUT_50MHZ);
      GpioPin_ctor(&s2,GPIOB,GPIO_PIN_7); gpio_set_mode(&s2,GPIO_CNF_ALT_OD|GPIO_MODE_OUT_50MHZ);
      gpio_set(&s,1); gpio_set(&s2,1); }

    /* I2C init */
    { I2cPort p; I2cPort_ctor(&p,I2C1,400000,8000000); i2c_init(&p); }
    g_i2c=I2C1;

    /* Init OLED */
    { uint8_t cmds[]={0xAE,0x20,0x02,0xA8,0x3F,0xD3,0x00,0x40,0xA1,0xC8,
      0xDA,0x12,0x81,0xFF,0xA4,0xA6,0xD5,0x80,0x8D,0x14,0xAF};
      for(size_t i=0;i<sizeof(cmds);i++)oled_cmd(cmds[i]); }

    /* Clear all */
    for(int p=0;p<8;p++)clear_page(p);

    /* Title: pages 1-2 */
    put_str(1,"OLED OK!");

    /* Counter: pages 4-5 */
    int cnt=0;
    char buf[32];
    extern int snprintf(char*,unsigned long,const char*,...);

    while(1){
        clear_page(4); clear_page(5);
        snprintf(buf,sizeof(buf),"%d",cnt++);
        put_str(4,buf);
        L_ON(); dly(400000); L_OFF(); dly(8000000);
    }
}
