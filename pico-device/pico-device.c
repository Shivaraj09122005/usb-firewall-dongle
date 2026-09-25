#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "bsp/board_api.h"
#include "tusb.h"

#define UART_ID      uart0
#define UART_BAUD    115200
#define UART_RX_PIN  1

static uint8_t report[8];
static uint8_t rx_index = 0;
static bool receiving = false;

int main(void)
{
    board_init();

    uart_init(UART_ID, UART_BAUD);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    tusb_rhport_init_t dev_init = {
        .role = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_AUTO
    };

    tusb_init(0, &dev_init);

    while (true)
    {
        tud_task();

        while (uart_is_readable(UART_ID))
        {
            uint8_t c = uart_getc(UART_ID);

            if (!receiving)
            {
                if (c == 'K')
                {
                    receiving = true;
                    rx_index = 0;
                }
                continue;
            }

            report[rx_index++] = c;

            if (rx_index == 8)
            {
                receiving = false;
                rx_index = 0;

                if (tud_hid_ready())
                {
                    tud_hid_keyboard_report(
                        0,
                        report[0],
                        &report[2]
                    );
                }
            }
        }
    }
}

void tud_hid_set_report_cb(
    uint8_t instance,
    uint8_t report_id,
    hid_report_type_t report_type,
    uint8_t const *buffer,
    uint16_t bufsize)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)bufsize;
}

uint16_t tud_hid_get_report_cb(
    uint8_t instance,
    uint8_t report_id,
    hid_report_type_t report_type,
    uint8_t *buffer,
    uint16_t reqlen)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;

    return 0;
}
