// defines that are generated by app framework.

#include "../rf4ce-profile/rf4ce-profile-types.h"
#include "../rf4ce-gdp/rf4ce-gdp-types.h"
#include "rf4ce-zrc20-types.h"

#define EMBER_AF_RF4CE_NODE_TYPE_TARGET

#define EMBER_AF_PLUGIN_RF4CE_ZRC20_ACTION_REPEAT_TRIGGER_INTERVAL_MS       100
#define EMBER_AF_PLUGIN_RF4CE_ZRC20_ACTION_REPEAT_WAIT_TIME_MS              200

#define EMBER_AF_RF4CE_ZRC_ACTION_BANKS_RX                                     \
{0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,                               \
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,                               \
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,                               \
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}
#define EMBER_AF_RF4CE_ZRC_ACTION_BANKS_TX                                     \
{0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,                               \
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,                               \
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,                               \
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}

#define EMBER_AF_RF4CE_ZRC_ACTION_CODES_RX_COUNT                  2
#define EMBER_AF_RF4CE_ZRC_ACTION_CODES_RX                                     \
{{TRUE, 0, {0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,                    \
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,                    \
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,                    \
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},                  \
 {TRUE, 7, {0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,                    \
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,                    \
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,                    \
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}}

#define EMBER_AF_RF4CE_ZRC_ACTION_CODES_TX_COUNT                  2
#define EMBER_AF_RF4CE_ZRC_ACTION_CODES_TX                                     \
{{TRUE, 0, {0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,                    \
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,                    \
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,                    \
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},                  \
 {TRUE, 7, {0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,                    \
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,                    \
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,                    \
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}}}

#define EMBER_AF_RF4CE_ZRC_IRDB_VENDOR_IDS                    {0x1234, 0x5678}
#define EMBER_AF_RF4CE_ZRC_IRDB_VENDOR_ID_COUNT              2

#define EMBER_AF_PLUGIN_RF4CE_ZRC20_REMOTE_IRDB_VENDORS_SUPPORTED_TABLE_SIZE   5

#define EMBER_AF_PLUGIN_RF4CE_ZRC20_REMOTE_ACTION_CODES_TABLE_SIZE             3

#define EMBER_AF_PLUGIN_RF4CE_ZRC20_ACTION_MAPPING_SERVER_MAPPABLE_ACTIONS_TABLE_SIZE 5
#define EMBER_AF_PLUGIN_RF4CE_ZRC20_ACTION_MAPPING_SERVER_ACTION_MAPPINGS_HEAP_SIZE 128
#define EMBER_AF_PLUGIN_RF4CE_ZRC20_ACTION_MAPPING_SERVER_ACTION_REMAP_TABLE_SIZE 8

#define EMBER_AF_RF4CE_ZRC_MAPPABLE_ACTIONS { \
  {0x00, 0x00, 0x00}, \
  {0x00, 0x00, 0x01}, \
  {0x00, 0x00, 0x02}, \
  {0x00, 0x00, 0x03}, \
  {0x00, 0x00, 0x04}, \
  {0x00, 0x00, 0x05}, \
  {0x00, 0x00, 0x06}, \
  {0x00, 0x00, 0x07}, \
  {0x00, 0x00, 0x08}, \
  {0x00, 0x00, 0x09}, \
  {0x00, 0x00, 0x0A}, \
  {0x00, 0x00, 0x0B}, \
  {0x00, 0x00, 0x0C}, \
  {0x00, 0x00, 0x0D}, \
  {0x00, 0x00, 0x0E}, \
  {0x00, 0x00, 0x0F}, \
}
#define EMBER_AF_RF4CE_ZRC_MAPPABLE_ACTION_COUNT 16

#define EMBER_AF_PLUGIN_RF4CE_ZRC20_ACTION_MAPPING_CLIENT_ACTION_MAPPING_HEAP_SIZE    1024

#define EMBER_AF_PLUGIN_RF4CE_ZRC20_ACTION_BANKS_VERSION    APL_ZRC_ACTION_BANKS_VERSION_DEFAULT

#define EMBER_AF_PLUGIN_RF4CE_ZRC20_MAX_INCOMING_ACTION_RECORDS 10
#define EMBER_AF_PLUGIN_RF4CE_ZRC20_MAX_OUTGOING_ACTION_RECORDS 5

void emberAfPluginRf4ceZrc20LegacyCommandDiscoveryCompleteCallback(EmberStatus status,
                                                                   const EmberAfRf4ceZrcCommandsSupported *commandsSupported);

void emberAfPluginRf4ceZrc20ActionCallback(const EmberAfRf4ceZrcActionRecord *record);

void emberAfPluginRf4ceZrc20HaActionCallback(const EmberAfRf4ceZrcActionRecord *record);

boolean emberAfPluginRf4ceProfileGdpDiscoveryRequestCallback(const EmberEUI64 ieeeAddr,
                                                             int8u nodeCapabilities,
                                                             const EmberRf4ceVendorInfo * vendorInfo,
                                                             const EmberRf4ceApplicationInfo * appInfo,
                                                             int8u searchDevType,
                                                             int8u rxLinkQuality);

boolean emberAfPluginRf4ceProfileGdpDiscoveryResponseCallback(boolean atCapacity,
                                                              int8u channel,
                                                              EmberPanId panId,
                                                              const EmberEUI64 ieeeAddr,
                                                              int8u nodeCapabilities,
                                                              const EmberRf4ceVendorInfo * vendorInfo,
                                                              const EmberRf4ceApplicationInfo * appInfo,
                                                              int8u rxLinkQuality,
                                                              int8u discRequestLqi);

void emberAfPluginRf4ceProfileGdpDiscoveryCompleteCallback(EmberStatus status);

void emberAfPluginRf4ceProfileGdpAutoDiscoveryResponseCompleteCallback(EmberStatus status,
                                                                       const EmberEUI64 srcIeeeAddr,
                                                                       int8u nodeCapabilities,
                                                                       const EmberRf4ceVendorInfo * vendorInfo,
                                                                       const EmberRf4ceApplicationInfo * appInfo,
                                                                       int8u searchDevType);

boolean emberAfPluginRf4ceProfileGdpPairRequestCallback(EmberStatus status,
                                                        int8u pairingIndex,
                                                        const EmberEUI64 sourceIeeeAddr,
                                                        int8u nodeCapabilities,
                                                        const EmberRf4ceVendorInfo * vendorInfo,
                                                        const EmberRf4ceApplicationInfo * appInfo,
                                                        int8u keyExchangeTransferCount);

void emberAfPluginRf4ceProfileGdpPairCompleteCallback(EmberStatus status,
                                                      int8u pairingIndex,
                                                      const EmberRf4ceVendorInfo * vendorInfo,
                                                      const EmberRf4ceApplicationInfo * appInfo);

void emberAfPluginRf4ceGdpZrc20BindingCompleteCallback(EmberAfRf4ceGdpBindingStatus status,
                                                       int8u pairingIndex);

void emberAfPluginRf4ceZrc20ActionMappingsNegotiationCompleteCallback(EmberStatus status);

void emberAfPluginRf4ceZrc20IncomingMappableActionCallback(int8u pairingIndex,
                                                           int16u entryIndex,
                                                           EmberAfRf4ceZrcMappableAction *mappableAction);

int16u emberAfPluginRf4ceZrc20GetMappableActionCountCallback(int8u pairingIndex);

EmberStatus emberAfPluginRf4ceZrc20GetMappableActionCallback(int8u pairingIndex,
                                                             int16u entryIndex,
                                                             EmberAfRf4ceZrcMappableAction *mappableAction);

void emberAfPluginRf4ceZrc20IncomingActionMappingCallback(int8u pairingIndex,
                                                          int16u entryIndex,
                                                          EmberAfRf4ceZrcActionMapping *actionMapping);

EmberStatus emberAfPluginRf4ceZrc20GetActionMappingCallback(int8u pairingIndex,
                                                            int16u entryIndex,
                                                            EmberAfRf4ceZrcActionMapping *actionMapping);

void emberAfPluginRf4ceZrc20HomeAutomationSupportedAnnouncementCompleteCallback(EmberStatus status);

void emberAfPluginRf4ceZrc20IncomingHomeAutomationSupportedCallback(int8u pairingIndex,
                                                                    int16u entryIndex,
                                                                    EmberAfRf4ceZrcHomeAutomationSupported *haSupported);

EmberStatus emberAfPluginRf4ceZrc20GetHomeAutomationSupportedCallback(int8u pairingIndex,
                                                                      int16u entryIndex,
                                                                      EmberAfRf4ceZrcHomeAutomationSupported *haSupported);

int16u emberAfPluginRf4ceZrc20GetHomeAutomationSupportedCountCallback(int8u pairingIndex);

EmberAfRf4ceGdpAttributeStatus emberAfPluginRf4ceZrc20GetHomeAutomationAttributeCallback(int8u pairingIndex,
                                                                                         int8u haInstanceId,
                                                                                         int8u haAttributeId,
                                                                                         EmberAfRf4ceZrcHomeAutomationAttribute *haAttribute);

void emberAfPluginRf4ceZrc20PullHomeAutomationAttributeCompleteCallback(EmberAfRf4ceGdpAttributeStatus responseStatus,
                                                                        EmberAfRf4ceZrcHomeAutomationAttribute *haAttribute);
