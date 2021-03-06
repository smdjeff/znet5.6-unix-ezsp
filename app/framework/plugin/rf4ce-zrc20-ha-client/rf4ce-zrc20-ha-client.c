// Copyright 2014 Silicon Laboratories, Inc.

#include "af.h"
#include "app/framework/plugin/rf4ce-profile/rf4ce-profile.h"
#include "app/framework/plugin/rf4ce-zrc20/rf4ce-zrc20.h"
#include "rf4ce-zrc20-ha-client.h"

static int8u haExternalBufferNumOfItems;
static int8u haExternalBuffer[16];

typedef struct {
  int8u haInstanceId;
  EmberAfRf4ceZrcHomeAutomationSupported haSupported;
} HaSupported;

#if defined(EMBER_AF_RF4CE_ZRC_HOME_AUTOMATION_SUPPORTED) || defined(EMBER_SCRIPTED_TEST)
static const HaSupported aplHomeAutomationSupported[] = EMBER_AF_RF4CE_ZRC_HOME_AUTOMATION_SUPPORTED;
#endif // EMBER_AF_RF4CE_ZRC_HOME_AUTOMATION_SUPPORTED

static int8u stringLength(const int8u *buffer);
static int16u longStringLength(const int8u *buffer);

EmberStatus emberAfPluginRf4ceZrc20GetHomeAutomationSupportedCallback(int8u pairingIndex,
                                                                      int8u haInstanceId,
                                                                      EmberAfRf4ceZrcHomeAutomationSupported *haSupported)
{
#if defined(EMBER_AF_RF4CE_ZRC_HOME_AUTOMATION_SUPPORTED) || defined(EMBER_SCRIPTED_TEST)
  /* Pairing index and HA instance should be valid, so we don't check them. */
  int8u i;
  for (i = 0; i < COUNTOF(aplHomeAutomationSupported); i++) {
    if (aplHomeAutomationSupported[i].haInstanceId == haInstanceId) {
      MEMCOPY((int8u*)haSupported,
              (int8u*)&aplHomeAutomationSupported[i].haSupported,
              sizeof(EmberAfRf4ceZrcHomeAutomationSupported));
      return EMBER_SUCCESS;
    }
  }
#endif // EMBER_AF_RF4CE_ZRC_HOME_AUTOMATION_SUPPORTED

  return EMBER_INVALID_CALL;
}

/* Example on how to construct and send HA action:
 *
 * pairingIndex = 1;
 * instanceId = 2;
 * emberAfRf4ceZrcHaFillCommandOnOffClusterOff();
 * emberAfRf4ceZrcHaSend(pairingIndex, instanceId);
 *
 */


