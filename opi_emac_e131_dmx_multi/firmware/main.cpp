/**
 * @file main.cpp
 *
 */
/* Copyright (C) 2019 by Arjan van Vught mailto:info@raspberrypi-dmx.nl
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <stdint.h>

#include "hardwarebaremetal.h"
#include "networkh3emac.h"
#include "ledblinkbaremetal.h"

#include "console.h"
#include "display.h"

#include "e131bridge.h"
#include "e131params.h"

// DMX Out
#include "dmxparams.h"
#include "h3/dmxsendmulti.h"

#include "spiflashinstall.h"
#include "spiflashstore.h"

#include "software_version.h"

static const char NETWORK_INIT[] = "Network init ...";
static const char BRIDGE_PARMAS[] = "Setting Bridge parameters ...";
static const char START_BRIDGE[] = "Starting the Bridge ...";
static const char BRIDGE_STARTED[] = "Bridge started";

extern "C" {

void notmain(void) {
	HardwareBaremetal hw;
	NetworkH3emac nw;
	LedBlinkBaremetal lb;
	Display display(DISPLAY_SSD1306);

	if (hw.GetBootDevice() == BOOT_DEVICE_MMC0) {
		SpiFlashInstall spiFlashInstall;
	}

	SpiFlashStore spiFlashStore;
	E131Params e131params((E131ParamsStore *) spiFlashStore.GetStoreE131());

	if (e131params.Load()) {
		e131params.Dump();
	}

	const TE131OutputType tOutputType = e131params.GetOutputType();


	uint8_t nHwTextLength;
	printf("[V%s] %s Compiled on %s at %s\n", SOFTWARE_VERSION, hw.GetBoardName(nHwTextLength), __DATE__, __TIME__);

	console_puts("Ethernet sACN E1.31 ");
	console_set_fg_color(tOutputType == E131_OUTPUT_TYPE_DMX ? CONSOLE_GREEN : CONSOLE_WHITE);
	console_puts("DMX Output");
	console_set_fg_color(CONSOLE_WHITE);
#if defined(ORANGE_PI)
	console_puts(" {2 Universes}\n");
#else
	console_puts(" {4 Universes}\n");
#endif

	hw.SetLed(HARDWARE_LED_ON);

	console_status(CONSOLE_YELLOW, NETWORK_INIT);
	display.TextStatus(NETWORK_INIT);

	nw.Init((NetworkParamsStore *)spiFlashStore.GetStoreNetwork());
	nw.Print();

	console_status(CONSOLE_YELLOW, BRIDGE_PARMAS);
	display.TextStatus(BRIDGE_PARMAS);

	E131Bridge bridge;
	e131params.Set(&bridge);

	uint16_t nUniverse;
	bool bIsSetIndividual = false;
	bool bIsSet;

	nUniverse = e131params.GetUniverse(0, bIsSet);
	if (bIsSet) {
		bridge.SetUniverse(0, E131_OUTPUT_PORT, nUniverse);
		bIsSetIndividual = true;
	}

	nUniverse = e131params.GetUniverse(1, bIsSet);
	if (bIsSet) {
		bridge.SetUniverse(1, E131_OUTPUT_PORT, nUniverse);
		bIsSetIndividual = true;
	}
#if defined (ORANGE_PI_ONE)
	nUniverse = e131params.GetUniverse(2, bIsSet);
	if (bIsSet) {
		bridge.SetUniverse(2, E131_OUTPUT_PORT, nUniverse);
		bIsSetIndividual = true;
	}
#ifndef DO_NOT_USE_UART0
	nUniverse = e131params.GetUniverse(3, bIsSet);
	if (bIsSet) {
		bridge.SetUniverse(3, E131_OUTPUT_PORT, nUniverse);
		bIsSetIndividual = true;
	}
#endif
#endif

	if (!bIsSetIndividual) { // Backwards compatibility
		nUniverse = e131params.GetUniverse();
		bridge.SetUniverse(0, E131_OUTPUT_PORT, 0 + nUniverse);
		bridge.SetUniverse(1, E131_OUTPUT_PORT, 1 + nUniverse);
#if defined (ORANGE_PI_ONE)
		bridge.SetUniverse(2, E131_OUTPUT_PORT, 2 + nUniverse);
#ifndef DO_NOT_USE_UART0
		bridge.SetUniverse(3, E131_OUTPUT_PORT, 3 + nUniverse);
#endif
#endif
	}

	DMXSendMulti dmx;
	DMXParams dmxparams((DMXParamsStore *)spiFlashStore.GetStoreDmxSend());

	if (!hw.IsButtonPressed()) {
		if (dmxparams.Load()) {
			dmxparams.Dump();
			dmxparams.Set(&dmx);
		}
	}

	bridge.SetOutput(&dmx);

	display.Cls();
	display.Printf(1, "Eth sACN E1.31 DMX");
	display.Printf(2, "%s", hw.GetBoardName(nHwTextLength));
	display.Printf(3, "IP: " IPSTR "", IP2STR(Network::Get()->GetIp()));
	if (nw.IsDhcpKnown()) {
		if (nw.IsDhcpUsed()) {
			display.PutString(" D");
		} else {
			display.PutString(" S");
		}
	}
	display.Printf(4, "N: " IPSTR "", IP2STR(Network::Get()->GetNetmask()));
	if (!bIsSetIndividual) {
		display.Printf(5, "U: %d", nUniverse);
	}
	display.Printf(6, "Active ports: %d", bridge.GetActiveOutputPorts());

	console_status(CONSOLE_YELLOW, START_BRIDGE);
	display.TextStatus(START_BRIDGE);

	bridge.Start();

	console_status(CONSOLE_GREEN, BRIDGE_STARTED);
	display.TextStatus(BRIDGE_STARTED);

	while (spiFlashStore.Flash())
		;

	hw.WatchdogInit();

	for (;;) {
		hw.WatchdogFeed();
		nw.Run();
		bridge.Run();
		lb.Run();
	}
}

}
