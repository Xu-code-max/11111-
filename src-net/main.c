/*-
 * Copyright (c) 2019-2020 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/console.h>
#include <sys/callout.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/thread.h>
#include <sys/ringbuf.h>

#include <arm/arm/nvic.h>
#include <arm/nordicsemi/nrf5340_net_core.h>

#include <nrfxlib/ble_controller/include/ble_controller.h>

#include "common.h"
#include "ble.h"

struct arm_nvic_softc nvic_sc;
struct nrf_uarte_softc uarte_sc;
struct nrf_spu_softc spu_sc;
struct nrf_power_softc power_sc;
struct nrf_timer_softc timer1_sc;
struct nrf_ipc_softc ipc_sc;
struct mdx_ringbuf_softc ringbuf_tx_sc;
struct mdx_ringbuf_softc ringbuf_rx_sc;

#define	UART_PIN_TX	25
#define	UART_PIN_RX	26
#define	UART_BAUDRATE	115200

static void
ble_rng_intr(void *arg, struct trapframe *tf, int irq)
{

	ble_controller_RNG_IRQHandler();
}

static void
ble_timer_intr(void *arg, struct trapframe *tf, int irq)
{

	ble_controller_TIMER0_IRQHandler();
}

static void
ble_radio_intr(void *arg, struct trapframe *tf, int irq)
{

	ble_controller_RADIO_IRQHandler();
}

static void
ble_rtc_intr(void *arg, struct trapframe *tf, int irq)
{

	ble_controller_RTC0_IRQHandler();
}

static void
ble_power_intr(void *arg, struct trapframe *tf, int irq)
{

	ble_controller_POWER_CLOCK_IRQHandler();
}

static void
nrf_egu0_intr(void *arg, struct trapframe *tf, int irq)
{

	ble_controller_low_prio_tasks_process();
}

static void
uart_putchar(int c, void *arg)
{
	struct nrf_uarte_softc *sc;
 
	sc = arg;
 
	if (c == '\n')
		nrf_uarte_putc(sc, '\r');

	nrf_uarte_putc(sc, c);
}

static void
nrf_input(int c, void *arg)
{

}

void
board_init(void)
{

	nrf_uarte_init(&uarte_sc, NRF_UARTE0,
	    UART_PIN_TX, UART_PIN_RX, UART_BAUDRATE);
	mdx_console_register(uart_putchar, (void *)&uarte_sc);
	nrf_uarte_register_callback(&uarte_sc, nrf_input, NULL);

	printf("mdepx initialized\n");

	nrf_timer_init(&timer1_sc, NRF_TIMER1);
	nrf_power_init(&power_sc, NRF_POWER);
	nrf_ipc_init(&ipc_sc, NRF_IPC);

	arm_nvic_init(&nvic_sc, BASE_SCS);

	arm_nvic_route_intr(&nvic_sc, ID_UARTE0, nrf_uarte_intr, &uarte_sc);
	arm_nvic_route_intr(&nvic_sc, ID_EGU0,   nrf_egu0_intr,  NULL);
	arm_nvic_route_intr(&nvic_sc, ID_RNG,    ble_rng_intr,   NULL);
	arm_nvic_route_intr(&nvic_sc, ID_TIMER0, ble_timer_intr, NULL);
	arm_nvic_route_intr(&nvic_sc, ID_TIMER1, nrf_timer_intr, &timer1_sc);
	arm_nvic_route_intr(&nvic_sc, ID_RADIO,  ble_radio_intr, NULL);
	arm_nvic_route_intr(&nvic_sc, ID_RTC0,   ble_rtc_intr,   NULL);
	arm_nvic_route_intr(&nvic_sc, ID_POWER,  ble_power_intr, NULL);
	arm_nvic_route_intr(&nvic_sc, ID_IPC,    nrf_ipc_intr,   &ipc_sc);

	arm_nvic_enable_intr(&nvic_sc, ID_TIMER1);
	arm_nvic_enable_intr(&nvic_sc, ID_UARTE0);
	arm_nvic_enable_intr(&nvic_sc, ID_EGU0);
	arm_nvic_enable_intr(&nvic_sc, ID_IPC);

	/* Send event 1 to channel 1 */
	nrf_ipc_configure_send(&ipc_sc, 1, (1 << 1));

	/* Receive event 0 on channel 0 */
	nrf_ipc_configure_recv(&ipc_sc, 0, (1 << 0), ble_ipc_intr, NULL);
	nrf_ipc_inten(&ipc_sc, 0, true);

	mdx_ringbuf_init(&ringbuf_rx_sc,
	    (void *)RINGBUF_RX_BASE, RINGBUF_RX_BASE_SIZE,
	    (void *)RINGBUF_RX_BUF, RINGBUF_RX_BUF_SIZE);
	mdx_ringbuf_join(&ringbuf_tx_sc,
	    (void *)RINGBUF_TX_BASE);
}

int
main(void)
{

	printf("Hello world!\n");

	ble_test();

	while (1)
		mdx_tsleep(2000000);

	return (0);
}
