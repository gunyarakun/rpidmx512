/**
 * @file rdm_send_respond.c
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
#include <stdbool.h>

#include "dmx.h"

#include "rdm.h"
#include "rdm_e120.h"
#include "rdm_device_info.h"
#include "rdm_send.h"
#include "rdm_queued_message.h"

#include "hardware.h"

/**
 * @ingroup rdm
 *
 * @param rdm_data
 * @param response_type
 * @param value
 */
static void rdm_send_respond_message(uint8_t *rdm_data, const uint8_t response_type, const uint16_t value) {
	uint8_t i;
	uint16_t rdm_checksum;
	uint8_t *uid_device;
	uint64_t delay;

	struct _rdm_command *rdm_response = (struct _rdm_command *) rdm_data;

	rdm_response->message_count = rdm_queued_message_get_count();

	switch (response_type) {
	case E120_RESPONSE_TYPE_ACK:
		rdm_response->slot16.response_type = E120_RESPONSE_TYPE_ACK;
		break;
	case E120_RESPONSE_TYPE_NACK_REASON:
	case E120_RESPONSE_TYPE_ACK_TIMER:
		rdm_response->message_length = RDM_MESSAGE_MINIMUM_SIZE + 2;
		rdm_response->slot16.response_type = response_type;
		rdm_response->param_data_length = 2;
		rdm_response->param_data[0] = (uint8_t) (value >> 8);
		rdm_response->param_data[1] = (uint8_t) value;
		break;
	default:
		// forces timeout
		return;
		// Unreachable code: break;
	}

	uid_device = (uint8_t *)rdm_device_info_get_uuid();

	for (i = 0; i < RDM_UID_SIZE; i++) {
		rdm_response->destination_uid[i] = rdm_response->source_uid[i];
		rdm_response->source_uid[i] = uid_device[i];
	}

	rdm_response->command_class++;

	rdm_checksum = 0;

	for (i = 0; i < rdm_response->message_length; i++) {
		rdm_checksum += rdm_data[i];
	}

	rdm_data[i++] = rdm_checksum >> 8;
	rdm_data[i] = rdm_checksum & 0XFF;

	delay = hardware_micros() - rdm_get_data_receive_end();
	// 3.2.2 Responder Packet spacing
	if (delay < RDM_RESPONDER_PACKET_SPACING) {
		udelay(RDM_RESPONDER_PACKET_SPACING - delay);
	}

	dmx_set_port_direction(DMX_PORT_DIRECTION_OUTP, false);
	rdm_send_data(rdm_data, rdm_response->message_length + RDM_MESSAGE_CHECKSUM_SIZE);
	udelay(RDM_RESPONDER_DATA_DIRECTION_DELAY);
	dmx_set_port_direction(DMX_PORT_DIRECTION_INP, true);
}

/**
 * @ingroup rdm
 *
 * @param rdm_data
 */
void rdm_send_respond_message_ack(uint8_t *rdm_data) {
	rdm_send_respond_message(rdm_data, E120_RESPONSE_TYPE_ACK, 0);
}

/**
 * @ingroup rdm
 *
 * @param rdm_data
 * @param reason
 */
void rdm_send_respond_message_nack(uint8_t *rdm_data, const uint16_t reason) {
	rdm_send_respond_message(rdm_data, E120_RESPONSE_TYPE_NACK_REASON, reason);
}

/**
 * @ingroup rdm
 *
 * @param rdm_data
 * @param timer
 */
void rdm_send_respond_message_ack_timer(uint8_t *rdm_data, const uint16_t timer) {
	rdm_send_respond_message(rdm_data, E120_RESPONSE_TYPE_ACK_TIMER, timer);
}

