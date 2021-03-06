#include <stdio.h>
#include <stdlib.h>
#include <console.h>
#include <string.h>
#include <generated/csr.h>
#include <generated/mem.h>
#include <generated/sdram_phy.h>
#include <time.h>

#include "ad9984a.h"
#include "ci.h"
#include "i2c.h"
#include "vga_in.h"
#include "processor.h"
#include "pll.h"
#include "config.h"

int status_enabled;
unsigned char atlys_leds_enable = 1;
unsigned char i2c_leds_enable = 1;

static void help_atlys_leds(void)
{
	puts("atlys_leds on                  - enable atlys leds");
	puts("atlys_leds off                 - disable atlys leds");
}

static void help_i2c_leds(void)
{
	puts("i2c_leds on                    - enable pcf8574a leds");
	puts("i2c_leds off                   - disable pcf8574a leds");
}
static void help_vga_switch(void)
{
	puts("vga_swtich on                  - display vga output on HDMI_OUT0");
	puts("vga_switch off                 - display back the pattern");
}
static void help_debug(void)
{
	puts("debug ad9984a                  - dump ad9984a register configuration");
	puts("debug ddr                      - show DDR bandwidth");
    puts("debug vga_in                   - dump vga_in fb data and registers");
}

static void help_vga_in(void)
{
    puts("clear_fb                       - clear framebuffers of vga_in");
    puts("disable_vga_in                 - disable vga_in");
    puts("vga_in_init                    - initialize vga_in");
}

static void help(void)
{
	puts("help                           - this command");
	puts("version                        - firmware/gateware version");
	puts("reboot                         - reboot CPU");
	puts("status <on/off>                - enable/disable status message (same with by pressing enter)");
	puts("");
	help_atlys_leds();
	puts("");
    help_i2c_leds();
	puts("");
    help_vga_switch();
    puts("");
    help_vga_in();
    puts("");
	help_debug();
}

static void version(void)
{
	printf("gateware revision: %08x\n", identifier_revision_read());
	printf("firmware revision: %08x, built "__DATE__" "__TIME__"\n", MSC_GIT_ID);
}

static void reboot(void)
{
	asm("call r0");
}

static void status_enable(void)
{
	printf("Enabling status\n");
	status_enabled = 1;
}

static void status_disable(void)
{
	printf("Disabling status\n");
	status_enabled = 0;
}

static void debug_ddr(void);

static void status_print(void)
{
	ad9984a_status();
    printf("ddr: ");
	debug_ddr();
}

static void status_service(void)
{
	static int last_event;

	if(elapsed(&last_event, identifier_frequency_read())) {
		if(status_enabled) {
			status_print();
			printf("\n");
		}
	}
}

static void leds_service(void)
{
    static int last_check;
    static unsigned char counter = 0;
    static unsigned char flag = 0 ;

    /* Leds at 4Hz */
	if(elapsed(&last_check, identifier_frequency_read()/4)) {
		if(atlys_leds_enable) {
			counter++;
            led_r_leds_write(counter);
		}
        
        if (i2c_leds_enable) {
            if (flag)
            {
                i2c_start_cond();
                i2c_write(0x38 << 1);
                i2c_write(0x55);
                i2c_stop_cond();
                
                i2c_start_cond();
                i2c_write(0x39 << 1);
                i2c_write(0x55);
                i2c_stop_cond();
            }
            else
            {
                i2c_start_cond();
                i2c_write(0x38 << 1);
                i2c_write(0xAA);
                i2c_stop_cond();
                
                i2c_start_cond();
                i2c_write(0x39 << 1);
                i2c_write(0xAA);
                i2c_stop_cond();
            }
            flag = !flag;
        }
	}
}

static void atlys_leds_on(void)
{
	printf("Enabling atlys leds\n");
	atlys_leds_enable = 1;
}

static void atlys_leds_off(void)
{
	printf("Disabling atlys leds\n");
	atlys_leds_enable = 0;
}

static void i2c_leds_on(void)
{
	printf("Enabling pcf8574a i2c leds\n");
	i2c_leds_enable = 1;
}

static void i2c_leds_off(void)
{
	printf("Disabling pcf8574a i2c leds\n");
	i2c_leds_enable = 0;
}
static void vga_switch_on(void)
{
    printf("Switching on VGA output on HDMI_OUT0...\n");
    processor_set_hdmi_out0_source(VIDEO_IN_HDMI_IN0);
    processor_update();
}
static void vga_switch_off(void)
{
    printf("Switching off VGA output and reverting to pattern o/p on HDMI_OUT0...\n");
    processor_set_hdmi_out0_source(VIDEO_IN_PATTERN);
    processor_update();
}
static unsigned int log2(unsigned int v)
{
	unsigned int r = 0;
	while(v>>=1) r++;
	return r;
}

