/**
 * @file oscws28xx.cpp
 *
 */
/* Copyright (C) 2017 by Arjan van Vught mailto:info@raspberrypi-dmx.nl
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

#include <stdint.h>
#include <stdio.h>

#include "hardware.h"
#include "util.h"
#include "console.h"

#include "osc.h"
#include "oscsend.h"
#include "oscmessage.h"
#include "oscws28xx.h"

#include "ws28xx.h"

#include "network.h"
#include "ip_address.h"

#include "software_version.h"

OSCWS28xx::OSCWS28xx(const int OutgoingPort, const int nLEDCount, const _ws28xxx_type nLEDType, const char *sLEDType) : m_Blackout(false) {
	m_OutgoingPort = OutgoingPort;
	m_nLEDCount = nLEDCount;
	m_nLEDType = nLEDType;
	m_LEDType = sLEDType;
	snprintf(m_Os, sizeof(m_Os), "[V%s] %s", SOFTWARE_VERSION, __DATE__);
	m_pModel = hardware_board_get_model();
	m_pSoC = hardware_board_get_soc();
}

OSCWS28xx::~OSCWS28xx(void) {
	Stop();
}

void OSCWS28xx::Start(void) {
	ws28xx_init(m_nLEDCount, m_nLEDType, 0);
	console_save_cursor();
	console_set_cursor(80, 0);
	console_set_fg_color(CONSOLE_CYAN);
	console_puts("outputting");
	console_restore_cursor();
}

void OSCWS28xx::Stop(void) {
	ws28xx_blackout();
	m_Blackout = true;
	console_save_cursor();
	console_set_cursor(80, 0);
	console_set_fg_color(CONSOLE_YELLOW);
	console_puts("blackout  ");
	console_restore_cursor();
}

void OSCWS28xx::Run(void) {
	uint16_t from_port;
	uint32_t from_ip;

	const int len = network_recvfrom((const uint8_t *) m_packet, (const uint16_t) FRAME_BUFFER_SIZE, &from_ip, &from_port);

	if (len == 0) {
		return;
	}

	if (OSC::isMatch((const char*) m_packet, "/ping")) {
		OSCSend MsgSend(from_ip, m_OutgoingPort, "/pong", 0);
	} else if (OSC::isMatch((const char*) m_packet, "/dmx1/blackout")) {
		OSCMessage Msg(m_packet, (unsigned) len);
		m_Blackout = (unsigned) Msg.GetFloat(0) == 1;
		console_save_cursor();
		console_set_cursor(80, 0);
		if (m_Blackout) {
			ws28xx_blackout();
			console_set_fg_color(CONSOLE_YELLOW);
			console_puts("blackout  ");
		} else {
			ws28xx_update();
			console_set_fg_color(CONSOLE_CYAN);
			console_puts("outputting");
		}
		console_restore_cursor();
	} else if (OSC::isMatch((const char*) m_packet, "/dmx1/*")) {
		OSCMessage Msg(m_packet, (unsigned) len);

		const char *p = (const char *) m_packet + 6;
		const unsigned dmx_channel = (unsigned) (*p - '0');
		const unsigned dmx_value = (unsigned) Msg.GetFloat(0);

		const unsigned index = dmx_channel - 1;	// DMX channel starts with 1

		if (index < 3) {
			m_RGBColour[index] = dmx_value;
		}

		for (unsigned j = 0; j < m_nLEDCount; j++) {
			ws28xx_set_led(j, m_RGBColour[0], m_RGBColour[1], m_RGBColour[2]);
		}

		if (!m_Blackout) {
			ws28xx_update();
		}

		console_save_cursor();
		console_set_cursor(80, 1);
		console_puthex_fg_bg(m_RGBColour[0], CONSOLE_RED, CONSOLE_BLACK);
		console_puthex_fg_bg(m_RGBColour[1], CONSOLE_GREEN, CONSOLE_BLACK);
		console_puthex_fg_bg(m_RGBColour[2], CONSOLE_BLUE, CONSOLE_BLACK);
		console_restore_cursor();
	} else if (OSC::isMatch((const char*) m_packet, "/2")) {
		OSCSend MsgSendInfo(from_ip, m_OutgoingPort, "/info/os", "s", m_Os);
		OSCSend MsgSendModel(from_ip, m_OutgoingPort, "/info/model", "s", m_pModel);
		OSCSend MsgSendSoc(from_ip, m_OutgoingPort, "/info/soc", "s", m_pSoC);
		OSCSend MsgSendLedType(from_ip, m_OutgoingPort, "/info/ledtype", "s", m_LEDType);
		OSCSend MsgSendLedCount(from_ip, m_OutgoingPort, "/info/ledcount", "i", m_nLEDCount);
	}

	printf("\n%d: " IPSTR ":%d ", (int)len, IP2STR(from_ip), (int) from_port);

	for (int i = 0; i < MIN(32, len); i++) {
		printf("%c", isprint(m_packet[i]) ? m_packet[i] : '.');
	}
}