// Method that fills in the buffer.
// It returns number of bytes filled
// and 0 on error.
int16u emAfPluginRf4ceZrc20HaFillExternalBuffer(PGM_P format, ...)
{
  int32u value;
  int8u valueLen;
  int8u *data;
  int16u dataLen;
  int8u i;
  int16u bytes = 0;

  va_list argPointer;
  va_start(argPointer, format);

  // Each argument comes in as an integer value, a pointer to a buffer, or a
  // pointer to a buffer followed by an integer length.
  for (i = 0; format[i] != 0; i++) {
    value = 0;
    valueLen = 0;
    data = 0;
    char cmd = format[i];
    if (cmd <= 's') {
      //  0--9, A--G, L, S, b, l, and s all have a pointer to a buffer.  The
      // length of that buffer is implied by 0--9 and A--G (0 to 16 bytes).
      // For L, S, and b, a separate integer specifies the length.  That length
      // will precede the data in the destination buffer for L and S, which
      // turns them into regular ZigBee strings.  In this case, care must be
      // taken to account for invalid strings, which have length 0xFFFF or 0xFF
      // for L and S respectively.  In the case of invalid strings, the length
      // byte(s) are copied to the destination buffer but the string itself is
      // not.  Finally, l and s are just ZigBee strings and the length of the
      // string data is contained within the buffer itself and the entire
      // buffer is copied as is to the destination buffer.  Note that
      // emberAf(Long)StringLength handles invalid strings, but it does not
      // include the length byte(s) in the total length, so the overhead
      // must be maually accounted for when copying.
      data = (int8u *)va_arg(argPointer, int8u *);
      valueLen = 0;
      if (cmd == 'L' || cmd == 'S' || cmd == 'b') {
        dataLen = (int16u)va_arg(argPointer, int);
        if (cmd == 'L') {
          value = dataLen;
          valueLen = (value == 0xFFFF ? 0 : 2);
        } else if (cmd == 'S') {
          value = (int8u)dataLen;
          valueLen = (value == 0xFF ? 0 : 1);
        }
      } else if (cmd == 'l') {
        dataLen = longStringLength(data) + 2;
      } else if (cmd == 's') {
        dataLen = stringLength(data) + 1;
      } else if ('0' <= cmd && cmd <= '9') {
        dataLen = cmd - '0';
      } else if ('A' <= cmd && cmd <= 'G') {
        dataLen = cmd - 'A' + 10;
      } else {
        emberAfDebugPrintln("ERR: Unknown format '%c'", cmd);
        return 0;
      }
    } else {
      // u, v, x, and w are one-, two-, three-, or four-byte integers.  u and v
      // must be extracted as an int while x and w come through as an int32u.
      // In all cases, the value is copied to the destination buffer in little-
      // endian format.
      dataLen = 0;
      if (cmd == 'u') {
        valueLen = 1;
      } else if (cmd == 'v') {
        valueLen = 2;
      } else if (cmd == 'x') {
        valueLen = 3;
      } else if (cmd == 'w') {
        valueLen = 4;
      } else {
        emberAfDebugPrintln("ERR: Unknown format '%c'", cmd);
        return 0;
      }
      value = (int32u)(valueLen <= 2
                       ? va_arg(argPointer, int)
                       : va_arg(argPointer, int32u));
    }

    // The destination buffer must be at least as large as the running total
    // plus the length of the integer value (if applicable) plus the length of
    // the data (if applicable).
    if (sizeof(haExternalBuffer) < bytes + dataLen + valueLen) {
      emberAfDebugPrintln("ERR: Buffer too short for %d bytes for format '%c'",
                          dataLen + valueLen,
                          cmd);
      return 0;
    }

    // If there is an integer value, add it to destination buffer in little-
    // endian format.
    for (; 0 < valueLen; valueLen--) {
        haExternalBuffer[bytes++] = LOW_BYTE(value);
      value = value >> 8;
    }

    // Finally, if there is data, add it to the destination buffer as is.
    MEMCOPY(haExternalBuffer + bytes, data, dataLen);
    bytes += dataLen;
  }
  va_end(argPointer);

  /* Save number of bytes. */
  haExternalBufferNumOfItems = bytes;

  return bytes;
}

EmberStatus emberAfRf4ceZrcHaSend(int8u pairingIndex,
                                  int8u haInstanceId)
{
  int8u i = 0;
  EmberAfRf4ceZrcActionBank actionBank = EMBER_AF_RF4CE_ZRC_ACTION_BANK_HOME_AUTOMATION_INSTANCE_0
                                          + haInstanceId;
  EmberAfRf4ceZrcActionCode actionCode = haExternalBuffer[i++];

  return emberAfRf4ceZrc20ActionStart(pairingIndex,
                                      actionBank,
                                      actionCode,
                                      EMBER_AF_RF4CE_ZRC_MODIFIER_BIT_NONE, /* actionModifiers */
                                      EMBER_RF4CE_NULL_VENDOR_ID,           /* actionVendorId */
                                      (const int8u*)&haExternalBuffer[i],   /* actionData */
                                      haExternalBufferNumOfItems-i,         /* actionDataLength */
                                      TRUE);                                /* atomic */
}

static int8u stringLength(const int8u *buffer)
{
  // The first byte specifies the length of the string.  A length of 0xFF means
  // the string is invalid and there is no character data.
  return (buffer[0] == 0xFF ? 0 : buffer[0]);
}

static int16u longStringLength(const int8u *buffer)
{
  // The first two bytes specify the length of the long string.  A length of
  // 0xFFFF means the string is invalid and there is no character data.
  int16u length = emberFetchLowHighInt16u(buffer);
  return (length == 0xFFFF ? 0 : length);
}