static void debug_ddr(void)
{
	unsigned long long int nr, nw;
	unsigned long long int f;
	unsigned int rdb, wrb;
	unsigned int burstbits;

	sdram_controller_bandwidth_update_write(1);
	nr = sdram_controller_bandwidth_nreads_read();
	nw = sdram_controller_bandwidth_nwrites_read();
	f = identifier_frequency_read();
	burstbits = (2*DFII_NPHASES) << DFII_PIX_DATA_SIZE;
	rdb = (nr*f >> (24 - log2(burstbits)))/1000000ULL;
	wrb = (nw*f >> (24 - log2(burstbits)))/1000000ULL;
	printf("read:%5dMbps  write:%5dMbps  all:%5dMbps\n", rdb, wrb, rdb + wrb);
}

static char *readstr(void)
{
	char c[2];
	static char s[64];
	static int ptr = 0;

	if(readchar_nonblock()) {
		c[0] = readchar();
		c[1] = 0;
		switch(c[0]) {
			case 0x7f:
			case 0x08:
				if(ptr > 0) {
					ptr--;
					putsnonl("\x08 \x08");
				}
				break;
			case 0x07:
				break;
			case '\r':
			case '\n':
				s[ptr] = 0x00;
				putsnonl("\n");
				ptr = 0;
				return s;
			default:
				if(ptr >= (sizeof(s) - 1))
					break;
				putsnonl(c);
				s[ptr] = c[0];
				ptr++;
				break;
		}
	}
	return NULL;
}

static char *get_token(char **str)
{
	char *c, *d;

	c = (char *)strchr(*str, ' ');
	if(c == NULL) {
		d = *str;
		*str = *str+strlen(*str);
		return d;
	}
	*c = 0;
	d = *str;
	*str = c+1;
	return d;
}


void ci_prompt(void)
{
	printf("HDMI2USB> ");
}

void ci_service(void)
{
	char *str;
	char *token;

	status_service();
    leds_service();

	str = readstr();
	if(str == NULL) return;

	token = get_token(&str);

	if(strcmp(token, "help") == 0) {
		puts("Available commands:");
		token = get_token(&str);
		if(strcmp(token, "atlys_leds") == 0)
			help_atlys_leds();
		else if(strcmp(token, "i2c_leds") == 0)
			help_i2c_leds();

		else if(strcmp(token, "debug") == 0)
			help_debug();
		else
			help();
		puts("");
	}
	else if(strcmp(token, "reboot") == 0) reboot();
	else if(strcmp(token, "version") == 0) version();
	

	else if(strcmp(token, "atlys_leds") == 0) {
		token = get_token(&str);
		if(strcmp(token, "on") == 0)
			atlys_leds_on();
		else if(strcmp(token, "off") == 0)
			atlys_leds_off();
		else
			help_atlys_leds();
	}
	else if(strcmp(token, "i2c_leds") == 0) {
		token = get_token(&str);
		if(strcmp(token, "on") == 0)
			i2c_leds_on();
		else if(strcmp(token, "off") == 0)
			i2c_leds_off();
		else
			help_i2c_leds();
	}
    else if(strcmp(token, "vga_switch") == 0) {
		token = get_token(&str);
		if(strcmp(token, "on") == 0)
			vga_switch_on();
		else if(strcmp(token, "off") == 0)
			vga_switch_off();
		else
			help_vga_switch();
	}
	else if(strcmp(token, "status") == 0) {
		token = get_token(&str);
		if(strcmp(token, "on") == 0)
			status_enable();
		else if(strcmp(token, "off") == 0)
			status_disable();
		else
			status_print();
	}
    else if(strcmp(token, "clear_fb") == 0) {
        vga_in_clear_framebuffers();
    }
    else if(strcmp(token, "disable_vga_in") == 0) {
        vga_in_disable();
    }
    else if(strcmp(token, "vga_in_init") == 0) {
        vga_in_start();
    }
	else if(strcmp(token, "debug") == 0) {
		token = get_token(&str);
		if(strcmp(token, "ad9984a") == 0)
			ad9984a_debug();
		else if(strcmp(token, "ddr") == 0)
			debug_ddr();
        else if(strcmp(token, "vga_in") == 0)
            vga_in_dump_fb();
		else
			help_debug();
	} else {
		if(status_enabled)
			status_disable();
	}

	ci_prompt();
}
