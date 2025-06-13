#include "ArtNet.h"

#include <endian.h>
#include <memory.h>
#include <netinet/in.h>

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

    color_data_t *packet_colors = reinterpret_cast<color_data_t*>(&packet[ARTNET_HEADER_SIZE]);

    // Copy the data
    int incr = numValues < 0 ? -1 : 1;
    int numChannels = incr * 4 * numValues;
    int valIndex = numValues < 0 ? -numValues - 1 : 0;
    for (int i = 0; i < numChannels; i += 4)
    {
        packet[ARTNET_HEADER_SIZE + i + 0] = htons(channelValues[valIndex].r);
        packet[ARTNET_HEADER_SIZE + i + 1] = htons(channelValues[valIndex].g);
        packet[ARTNET_HEADER_SIZE + i + 2] = htons(channelValues[valIndex].b);
        packet[ARTNET_HEADER_SIZE + i + 3] = htons(channelValues[valIndex].w);
        valIndex += incr;
    }
}