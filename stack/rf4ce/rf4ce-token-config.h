/*
 * File: rf4ce-token-config.h
 * Description: Zigbee RF4CE token definitions used by the stack.
 *
 * Author(s): Maurizio Nanni, maurizio.nanni@silabs.com
 *
 * Copyright 2013 Silicon Laboratories, Inc.                                *80*
 */

#if defined(DEFINETYPES)

typedef struct {
  int8u networkIndex;
  int8u nodeCapabilities;
  int8s radioPower;
  int8u baseChannel;
  int16u localNodeId;
  int16u localPanId;
  int16u vendorId;
  int8u vendorString[7];
} tokTypeStackRf4ceData;

typedef struct {
  int8u securityLinkKey[16];
  int8u destLongId[8];
  int16u sourceNodeId;
  int16u destPanId;
  int16u destNodeId;
  int16u vendorId;
  int8u destProfileIdList[7];
  int8u channel;
  int8u capabilities;
} tokTypeStackRf4cePairingTableEntry;

#endif //DEFINETYPES

#ifdef DEFINETOKENS

DEFINE_BASIC_TOKEN(STACK_RF4CE_DATA,
                   tokTypeStackRf4ceData,
                   {
                     0xFF,                              // network index
                     0xFF,                              // node capabilities
                     0xFF,                              // radio power
                     0xFF,                              // base channel
                     0xFFFF,                            // local node ID
                     0xFFFF,                            // local PAN ID
                     0xFFFF,                            // vendor ID
                     {0xFF, 0xFF, 0xFF, 0xFF,           // vendor string
                      0xFF, 0xFF, 0xFF},
                   })

DEFINE_INDEXED_TOKEN(STACK_RF4CE_PAIRING_TABLE,
                     tokTypeStackRf4cePairingTableEntry,
                     EMBER_RF4CE_PAIRING_TABLE_TOKEN_SIZE,
                     {
                       {0xFF, 0xFF, 0xFF, 0xFF,         // security link key
                        0xFF, 0xFF, 0xFF, 0xFF,
                        0xFF, 0xFF, 0xFF, 0xFF,
                        0xFF, 0xFF, 0xFF, 0xFF},
                       {0xFF, 0xFF, 0xFF, 0xFF,         // dest. long ID
                        0xFF, 0xFF, 0xFF, 0xFF},
                       0xFFFF,                          // source node ID
                       0xFFFF,                          // dest. PAN ID
                       0xFFFF,                          // dest. node ID
                       0xFFFF,                          // dest. vendor ID
                       {0xFF, 0xFF, 0xFF, 0xFF,         // dest. profile ID list
                        0xFF, 0xFF, 0xFF},
                       0xFF,                            // channel
                       0xFF                             // dest. capabilities
                     })


#endif
