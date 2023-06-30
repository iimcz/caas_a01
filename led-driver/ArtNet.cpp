#include "ArtNet.h"
#include "LedDriver.h"

#include <endian.h>
#include <memory.h>
#include <iostream>

void constructArtNetPacket(uint8_t *packet, const color_t *channelValues, int32_t numValues, uint8_t universe, uint8_t net)
{
    // Clear the packet array
    memset(packet, 0, ARTNET_FULL_PACKET_SIZE);

    //static uint8_t sequence = 0;
    const uint32_t dataLength = 512;

    // Construct the header
    memcpy(packet, ARTNET_ID, ARTNET_ID_SIZE);
    *((uint16_t *)&packet[ARTNET_OPCODE_OFFSET]) = htole16(ARTNET_OPCODE);
    *((uint16_t *)&packet[ARTNET_VERSION_OFFSET]) = htobe16(ARTNET_PROTOCOL_VERSION);
    packet[ARTNET_SEQUENCE_OFFSET] = 0;
    packet[ARTNET_PHYSICAL_OFFSET] = 0;
    packet[ARTNET_UNIVERSE_OFFSET] = universe;
    packet[ARTNET_NET_OFFSET] = net;
    packet[ARTNET_LENGTH_HI_OFFSET] = (dataLength >> 8) & 0xFF;
    packet[ARTNET_LENGTH_LO_OFFSET] = dataLength & 0xFF;

    // Copy the data
    if (numValues < 0)
    {
        int numChannels = -4 * numValues;
        int valIndex = -numValues - 1;
        for (int i = 0; i < numChannels; i += 4)
        {
            packet[ARTNET_HEADER_SIZE + i + 0] = channelValues[valIndex].r;
            packet[ARTNET_HEADER_SIZE + i + 1] = channelValues[valIndex].g;
            packet[ARTNET_HEADER_SIZE + i + 2] = channelValues[valIndex].b;
            packet[ARTNET_HEADER_SIZE + i + 3] = channelValues[valIndex].w;
            valIndex--;
        }
    }
    else
    {
        int numChannels = 4 * numValues;
        int valIndex = 0;
        for (int i = 0; i < numChannels; i += 4)
        {
            packet[ARTNET_HEADER_SIZE + i + 0] = channelValues[valIndex].r;
            packet[ARTNET_HEADER_SIZE + i + 1] = channelValues[valIndex].g;
            packet[ARTNET_HEADER_SIZE + i + 2] = channelValues[valIndex].b;
            packet[ARTNET_HEADER_SIZE + i + 3] = channelValues[valIndex].w;
            valIndex++;
        }
    }
}