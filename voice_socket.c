/*
 * Ascent MMORPG Server
 * Copyright (C) 2005-2008 Ascent Team <http://www.ascentemu.com/>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "common.h"
#include "network.h"
#include "log.h"
#include "network_handlers.h"
#include "configfile.h"
#include "ascent_packet.h"
#include "ascent_opcodes.h"
#include "voice_channel.h"
#include "voice_channelmgr.h"

 // get specified bit from coded data
int GetBit(const unsigned char* buf, int curBit)
{
	return (buf[curBit >> 3] >> (curBit % 8)) & 1;
}

#define RATES_NUM       6   // number of codec rates
#define SENSE_CLASSES   6   // number of sensitivity classes (A..F)

// frame types
#define FT_SPEECH       0   // active speech
#define FT_DTX_SID      1   // silence insertion descriptor

// retrieve frame information
int GetFrameInfo(               // o: frame size in bits
	short rate,                 // i: encoding rate (0..5)
	short base_rate,            // i: base (core) layer rate,
	const unsigned char buf[2], // i: coded bit frame
	int size,                   // i: coded bit frame size in bytes
	short pLayerBits[RATES_NUM],     // o: number of bits in layers
	short pSenseBits[SENSE_CLASSES], // o: number of bits in
	//    sensitivity classes
	short* nLayers                   // o: number of layers
)
{
	static const short Bits_1[4] = { 0, 9, 9,15 };
	static const short Bits_2[16] = { 43,50,36,31,46,48,40,44,
										47,43,44,45,43,44,47,36 };
	static const short Bits_3[2][6] = { {13,11,23,33,36,31},
									   {25, 0,23,32,36,31}, };
	int FrType;
	int i, nBits = 0;

	if (rate < 0 || rate > 5) {
		return 0; // incorrect stream
	}

	// extract frame type bit if required
	FrType = GetBit(buf, nBits++) ? FT_SPEECH : FT_DTX_SID;

	if ((FrType != FT_DTX_SID && size < 2) || size < 1) {
		return 0; // not enough input data
	}

	for (i = 0; i < SENSE_CLASSES; i++) {
		pSenseBits[i] = 0;

	}

	{
		int cw_0;
		int b[14];

		// extract meaning bits
		for (i = 0; i < 14; i++) {
			b[i] = GetBit(buf, nBits++);
		}

		// parse
		if (FrType == FT_DTX_SID) {
			cw_0 = (b[0] << 0) | (b[1] << 1) | (b[2] << 2) | (b[3] << 3);
			rate = 0;
			pSenseBits[0] = 10 + Bits_2[cw_0];
		}
		else {

			int i, idx;
			int nFlag_1, nFlag_2, cw_1, cw_2;

			nFlag_1 = b[0] + b[2] + b[4] + b[6];
			cw_1 = (cw_1 << 1) | b[0];
			cw_1 = (cw_1 << 1) | b[2];
			cw_1 = (cw_1 << 1) | b[4];
			cw_1 = (cw_1 << 1) | b[6];

			nFlag_2 = b[1] + b[3] + b[5] + b[7];
			cw_2 = (cw_2 << 1) | b[1];
			cw_2 = (cw_2 << 1) | b[3];
			cw_2 = (cw_2 << 1) | b[5];
			cw_2 = (cw_2 << 1) | b[7];

			cw_0 = (b[10] << 0) | (b[11] << 1) | (b[12] << 2) | (b[13] << 3);
			if (base_rate < 0)    base_rate = 0;
			if (base_rate > rate) base_rate = rate;
			idx = base_rate == 0 ? 0 : 1;

			pSenseBits[0] = 15 + Bits_2[cw_0];
			pSenseBits[1] = Bits_1[(cw_1 >> 0) & 0x3] +
				Bits_1[(cw_1 >> 2) & 0x3];
			pSenseBits[2] = nFlag_1 * 5;
			pSenseBits[3] = nFlag_2 * 30;
			pSenseBits[5] = (4 - nFlag_2) * (Bits_3[idx][0]);

			for (i = 1; i < rate + 1; i++) {
				pLayerBits[i] = 4 * Bits_3[idx][i];
			}

		}

		pLayerBits[0] = 0;
		for (i = 0; i < SENSE_CLASSES; i++) {
			pLayerBits[0] += pSenseBits[i];
		}

		*nLayers = rate + 1;
	}

	{
		// count total frame size
		int payloadBitCount = 0;
		for (i = 0; i < *nLayers; i++) {
			payloadBitCount += pLayerBits[i];
		}
		return payloadBitCount;
	}
}

void dumphex(char* buf, int len)
{
	int i;
	for (i = 0; i < len; ++i)
		printf("%.2X ", ((unsigned char*)buf)[i]);
	printf("\n");
}

int voicechat_client_socket_read_handler(network_socket *s, int act)
{
	struct sockaddr_in read_address;
	char buffer[4096] = {0};			// people needing a bigger buffer, again can go to hell
	int bytes;

	uint16 channel_id;
	uint8 user_id;
	uint32 header;
	uint16 frameNumber;
	int i;

	voice_channel * chn;
	voice_channel_member * memb;

    // nothing to do
    if (get_channel_count() == 0 || get_server_count() == 0)
    {
        return 0;
    }

	if( act == IOEVENT_ERROR )
	{
		log_write(ERROR, "UDP Socket read an error!");
		return 0;
	}

	if( (bytes = network_read_data(s, buffer, 4096, (struct sockaddr*)&read_address)) < 0 )
	{
		log_write(ERROR, "UDP socket read error bytes!");
		return 0;
	}

	log_write(DEBUG, "udp socket got %d bytes from address %s\n", bytes, inet_ntoa(read_address.sin_addr));
	channel_id = *(uint16*)(&buffer[5]);
	user_id = buffer[4];
	//dumphex(buffer, bytes);

	// todo decrypt
	header = *(uint32*)(&buffer[0]);
	//frameNumber = *(uint16*)(&buffer[6]);
	//log_write(DEBUG, "voice frame #%u\n", (int)frameNumber);

	chn = voice_channel_get((int)channel_id);
	log_write(DEBUG, "channel %u userid %u\n", (int)channel_id, (int)user_id);
	if( chn == NULL )
	{
		log_write(DEBUG, "%s udp client sent invalid voice channel.", inet_ntoa(read_address.sin_addr));
		return 0;
	}

	if( user_id >= chn->member_slots )
	{
		log_write(DEBUG, "%s udp client sent out of range user id.", inet_ntoa(read_address.sin_addr));
		return 0;
	}

	memb = &chn->members[user_id];

	// client initial packet check
	if( bytes == 7 )
	{
		if( memb->voiced && memcmp(&memb->client_address, &read_address, sizeof(struct sockaddr)) != 0 )
		{
			log_write(DEBUG, "%s udp client is sending a different read address. should be %s", inet_ntoa(read_address.sin_addr), inet_ntoa(memb->client_address.sin_addr));
		}

        if (memb->enabled == 0)
            return 0;

		memcpy(&memb->client_address, &read_address, sizeof(struct sockaddr));
		log_write(DEBUG, "client %u address set to %s\n", (int)user_id, inet_ntoa(chn->members[user_id].client_address.sin_addr));
	}
	else
	{
		if (memb->muted == 1 || memb->enabled == 0)
			return 0;

		// distribute the packet
		for( i = 0; i < chn->member_slots; ++i )
		{
			if( i == user_id || chn->members[i].enabled == 0 || chn->members[i].voiced == 0)
				continue;			// don't send to yourself :P
			
			if( network_write_data( s, buffer, bytes, (struct sockaddr*)&chn->members[i].client_address ) < 0 )
			{
				log_write(ERROR, "network_write_data to UDP client %s failed.", inet_ntoa(chn->members[i].client_address.sin_addr));
			}
		}
	}
	
	return 0;
}
