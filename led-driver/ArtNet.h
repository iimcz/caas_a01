#ifndef _ARTNET_H
#define _ARTNET_H

#include <stdint.h>
#include <iterator>

// The size of the ArtNet packet ID in bytes
#define ARTNET_ID_SIZE 8

// The ArtNet packet ID as a C string
#define ARTNET_ID "Art-Net"

// The size of the ArtNet header in bytes
#define ARTNET_HEADER_SIZE 18

// The size of the ArtNet packet containing the full 512 values
#define ARTNET_FULL_PACKET_SIZE (ARTNET_HEADER_SIZE + 512)

// The byte offset of the opcode field in the ArtNet header
#define ARTNET_OPCODE_OFFSET 8

// The byte offset of the version field in the ArtNet header
#define ARTNET_VERSION_OFFSET 10

// The byte offset of the sequence field in the ArtNet header
#define ARTNET_SEQUENCE_OFFSET 12

// The byte offset of the physical field in the ArtNet header
#define ARTNET_PHYSICAL_OFFSET 13

// The byte offset of the universe field in the ArtNet header
#define ARTNET_UNIVERSE_OFFSET 14

// The byte offset of the net field in the ArtNet header
#define ARTNET_NET_OFFSET 15

// The byte offset of the lengthHi field in the ArtNet header
#define ARTNET_LENGTH_HI_OFFSET 16

// The byte offset of the lengthLo field in the ArtNet header
#define ARTNET_LENGTH_LO_OFFSET 17

// ArtNet opcode for DMX messages
#define ARTNET_OPCODE 0x5000

// ArtNet protocol version constant
#define ARTNET_PROTOCOL_VERSION 0x000e

// ArtNet default port number
#define ARTNET_PORT 6454

struct color_t;
void constructArtNetPacket(uint8_t *packet, const color_t *channelValues, int32_t numChannels, uint8_t universe, uint8_t net);

#endif // _ARTNET_H