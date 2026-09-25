#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "bsp/board_api.h"
#include "tusb.h"

#define GREEN_LED 15
#define RED_LED   14

#define UART_ID      uart1
#define UART_BAUD    115200
#define UART_TX_PIN  4

#define FAST_INTERVAL_MS  50
#define FAST_REPORT_LIMIT 3

static uint32_t last_press_time = 0;
static bool have_previous_press = false;
static uint8_t fast_count = 0;


/*
 * Convert the incoming HID report into our
 * standard 8-byte keyboard format:
 *
 * [0] modifier
 * [1] reserved
 * [2] key 1
 * [3] key 2
 * [4] key 3
 * [5] key 4
 * [6] key 5
 * [7] key 6
 */
static void normalize_report(
    uint8_t const *src,
    uint16_t len,
    uint8_t *dst)
{
    for (int i = 0; i < 8; i++)
        dst[i] = 0;

    if (len == 0)
        return;

    /*
     * Standard 8-byte boot keyboard report.
     */
    if (len >= 8)
    {
        for (int i = 0; i < 8; i++)
            dst[i] = src[i];

        return;
    }

    /*
     * Short keyboard report.
     *
     * Assume:
     * src[0] = modifier
     * src[1...] = keycodes
     */
    dst[0] = src[0];

    for (uint16_t i = 1; i < len && i < 7; i++)
    {
        dst[i + 1] = src[i];
    }
}


/*
 * Determine whether the normalized report
 * contains a key press.
 */
static bool report_has_key_press(
    uint8_t const *report)
{
    for (int i = 2; i < 8; i++)
    {
        if (report[i] != 0)
            return true;
    }

    return false;
}


/*
 * Send normalized 8-byte report to Pico #2.
 */
static void send_report_uart(
    uint8_t const *report)
{
    uart_putc(UART_ID, 'K');

    for (int i = 0; i < 8; i++)
        uart_putc(UART_ID, report[i]);
}


void tuh_hid_mount_cb(
    uint8_t dev_addr,
    uint8_t instance,
    uint8_t const *desc_report,
    uint16_t desc_len)
{
    (void)desc_report;
    (void)desc_len;

    have_previous_press = false;
    fast_count = 0;

    tuh_hid_receive_report(dev_addr, instance);
}


void tuh_hid_umount_cb(
    uint8_t dev_addr,
    uint8_t instance)
{
    (void)dev_addr;
    (void)instance;

    have_previous_press = false;
    fast_count = 0;
}


void tuh_hid_report_received_cb(
    uint8_t dev_addr,
    uint8_t instance,
    uint8_t const *report,
    uint16_t len)
{
    uint8_t normalized[8];

    normalize_report(
        report,
        len,
        normalized
    );

    bool is_key_press =
        report_has_key_press(normalized);

    bool block = false;


    /*
     * Analyze timing only on actual key presses.
     */
    if (is_key_press)
    {
        uint32_t now =
            to_ms_since_boot(get_absolute_time());

        if (have_previous_press)
        {
            uint32_t interval =
                now - last_press_time;

            if (interval < FAST_INTERVAL_MS)
            {
                if (fast_count < FAST_REPORT_LIMIT)
                    fast_count++;

                if (fast_count >= FAST_REPORT_LIMIT)
                    block = true;
            }
            else
            {
                fast_count = 0;
            }
        }

        last_press_time = now;
        have_previous_press = true;
    }


    /*
     * BLOCK
     */
    if (block)
    {
        gpio_put(GREEN_LED, 0);

        gpio_put(RED_LED, 1);
        sleep_ms(100);
        gpio_put(RED_LED, 0);
    }


    /*
     * ALLOW
     */
    else
    {
        gpio_put(RED_LED, 0);

        gpio_put(GREEN_LED, 1);

        send_report_uart(normalized);

        sleep_ms(5);

        gpio_put(GREEN_LED, 0);
    }


    tuh_hid_receive_report(
        dev_addr,
        instance
    );
}


int main(void)
{
    board_init();

    gpio_init(GREEN_LED);
    gpio_set_dir(GREEN_LED, GPIO_OUT);
    gpio_put(GREEN_LED, 0);

    gpio_init(RED_LED);
    gpio_set_dir(RED_LED, GPIO_OUT);
    gpio_put(RED_LED, 0);


    uart_init(
        UART_ID,
        UART_BAUD
    );

    gpio_set_function(
        UART_TX_PIN,
        GPIO_FUNC_UART
    );


    tusb_rhport_init_t host_init = {
        .role = TUSB_ROLE_HOST,
        .speed = TUSB_SPEED_AUTO
    };

    tusb_init(
        0,
        &host_init
    );


    while (true)
    {
        tuh_task();
    }
}
