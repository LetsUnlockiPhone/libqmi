//
//  etl.cpp
//  qmifuzzer
//
//  Created by Joshua Hill on 4/24/13.
//
//

#include "etl.h"

/* QCDM protocol frames are pseudo Async HDLC frames which end with a 3-byte
 * trailer.  This trailer consists of the 16-bit CRC of the frame plus an ending
 * "async control character" whose value is 0x7E.  The frame *and* the CRC are
 * escaped before adding the trailing control character so that the control
 * character (0x7E) and the escape marker (0x7D) are never seen in the frame.
 */

/* Table of CRCs for each possible byte, with a generator polynomial of 0x8408 */
static const u_int16_t crc_table[256] = {
    0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
    0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
    0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
    0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
    0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
    0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
    0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
    0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
    0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
    0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
    0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
    0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
    0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
    0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
    0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
    0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
    0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
    0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
    0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
    0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
    0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
    0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
    0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
    0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
    0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
    0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
    0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
    0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
    0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
    0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
    0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
    0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

/* Calculate the CRC for a buffer using a seed of 0xffff */
u_int16_t etl_crc16(uint8_t* buffer, uint32_t len) {
    u_int16_t crc = 0xffff;
    
    while (len--)
        crc = crc_table[(crc ^ *buffer++) & 0xff] ^ (crc >> 8);
    return ~crc;
}

int etl_request(uint8_t* input, uint32_t insize, uint8_t** output, uint32_t* outsize) {
    uint32_t size = 0;
    uint8_t* inbuf = NULL;
    uint8_t* outbuf = NULL;
    uint8_t* buffer = NULL;
    uint16_t crc = 0;
    
    inbuf = (uint8_t*) malloc(insize+2); // plus 2 for the crc
    memset(inbuf, '\0', insize+2);
    memcpy(inbuf, input, insize); // copy the original data into our buffer with the crc
    
    crc = etl_crc16 (input, insize); // perform the crc or the original data
    inbuf[insize] = crc & 0xFF; // add first byte of crc
    inbuf[insize+1] = (crc >> 8) & 0xFF; // add second byte of crc
    
    etl_escape(inbuf, insize+2, &outbuf, &size); // escape all control and escape characters
    
    buffer = (uint8_t*) malloc(size+2); // plus 2 for start and end control characters
    memset(buffer, '\0', size+2);
    memcpy(&buffer[1], outbuf, size); // copy our crc'd and escaped characters into final buffer
    
    buffer[0] = 0x7E; // Add our beginning control character
    buffer[size+1] = 0x7E; // Add out ending control character
    
    free(inbuf); // We don't need this anymore
    free(outbuf); // We don't need this anymore
    
    *output = buffer;
    *outsize = size+2;
    return 0;
}

int etl_response(uint8_t* input, uint32_t insize, uint8_t** output, uint32_t* outsize) {
    uint32_t size = 0;
    uint8_t* outbuf = NULL;
    etl_unescape(input, insize, &outbuf, &size);
    
    uint16_t crc = etl_crc16(&outbuf[1], size-4);
    uint16_t chk = *((uint16_t*) &outbuf[size-3]);
    if(crc != chk) {
        //fprintf(stderr, "Invalid CRC!!!\n");
        size += 2;
    }
    uint8_t* buffer = (uint8_t*) malloc(size-4);
    memset(buffer, '\0', size-4);
    memcpy(buffer, &outbuf[1], size-4);
    free(outbuf);
    
    *output = buffer;
    *outsize = size-4;
    return 0;
}

int etl_escape(uint8_t* input, uint32_t insize, uint8_t** output, uint32_t* outsize) {
    int i = 0;
    uint32_t size = 0;
    for(i = 0; i < insize; i++) {
        if(input[i] == 0x7E || input[i] == 0x7D) {
            size++;
        }
        size++;
    }
    
    int o = 0;
    uint8_t* buffer = NULL;
    buffer = (uint8_t*) malloc(size);
    memset(buffer, '\0', size);
    for(i = 0; i < insize; i++) {
        if(input[i] == 0x7E) {
            buffer[o] = 0x7D;
            buffer[o+1] = 0x7E ^ 0x20;
            o++;
        } else if(input[i] == 0x7D) {
            buffer[o] = 0x7D;
            buffer[o+1] = 0x7D ^ 0x20;
            o++;
        } else {
            buffer[o] = input[i];
        }
        o++;
    }
    
    *outsize = size;
    *output = buffer;
    return 0;
}

int etl_unescape(uint8_t* input, uint32_t insize, uint8_t** output, uint32_t* outsize) {
    int i = 0;
    uint32_t size = insize;
    for(i = insize; i >= 0; i--) {
        if(input[i] == 0x7D) size--;
    }
    
    int o = 0;
    uint8_t* buffer = NULL;
    buffer = (uint8_t*) malloc(size);
    memset(buffer, '\0', size);
    for(i = 0; i <= insize; i++) {
        if(input[i] == 0x7D) {
            buffer[o] = input[i+1] ^ 0x20;
            i++;
        } else {
            buffer[o] = input[i];
        }
        o++;
    }
    
    *outsize = size;
    *output = buffer;
    return 0;
}