// *******************************************************************
// * key-establishment.c
// *
// *
// * Copyright 2008 by Ember Corporation. All rights reserved.              *80*
// *******************************************************************

#include "app/framework/include/af.h"
#include "app/framework/security/crypto-state.h"
#include "app/framework/util/af-main.h"
#include "app/framework/util/common.h"
#include "hal/micro/random.h"

#include "app/framework/plugin/test-harness/test-harness.h"

#if !defined(EZSP_HOST)
  #include "stack/include/cbke-crypto-engine.h"
#endif
#include "key-establishment.h"
#include "key-establishment-storage.h"

//------------------------------------------------------------------------------
// Globals


//#define FORCE_283K1_BEHAVIOR

#define LAST_KEY_ESTABLISH_EVENT INITIATOR_RECEIVED_CONFIRM_KEY

// These are initialized by the init routine.
static KeyEstablishEvent lastEvent;

typedef struct {
  EmberEUI64 eui64;
  EmberPanId panId;
} InterPan;

typedef struct {
  EmberNodeId nodeId;
  int8u       endpoint;
} IntraPan;

typedef union {
  InterPan interPan;
  IntraPan intraPan;
} PanData;

typedef struct {
  PanData pan;
  boolean isInitiator;
  boolean isIntraPan;
  int8u sequenceNumber;
} KeyEstablishmentPartner;

static KeyEstablishmentPartner partner;

#define KEY_ESTABLISHMENT_TIMEOUT_BASE_SECONDS 10

//This is mask for the Valid Key Usage byte.
#define KEY_USAGE_BYTE 36
#define KEY_USAGE_BYTE_VALID_VALUE 0x08
#define TYPE_BYTE 0
#define TYPE_BYTE_VALID_VALUE 0x00
#define CURVE_BYTE 9
#define CURVE_BYTE_VALID_VALUE 0x0D
#define HASH_BYTE 10
#define HASH_BYTE_VALID_VALUE 0x08 
// This is the delay time the local device will report when it sends the 
// Terminate message
#define BACK_OFF_TIME_REPORTED_TO_PARTNER    30      // in seconds

// This is the delay time the local device will use when it encounters
// a failure that doesn't receive an explicit delay from the partner.
#define INTERNAL_ERROR_BACK_OFF_TIME         10      // in seconds

#if !defined(EMBER_AF_CUSTOM_KE_EPHEMERAL_DATA_GENERATE_TIME_SECONDS)
  #define EMBER_AF_CUSTOM_KE_EPHEMERAL_DATA_GENERATE_TIME_SECONDS \
    DEFAULT_EPHEMERAL_DATA_GENERATE_TIME_SECONDS
#endif

#if !defined(EMBER_AF_CUSTOM_KE_GENERATE_SHARED_SECRET_TIME_SECONDS)
  #define EMBER_AF_CUSTOM_KE_GENERATE_SHARED_SECRET_TIME_SECONDS \
    DEFAULT_GENERATE_SHARED_SECRET_TIME_SECONDS
#endif

#define EPHEMERAL_DATA_GENERATE_TIME_SECONDS   EMBER_AF_CUSTOM_KE_EPHEMERAL_DATA_GENERATE_TIME_SECONDS
#define GENERATE_SHARED_SECRET_TIME_SECONDS    EMBER_AF_CUSTOM_KE_GENERATE_SHARED_SECRET_TIME_SECONDS

// For purposes of testing timeouts, we allow redefining the advertised
// ephemeral data generate time separately from the actual 
// ephemeral data generate time.
#if !defined(EM_AF_ADVERTISED_EPHEMERAL_DATA_GEN_TIME_SECONDS)
  #define EM_AF_ADVERTISED_EPHEMERAL_DATA_GEN_TIME_SECONDS EPHEMERAL_DATA_GENERATE_TIME_SECONDS
#endif

// Offsets within the payload of the Initiate Key request/response messages
#define EPHEMERAL_DATA_TIME_OFFSET 2
#define CONFIRM_KEY_TIME_OFFSET    3

EmberAfCbkeKeyEstablishmentSuite emAfAvailableCbkeSuite = EMBER_AF_CBKE_KEY_ESTABLISHMENT_SUITE_163K1;
EmberAfCbkeKeyEstablishmentSuite emAfCurrentCbkeSuite   = EMBER_AF_CBKE_KEY_ESTABLISHMENT_SUITE_163K1;
EmberAfCbkeKeyEstablishmentSuite emUseTestHarnessSuite  = EMBER_AF_INVALID_KEY_ESTABLISHMENT_SUITE;

#if defined EMBER_TEST
KeyEstablishEvent timeoutState = NO_KEY_ESTABLISHMENT_EVENT;
#endif

#define CBKE_163K1_SUITE_OFFSET 0
#define CBKE_283K1_SUITE_OFFSET 4

static int8u cbkeSuiteOffset = CBKE_163K1_SUITE_OFFSET;
//The SMAC and Terminate sizes are duplicated to be able to easily select
//suite based sizes for the 163k and the 283k curve.
// This KeyEstablishMessage enum to message size.
PGM int8u emAfKeyEstablishMessageToDataSize[] = {
  EM_AF_KE_INITIATE_SIZE,
  EM_AF_KE_EPHEMERAL_SIZE,
  EM_AF_KE_SMAC_SIZE,
  EM_AF_KE_TERMINATE_SIZE,
  EM_AF_KE_INITIATE_SIZE_283K1,
  EM_AF_KE_EPHEMERAL_SIZE_283K1,
  EM_AF_KE_SMAC_SIZE,
  EM_AF_KE_TERMINATE_SIZE,
};

// This relates AppNotifyMesage enum to EmberAfAmiKeyEstablishmentStatus codes.
// A value of EMBER_ZCL_AMI_KEY_ESTABLISHMENT_STATUS_SUCCESS means no terminate
// message is sent.
static const EmberAfAmiKeyEstablishmentStatus appNotifyMessageToKeyEstablishStatus[] = {
  // Success!
  EMBER_ZCL_AMI_KEY_ESTABLISHMENT_STATUS_SUCCESS,            // NO_APP_MESSAGE
  EMBER_ZCL_AMI_KEY_ESTABLISHMENT_STATUS_SUCCESS,            // RECEIVED_PARTNER_CERTIFICATE
  EMBER_ZCL_AMI_KEY_ESTABLISHMENT_STATUS_SUCCESS,            // GENERATING_EPHEMERAL_KEYS
  EMBER_ZCL_AMI_KEY_ESTABLISHMENT_STATUS_SUCCESS,            // GENERATING_SHARED_SECRET
  EMBER_ZCL_AMI_KEY_ESTABLISHMENT_STATUS_SUCCESS,            // KEY_GENERATION_DONE
  EMBER_ZCL_AMI_KEY_ESTABLISHMENT_STATUS_SUCCESS,            // GENERATE_SHARED_SECRET_DONE
  EMBER_ZCL_AMI_KEY_ESTABLISHMENT_STATUS_SUCCESS,            // LINK_KEY_ESTABLISHED

  // Failures
  EMBER_ZCL_AMI_KEY_ESTABLISHMENT_STATUS_NO_RESOURCES,       // NO_LOCAL_RESOURCES
  EMBER_ZCL_AMI_KEY_ESTABLISHMENT_STATUS_SUCCESS,            // PARTNER_NO_RESOURCES
  EMBER_ZCL_AMI_KEY_ESTABLISHMENT_STATUS_SUCCESS,            // TIMEOUT_OCCURRED
  EMBER_ZCL_AMI_KEY_ESTABLISHMENT_STATUS_SUCCESS,            // INVALID_APP_COMMAND
  EMBER_ZCL_AMI_KEY_ESTABLISHMENT_STATUS_SUCCESS,            // MESSAGE_SEND_FAILURE
  EMBER_ZCL_AMI_KEY_ESTABLISHMENT_STATUS_SUCCESS,            // PARTNER_SENT_TERMINATE
  EMBER_ZCL_AMI_KEY_ESTABLISHMENT_STATUS_BAD_MESSAGE,        // INVALID_PARTNER_MESSAGE
  EMBER_ZCL_AMI_KEY_ESTABLISHMENT_STATUS_BAD_MESSAGE,        // PARTNER_SENT_DEFAULT_RESPONSE_ERROR

  EMBER_ZCL_AMI_KEY_ESTABLISHMENT_STATUS_UNKNOWN_ISSUER,     // BAD_CERTIFICATE_ISSUER
  EMBER_ZCL_AMI_KEY_ESTABLISHMENT_STATUS_BAD_KEY_CONFIRM,    // KEY_CONFIRM_FAILURE
  EMBER_ZCL_AMI_KEY_ESTABLISHMENT_STATUS_UNSUPPORTED_SUITE,  // BAD_KEY_ESTABLISHMENT_SUITE
  EMBER_ZCL_AMI_KEY_ESTABLISHMENT_STATUS_NO_RESOURCES,       // KEY_TABLE_FULL
  EMBER_ZCL_AMI_KEY_ESTABLISHMENT_STATUS_NO_RESOURCES,       // NO_ESTABLISHMENT_ALLOWED
  EMBER_ZCL_AMI_KEY_ESTABLISHMENT_STATUS_INVALID_KEY_USAGE,  // INVALID_CERTIFICATE_KEY_USAGE
};

#define sendEphemeralData(publicKey) \
  (sendNextKeyEstablishMessage(ZCL_EPHEMERAL_DATA_RESPONSE_COMMAND_ID, \
                               (publicKey)))
#define sendConfirmKey(smac) \
  (sendNextKeyEstablishMessage(ZCL_CONFIRM_KEY_DATA_REQUEST_COMMAND_ID, \
                               (smac)))

// This relates the KeyEstablishEvent enum to the timeouts for each event.
// We will setup the values when we receive the first message.
// Values in seconds.  The timeout values passed in the protocol are 8-bit
// but that means when we add our fudge factor it can overflow beyond
// 255.  So we make these 16-bit values to prevent problems.
static int16u eventTimeoutsSec[LAST_KEY_ESTABLISH_EVENT];

// This is the last endpoint that was initialized for key establishment
// it is the one we use for all our event scheduling
static int8u keyEstablishmentEndpoint = 0xFF;

// We record the sequence numbers of our partner device's messages so
// we can filter out dupes.  3 messages can be received during normal
// KE, plus 1 for a possible Terminate message.
#define NUM_SEQ_NUMBER 4
static int8u apsSequenceNumbersReceived = 0;
static int8u apsSequenceNumbers[NUM_SEQ_NUMBER];

//Offsets for the ECC 163k and 283k curve. We assume this to default to
//the ECC 163k curve.
#define CERT_SUBJECT_OFFSET 22
#define CERT_ISSUER_OFFSET  (CERT_SUBJECT_OFFSET + 8)
#define CERT_SUBJECT_OFFSET_283K1 28
#define CERT_ISSUER_OFFSET_283K1  11
#define CERT_ISSUER_SIZE    8

static int8u certSubjectOffset = CERT_SUBJECT_OFFSET;
static int8u certIssuerOffset  = CERT_ISSUER_OFFSET;

#if defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_KEY_ESTABLISHMENT_CLUSTER)
static PGM_P terminateStatus[] = TERMINATE_STATUS_STRINGS;
#endif

// Over the air message lengths for Initiate Key Establishment Request and Response
// certificate + keyEstablishmentSuite + ephemeralDataGenerateTime + confirmKeyGenerateTime
# define INITIATE_KEY_ESTABLISHMENT_LENGTH_163K1 EMBER_CERTIFICATE_SIZE + 2 + 1 + 1
# define INITIATE_KEY_ESTABLISHMENT_LENGTH_283K1 EMBER_CERTIFICATE_283K1_SIZE + 2 + 1 + 1

//------------------------------------------------------------------------------
// Forward declarations
static boolean checkMalformed283k1Command(boolean isCertificate);
static void sendMalformedMessageTerminateCommand(EmberAfClusterCommand *cmd);
static void checkSupportedCurve(boolean initiateAsServer);
static boolean checkKeyUsage(int8u keyUsageByte);
static boolean checkKeyTable(int8u *bigEndianEui64);
static void keyEstablishStateMachine(KeyEstablishEvent newEvent,
                                     int8u *data1,
                                     int8u *data2);
static void clearKeyEstablishmentState(void);
static boolean askApplicationWithDelay(EmberAfKeyEstablishmentNotifyMessage message, 
                                       int8u delayInSec);
#define askApplication(message) askApplicationWithDelay((message), 0)
static void sendKeyEstablishMessage(KeyEstablishMessage message);
static void sendTerminateMessage(EmberAfAmiKeyEstablishmentStatus status,
                                 int8u backOffTime);
static EmberAfKeyEstablishmentNotifyMessage verifySmac(EmberSmacData *otherSmac);
static boolean checkRequestedSuite(int8u *suiteLittleEndian);

static void setupEventTimeouts(int8u theirGenerateKeyTimeSeconds,
                               int8u theirConfirmKeyTimeSeconds);
static boolean commandReceivedHandler(EmberAfClusterCommand *cmd);
static void messageSentHandler(EmberOutgoingMessageType type,
                               int16u indexOrDestination,
                               EmberApsFrame *apsFrame,
                               int16u msgLen,
                               int8u *message,
                               EmberStatus status);
static boolean commandIsFromOurPartner(const EmberAfClusterCommand *cmd);
static boolean setPartnerFromCommand(const EmberAfClusterCommand *cmd);
static void writeKeyEstablishmentClusterAttribute(int8u endpoint);
#if defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_KEY_ESTABLISHMENT_CLUSTER)
  static void debugPrintSmac(boolean initiatorSmac, int8u *smac);
  static void debugPrintOtherSmac(boolean received, int8u *smac);
  static void debugPrintCert(boolean initiatorCert, int8u *cert);
  void emPrintData283k1(int8u *buffer, int8u size, unsigned char *type);
  static void debugPrintKey(boolean initiatorKey, int8u *key);
#else
  #define debugPrintSmac(initiatorSmac, smac)
  #define debugPrintOtherSmac(received, smac)
  #define debugPrintCert(initiatorCert, cert)
  #define debugPrintKey(initiatorKey, key)
#endif


#if defined(EMBER_AF_HAS_SECURITY_PROFILE_SE_TEST)
 
  #if defined(EMBER_AF_PLUGIN_TEST_HARNESS)
    // Test code only
    #define NEW_KEY_TABLE_ENTRY_ALLOWED (emKeyEstablishmentPolicyAllowNewKeyEntries)

  #else
    #define NEW_KEY_TABLE_ENTRY_ALLOWED TRUE
  #endif

#elif defined(EMBER_AF_HAS_SECURITY_PROFILE_SE_FULL) 
  #define NEW_KEY_TABLE_ENTRY_ALLOWED FALSE

#else
  #error SE Security not configuration defined
#endif

//------------------------------------------------------------------------------
// Internal Functions
static void checkSupportedCurve(boolean initiateAsServer)
{
  // This function should only get called for Intra-Pan Key Establishment. We currently 
  // don't support the read attributes stage for Inter-Pan Key Establishment.  
  int16u attributeIds;
  attributeIds = ZCL_KEY_ESTABLISHMENT_SUITE_SERVER_ATTRIBUTE_ID;
  emberAfFillCommandGlobalClientToServerReadAttributes(ZCL_KEY_ESTABLISHMENT_CLUSTER_ID,
                                                       &attributeIds,
                                                       sizeof(attributeIds));
  emberAfSetCommandEndpoints(keyEstablishmentEndpoint,partner.pan.intraPan.endpoint);
  emberAfSendCommandUnicast(EMBER_OUTGOING_DIRECT,partner.pan.intraPan.nodeId);
}


static boolean checkValidByteValue283k1(int8u byte,int8u requiredValue,int8u mask)
{
  if(isCbkeKeyEstablishmentSuite283k1()) {
    if((byte & mask)!=requiredValue){
      emberAfKeyEstablishmentClusterPrintln("Error: Invalid byte in cert: byteValue %u requiredValue %u",byte,requiredValue);
      cleanupAndStop(INVALID_CERTIFICATE_KEY_USAGE);
      return FALSE;
    }
  }
  return TRUE;
}

static boolean checkKeyTable(int8u *bigEndianEui64)
{
  EmberEUI64 eui64;
  EmberKeyStruct keyStruct;

  emberReverseMemCopy(eui64, bigEndianEui64, EUI64_SIZE);

  emberAfKeyEstablishmentClusterPrint("Checking key table for IEEE ");
  emberAfKeyEstablishmentClusterDebugExec(emberAfPrintBigEndianEui64(eui64));
  emberAfKeyEstablishmentClusterPrintln("");

  if (emberAfGetNodeId() != EMBER_TRUST_CENTER_NODE_ID) {
    if (emberGetKey(EMBER_TRUST_CENTER_LINK_KEY, &keyStruct) == EMBER_SUCCESS
        && (keyStruct.bitmask & EMBER_KEY_HAS_PARTNER_EUI64)
        && MEMCOMPARE(eui64, keyStruct.partnerEUI64, EUI64_SIZE) == 0) {
      // The key to be updated is our existing TC Link Key, therefore
      // we have room.
      return TRUE;
    } 
    emberAfKeyEstablishmentClusterPrintln("Error: EUI64 of TC does not match its cert.");
    cleanupAndStop(INVALID_PARTNER_MESSAGE);
    return FALSE;

  } else {
    // We are the trust center

    // We either have an existing link key entry that we can update,
    // or there is an empty entry.
    if (emberFindKeyTableEntry(eui64, TRUE) != 0xFF) {
      return TRUE;

    } else if (NEW_KEY_TABLE_ENTRY_ALLOWED != TRUE) {
      emberAfKeyEstablishmentClusterPrintln("Error: Unknown EUI64 trying to perform CBKE.");
      cleanupAndStop(INVALID_PARTNER_MESSAGE);
      return FALSE;

    } else if (0xFF != emberFindKeyTableEntry((int8u*)emberAfNullEui64, 
                                              TRUE)) {
      return TRUE;
    }
  }

  emberAfKeyEstablishmentClusterPrintln("Error: Key table full.");
  cleanupAndStop(KEY_TABLE_FULL);
  return FALSE;
}

// data1 will used for the initiate key metadata, ephemeral public key, the
//   initiator locally calculated smac, or the partner's received smac.
// data2 will used for the partner certificate or for the responder locally
//   calculated smac.

static void keyEstablishStateMachine(KeyEstablishEvent newEvent,
                                     int8u *data1,
                                     int8u *data2)
{
  // The state machine is called by the public APIs, library callbacks, and
  // command handlers.  Most of the processing in this state machine only cares
  // whether the next event is a result of a received command.
  EmberAfClusterCommand *cmd = emberAfCurrentCommand();

  if (cmd != NULL && cmd->type >= EMBER_INCOMING_MULTICAST) {
    emberAfKeyEstablishmentClusterPrintln("Ignoring Broadcast KE message");
    return;
  }

  emberAfKeyEstablishmentClusterPrintln("Key Establish State Machine event:%d",
                                        newEvent);

  if (cmd != NULL && cmd->type >= EMBER_INCOMING_MULTICAST) {
    emberAfKeyEstablishmentClusterPrintln("Ignoring broadcast KE command.");
    return;
  }

  if (cmd != NULL) {
    if (commandIsFromOurPartner(cmd)
        && (partner.isInitiator || lastEvent != NO_KEY_ESTABLISHMENT_EVENT)) {
      // Filter out duplicate APS messages.

      // Edge Case: If the same partner initiates key establishment with us and
      // they happen to use one of the same APS sequence numbers as the last
      // time, this will fail since we assume it is a duplicate message.  The
      // hope is that the partner will retry and it should succeed.
      int8u i;
      for (i = 0; i < apsSequenceNumbersReceived; i++) {
        if (cmd->apsFrame->sequence == apsSequenceNumbers[i]) {
          emberAfKeyEstablishmentClusterPrintln("Got duplicate APS message (seq:%d), dropping!",
                                                cmd->apsFrame->sequence);
          return;
        }
      }
    } else if (!isCbkeKeyEstablishmentSuiteValid() ||(lastEvent != NO_KEY_ESTABLISHMENT_EVENT)) {
      KeyEstablishmentPartner tmpPartner;
      emberAfKeyEstablishmentClusterPrintln("emAfAvailableCbkeSuite %u last event %u",emAfAvailableCbkeSuite, lastEvent);
      // If we have not successfully initialized or we are already in doing
      // key establishment with another partner, tell this new partner to go
      // away and maybe try again later.  The sendTerminateMessage function
      // assumes it is sending to the current partner, so we have to temporarily
      // switch to the new partner, send the terminate, and then switch back to
      // our real partner.
      MEMCOPY(&tmpPartner, &partner, sizeof(KeyEstablishmentPartner));
      emberAfKeyEstablishmentClusterPrintln(isCbkeKeyEstablishmentSuiteValid()
                                            ? "Second Key estabishment not supported, terminating it."
                                            : "Key Est. FAILED INITIALIZATION, terminating");
      if (setPartnerFromCommand(cmd)) {
        partner.sequenceNumber = cmd->seqNum;
        sendTerminateMessage(EMBER_ZCL_AMI_KEY_ESTABLISHMENT_STATUS_NO_RESOURCES,
                             BACK_OFF_TIME_REPORTED_TO_PARTNER);
      }
      MEMMOVE(&partner, &tmpPartner, sizeof(KeyEstablishmentPartner));
      return;
    }

    // If we got here and we're not currently doing key establishment, it means
    // we are starting key establishment with a new partner.  Remember the new
    // partner and clear the previous set of sequence numbers.  We must handle
    // the case where the same partner is initiating key establishment with us
    // that did it last time.  We can't use the above else clause to do that.
    if (lastEvent == NO_KEY_ESTABLISHMENT_EVENT) {
      if (!setPartnerFromCommand(cmd)) {
        return;
      }
      apsSequenceNumbersReceived = 0;
    }

    // Remember the received APS sequence numbers so we can filter duplicates.
    if (partner.isIntraPan) {
      apsSequenceNumbers[apsSequenceNumbersReceived] = cmd->apsFrame->sequence;
      apsSequenceNumbersReceived++;
    }

    // Remember the received ZCL sequence number for the response.
    if (partner.isInitiator) {
      partner.sequenceNumber = cmd->seqNum;
    }
  }

  // If we receive an unexpected message, we terminate and hope the partner
  // tries again.
  //CHECK_SUPPORTED_CURVES is only done by the initiator. Therefore, the server
  // will see a jump of 2 for BEGIN_KEY_ESTABLISHMENT
  if (newEvent != lastEvent + 1)  {
    if(partner.isInitiator && lastEvent == NO_KEY_ESTABLISHMENT_EVENT){
      if(newEvent != BEGIN_KEY_ESTABLISHMENT){
        emberAfKeyEstablishmentClusterPrintln("Got wrong message in the sequence. last event %u, new event %u",lastEvent,newEvent);
        cleanupAndStop(INVALID_PARTNER_MESSAGE);
        return;
      }
    }else{ 
      emberAfKeyEstablishmentClusterPrintln("Got wrong message in the sequence. last event %u, new event %u",lastEvent,newEvent);
      cleanupAndStop(INVALID_PARTNER_MESSAGE);
      return;
    }
  } 

  // Key establishment can only takes place with the trust center.
  if (partner.isIntraPan
      && emberAfGetNodeId() != EMBER_TRUST_CENTER_NODE_ID
      && partner.pan.intraPan.nodeId != EMBER_TRUST_CENTER_NODE_ID) {
    cleanupAndStop(NO_ESTABLISHMENT_ALLOWED);
    return;
  }

#if defined EMBER_TEST
  // Testing only
  if (timeoutState != NO_KEY_ESTABLISHMENT_EVENT && timeoutState <= newEvent) {
    emberAfKeyEstablishmentClusterPrintln("Dropping message to induce timeout in partner.");
    cleanupAndStop(TIMEOUT_OCCURRED);
    return;
  }
#endif

  switch (newEvent) {

  case CHECK_SUPPORTED_CURVES:
    {
      checkSupportedCurve(emberAfGetNodeId() == EMBER_TRUST_CENTER_NODE_ID);
      break;
    }
  // For initiator, send the cert wait for reply.
  // For responder, we received the cert.
  case BEGIN_KEY_ESTABLISHMENT:
    {
      EmberAfKeyEstablishmentNotifyMessage result = NO_APP_MESSAGE;
      emberAfKeyEstablishmentClusterPrintln("Current Suite %u",emAfCurrentCbkeSuite);
      if (partner.isInitiator) {
        if (!checkRequestedSuite(data1)
            || !checkIssuer(data2 + certIssuerOffset)
            || !checkKeyTable(data2 + certSubjectOffset)
            || !checkValidByteValue283k1(*(data2 + KEY_USAGE_BYTE), KEY_USAGE_BYTE_VALID_VALUE,KEY_USAGE_BYTE_VALID_VALUE)
            || !checkValidByteValue283k1(*(data2 + TYPE_BYTE), TYPE_BYTE_VALID_VALUE,0xFF)
            || !checkValidByteValue283k1(*(data2 + CURVE_BYTE), CURVE_BYTE_VALID_VALUE,0xFF)
            || !checkValidByteValue283k1(*(data2 + HASH_BYTE), HASH_BYTE_VALID_VALUE,0xFF)) {
          debugPrintCert(TRUE,data2);
          // For all of the above functions, a terminate message is
          // sent with the appropriate error code.
          return;
        }

        //TO DO: Print 283k1 cert here.
        debugPrintCert(TRUE,data2);

        if (!askApplication(RECEIVED_PARTNER_CERTIFICATE)
            || !storePublicPartnerData(TRUE, // certificate?
                                       data2)) {
          result = NO_LOCAL_RESOURCES;
        } else {
          setupEventTimeouts(data1[EPHEMERAL_DATA_TIME_OFFSET],
                             data1[CONFIRM_KEY_TIME_OFFSET]);
        }
      }
      if (result == NO_APP_MESSAGE) {
        result = sendCertificate();
      }
      if (result != NO_APP_MESSAGE) {
        // We now accept a keyEstablishment attribute of 0 and have
        // to watch out for this additional error case. 
        // In case our device doesn't have the suite that was incorrectly
        // agreed on, we don't want to send a Terminate Key Establishment 
        // message in response to a read attributes response.  
        if(!partner.isInitiator){
          result = BAD_KEY_ESTABLISHMENT_SUITE;
        }
        cleanupAndStop(result);
        return;
      }
      break;
    }

  // For initiator, we received responder cert it is time to generate keys.
  // For responder, we received ephemeral data it is time to generate keys.
  case GENERATE_KEYS:
    if (!partner.isInitiator) {
      if (!checkRequestedSuite(data1)
          || !checkIssuer(data2 + certIssuerOffset)
          || !checkKeyTable(data2 + certSubjectOffset)
          || !checkValidByteValue283k1(*(data2 + KEY_USAGE_BYTE), KEY_USAGE_BYTE_VALID_VALUE,KEY_USAGE_BYTE_VALID_VALUE)
          || !checkValidByteValue283k1(*(data2 + TYPE_BYTE), TYPE_BYTE_VALID_VALUE,0xFF)
          || !checkValidByteValue283k1(*(data2 + CURVE_BYTE), CURVE_BYTE_VALID_VALUE,0xFF)
          || !checkValidByteValue283k1(*(data2 + HASH_BYTE), HASH_BYTE_VALID_VALUE,0xFF)) {
        // For all of the above functions, a terminate message is
        // sent with the appropriate error code.
        debugPrintCert(TRUE,data2);
        return;
      }
      setupEventTimeouts(data1[EPHEMERAL_DATA_TIME_OFFSET],
                         data1[CONFIRM_KEY_TIME_OFFSET]);

      debugPrintCert(FALSE, data2);
    } else {
      debugPrintKey(TRUE, data1);
    }

    if (!askApplication(GENERATING_EPHEMERAL_KEYS)
        || !storePublicPartnerData(!partner.isInitiator, // certificate?
                                   (!partner.isInitiator
                                    ? data2              // partner cert
                                    : data1))            // partner key
        || emGenerateCbkeKeysForCurve() != EMBER_OPERATION_IN_PROGRESS) {
      cleanupAndStop(NO_LOCAL_RESOURCES);
      return;
    }
    emAfSetCryptoOperationInProgress();
    break;

  // For both roles, we are done generating keys.  Send the message.
  case SEND_EPHEMERAL_DATA_MESSAGE:
    if (!askApplication(KEY_GENERATION_DONE)) {
      cleanupAndStop(NO_LOCAL_RESOURCES);
      return;
    }
    sendEphemeralData(data1);
    break;

  // For initiator, we received ephemeral data resp.  Generate shared secret.
  // For responder, we received confirm key request.  Generate shared secret.
  case GENERATE_SHARED_SECRET:
    {
      EmberCertificate283k1Data partnerCert;
      EmberPublicKey283k1Data partnerEphemeralPublicKey;

#if defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_KEY_ESTABLISHMENT_CLUSTER)
      if (!partner.isInitiator) {
        debugPrintKey(FALSE, data1);
      } else {
        debugPrintOtherSmac(TRUE, data1);
      }
#endif
      // For the initiator this is slightly ineffecient because we store
      // the public key but then immediately retrieve it.  However it
      // saves on flash to treat responder and initiator the same.
      if (!askApplication(GENERATING_SHARED_SECRET)
          || (!partner.isInitiator
              ? !storePublicPartnerData(FALSE, data1)
              : !storeSmac((EmberSmacData *)data1))
          || !retrieveAndClearPublicPartnerData(&partnerCert,
                                                &partnerEphemeralPublicKey)
          || (EMBER_OPERATION_IN_PROGRESS
              != emCalculateSmacsForCurve(!partner.isInitiator,
                                          &partnerCert,
                                          &partnerEphemeralPublicKey))) {
        cleanupAndStop(NO_LOCAL_RESOURCES);
        return;
      }
      emAfSetCryptoOperationInProgress();
      break;
    }

  // For both roles, we are done generating shared secret,
  //   send confirm key message.
  case SEND_CONFIRM_KEY_MESSAGE:
    {
      EmberSmacData *initiatorSmac = (EmberSmacData *)data1;
      EmberSmacData *responderSmac = (EmberSmacData *)data2;

      debugPrintSmac(TRUE,  emberSmacContents(initiatorSmac));
      debugPrintSmac(FALSE, emberSmacContents(responderSmac));

      if (!askApplication(GENERATE_SHARED_SECRET_DONE)
          || (!partner.isInitiator && !storeSmac(responderSmac))) {
        cleanupAndStop(NO_LOCAL_RESOURCES);
        return;
      }

      if (partner.isInitiator) {
        EmberAfKeyEstablishmentNotifyMessage result = verifySmac(initiatorSmac);
        if (result != NO_APP_MESSAGE) {
          cleanupAndStop(result);
          return;
        }
      }

      sendConfirmKey(emberSmacContents(!partner.isInitiator
                                       ? initiatorSmac
                                       : responderSmac));

      if (partner.isInitiator) {
        // TODO:  Wait for the APS Ack from the initiator and then store
        // the link key.

        // Success!
        cleanupAndStop(LINK_KEY_ESTABLISHED);
        return;
      }
      break;
    }

  case INITIATOR_RECEIVED_CONFIRM_KEY:
    {
      EmberAfKeyEstablishmentNotifyMessage message = verifySmac((EmberSmacData *)data1);
      debugPrintOtherSmac(TRUE, data1);

      // For both success and failure, stop key establishment.
      cleanupAndStop(message == NO_APP_MESSAGE
                     ? LINK_KEY_ESTABLISHED
                     : message);
      return;
    }

  default:
    // Unknown message, ignore.
    return;
  }

  {
    // We assume that the MILLISECOND_TICKS_PER_SECOND is 1024,
    // and thus a bit-shift by 10 is done to avoid a 32-bit divide operation.
    int32u timeMs = (int32u)(eventTimeoutsSec[newEvent]) << 10;
    emberAfScheduleServerTick(keyEstablishmentEndpoint,
                              ZCL_KEY_ESTABLISHMENT_CLUSTER_ID,
                              timeMs);
  }
  lastEvent = newEvent;
  return;
}

static void clearKeyEstablishmentState(void)
{
  partner.isInitiator = TRUE;
  //emberAfKeyEstablishmentClusterPrintln("partner.isInitiator clearKeyState %u",partner.isInitiator);
  lastEvent = NO_KEY_ESTABLISHMENT_EVENT;
  clearAllTemporaryPublicData();
  emberClearTemporaryDataMaybeStoreLinkKeyForCurve(FALSE);
  emberAfDeactivateServerTick(keyEstablishmentEndpoint,
                              ZCL_KEY_ESTABLISHMENT_CLUSTER_ID);

  // NOTE: When clearing the state, we intentionally retain information about
  // the partner (e.g., node id, APS sequence numbers, etc.).  That information
  // is used to help determine if the a received message is a duplicate of one
  // we already processed.  Because we may receive a retried message after we
  // have completed key establishment, we must remember the partner information.
}

static boolean askApplicationWithDelay(EmberAfKeyEstablishmentNotifyMessage message, 
                                       int8u delayInSec)
{
  PGM_P appNotifyText[] = APP_NOTIFY_MESSAGE_TEXT;

  if (message == NO_APP_MESSAGE) {
    return TRUE;
  }

  emberAfKeyEstablishmentClusterFlush();
  emberAfKeyEstablishmentClusterPrint("%p: %p %p: %p (%d), %p ",
                                      (!partner.isInitiator
                                       ? "Initiator"
                                       : "Responder"),
                                      "Key Establish",
                                      (message >= APP_NOTIFY_ERROR_CODE_START
                                       ? "Error"
                                       : (message == LINK_KEY_ESTABLISHED
                                          ? "Success"
                                          : "Notify")),
                                      appNotifyText[message],
                                      message,
                                      "partner");
  if (partner.isIntraPan) {
    emberAfKeyEstablishmentClusterPrintln("0x%2x", partner.pan.intraPan.nodeId);
    return emberAfKeyEstablishmentCallback(message,
                                           !partner.isInitiator,
                                           partner.pan.intraPan.nodeId,
                                           delayInSec);
  } else {
    emberAfKeyEstablishmentClusterDebugExec(emberAfPrintBigEndianEui64(partner.pan.interPan.eui64));
    emberAfKeyEstablishmentClusterPrintln("");
    return emberAfInterPanKeyEstablishmentCallback(message,
                                                   !partner.isInitiator,
                                                   partner.pan.interPan.panId,
                                                   partner.pan.interPan.eui64,
                                                   delayInSec);
  }
}

static void sendKeyEstablishMessage(KeyEstablishMessage message)
{
  // It is assumed the "appResponseData" has already been filled
  // with data and the "appResponseLength" has been set appropriately
  // including the message overhead that will be filled in by this function.
  int8u *ptr = appResponseData;
  *ptr++ = (ZCL_CLUSTER_SPECIFIC_COMMAND
            | (!partner.isInitiator
               ? ZCL_FRAME_CONTROL_CLIENT_TO_SERVER
               : ZCL_FRAME_CONTROL_SERVER_TO_CLIENT));
  *ptr++ = partner.sequenceNumber;
  *ptr   = message;

  if (!emAfKeyEstablishmentTestHarnessMessageSendCallback(message)) {
    return;
  }

  if (partner.isIntraPan) {
    EmberApsFrame apsFrame;
    apsFrame.clusterId = ZCL_KEY_ESTABLISHMENT_CLUSTER_ID;
    apsFrame.sourceEndpoint = keyEstablishmentEndpoint;
    apsFrame.destinationEndpoint = partner.pan.intraPan.endpoint;
    apsFrame.options = (EMBER_AF_DEFAULT_APS_OPTIONS | EMBER_APS_OPTION_RETRY);
    emberAfSendUnicast(EMBER_OUTGOING_DIRECT,
                       partner.pan.intraPan.nodeId,
                       &apsFrame,
                       appResponseLength,
                       appResponseData);
  } else {
    emberAfSendInterPan(partner.pan.interPan.panId,
                        partner.pan.interPan.eui64,
                        EMBER_NULL_NODE_ID,
                        0, // multicast id - unused
                        ZCL_KEY_ESTABLISHMENT_CLUSTER_ID,
                        SE_PROFILE_ID,
                        appResponseLength,
                        appResponseData);
  }
}

static void sendTerminateMessage(EmberAfAmiKeyEstablishmentStatus status,
                                 int8u backOffTime)
{
  int8u *ptr = appResponseData + EMBER_AF_ZCL_OVERHEAD;
  *ptr++ = status;
  *ptr++ = backOffTime;

  // Since we only support the single key establishment suite, we can
  // get away with casting this into an int8u and simply zeroing the high bits.
  *ptr++ = LOW_BYTE(emAfCurrentCbkeSuite);
  *ptr++ = HIGH_BYTE(emAfCurrentCbkeSuite);

  appResponseLength = (ptr - appResponseData);
  sendKeyEstablishMessage(ZCL_TERMINATE_KEY_ESTABLISHMENT_COMMAND_ID);
}

static EmberAfKeyEstablishmentNotifyMessage verifySmac(EmberSmacData *otherSmac)
{
  // For the initiator, the stored SMAC will be the responder's version
  //   locally calculated.
  // For the responder, the stored SMAC will be the initiator's version
  //   received via the Confirm Key request message.
  EmberSmacData *ptr;
  if (!getSmacPointer(&ptr)) {
    return NO_LOCAL_RESOURCES;
  }

  emberAfKeyEstablishmentClusterPrintln("My local SMAC:");
  debugPrintOtherSmac(FALSE, emberSmacContents(ptr));

  return (MEMCOMPARE(emberSmacContents(ptr), otherSmac, EMBER_SMAC_SIZE) == 0
          ? NO_APP_MESSAGE        // success
          : KEY_CONFIRM_FAILURE);
}

static boolean checkRequestedSuite(int8u *suiteLittleEndian)
{
  int16u requestedSuite;
  requestedSuite = suiteLittleEndian[0] + ((int16u)(suiteLittleEndian[1]) << 8);
  if (requestedSuite == EMBER_AF_INVALID_KEY_ESTABLISHMENT_SUITE) {
    emberAfKeyEstablishmentClusterPrintln("Bad Key Establishment Suite");
    cleanupAndStop(BAD_KEY_ESTABLISHMENT_SUITE);
    return FALSE;
  }
  return TRUE;
}

static void setupEventTimeouts(int8u theirGenerateKeyTimeSeconds,
                               int8u theirConfirmKeyTimeSeconds)
{
  int8u i;
  eventTimeoutsSec[0] = 0;  // NO_KEY_ESTABLISHMENT_EVENT

  for (i = 1; i < LAST_KEY_ESTABLISH_EVENT; i++) {
    eventTimeoutsSec[i] = KEY_ESTABLISHMENT_TIMEOUT_BASE_SECONDS;
  }
  eventTimeoutsSec[BEGIN_KEY_ESTABLISHMENT]     += (!partner.isInitiator
                                                    ? 0
                                                    : theirGenerateKeyTimeSeconds);
  eventTimeoutsSec[GENERATE_KEYS]               += EPHEMERAL_DATA_GENERATE_TIME_SECONDS;
  eventTimeoutsSec[GENERATE_SHARED_SECRET]      += GENERATE_SHARED_SECRET_TIME_SECONDS;
  eventTimeoutsSec[SEND_EPHEMERAL_DATA_MESSAGE] += (!partner.isInitiator
                                                    ? theirGenerateKeyTimeSeconds
                                                    : theirConfirmKeyTimeSeconds);
  // Only initiator needs this timeout while waiting for the event
  // INITIATER_RECEIVED_CONFIRM_KEY_MESSAGE.  Responder ends KE when
  // getting to this event.
  eventTimeoutsSec[SEND_CONFIRM_KEY_MESSAGE]    += theirConfirmKeyTimeSeconds;
}

static void sendMalformedMessageTerminateCommand(EmberAfClusterCommand *cmd)
{
    // Send a TerminateKeyEstablishment message with a BAD_MESSAGE status in
    // response to malformed messages.  Even if the message was from our
    // partner, we do not abort the ongoing key establishment and instead wait
    // for the partner to correct the problem or for a timeout to occur.
    emberAfKeyEstablishmentClusterPrintln("Invalid length for KE command: %d (expected: %d, actual: %d)",
                                          cmd->commandId,
                                          emAfKeyEstablishMessageToDataSize[cmd->commandId + cbkeSuiteOffset],
                                          (cmd->bufLen - cmd->payloadStartIndex));
    emberAfFillExternalBuffer((ZCL_CLUSTER_SPECIFIC_COMMAND
                               | (cmd->direction == ZCL_DIRECTION_CLIENT_TO_SERVER
                                  ? ZCL_FRAME_CONTROL_SERVER_TO_CLIENT
                                  : ZCL_FRAME_CONTROL_CLIENT_TO_SERVER)),
                              ZCL_KEY_ESTABLISHMENT_CLUSTER_ID,
                              ZCL_TERMINATE_KEY_ESTABLISHMENT_COMMAND_ID,
                              "uuv",
                              EMBER_ZCL_AMI_KEY_ESTABLISHMENT_STATUS_BAD_MESSAGE,
                              BACK_OFF_TIME_REPORTED_TO_PARTNER,
                              emAfCurrentCbkeSuite);
    emberAfSendResponse();
}

static boolean commandReceivedHandler(EmberAfClusterCommand *cmd)
{
//  EmberAfStatus status = keyEstablishmentClusterCommandParse(cmd);
  EmberAfStatus status = (cmd->direction == ZCL_DIRECTION_CLIENT_TO_SERVER
                      ? emberAfKeyEstablishmentClusterServerCommandParse(cmd)
                      : emberAfKeyEstablishmentClusterClientCommandParse(cmd));
  if (status == EMBER_ZCL_STATUS_MALFORMED_COMMAND) {
    sendMalformedMessageTerminateCommand(cmd);
  } else if (status != EMBER_ZCL_STATUS_SUCCESS) {
    emberAfSendDefaultResponse(cmd, status);
  }
  return TRUE;
}

static void messageSentHandler(EmberOutgoingMessageType type,
                               int16u indexOrDestination,
                               EmberApsFrame *apsFrame,
                               int16u msgLen,
                               int8u *message,
                               EmberStatus status)
{
  if (!partner.isIntraPan
      || lastEvent == NO_KEY_ESTABLISHMENT_EVENT
      || partner.pan.intraPan.nodeId != indexOrDestination) {
    // Unknown APS Ack, or an Ack for key establishment not in progress.
    return;
  }

  if (status != EMBER_SUCCESS) {
    emberAfKeyEstablishmentClusterPrintln("Error: Failed to send key establish message to 0x%2x, status: 0x%x",
                                          indexOrDestination,
                                          status);
    cleanupAndStop(MESSAGE_SEND_FAILURE);
    return;
  }
}

// This function should only return FALSE for inter-PAN commands that do not
// include the source long address or if there is no endpoint for the inter-PAN
// network.
static boolean setPartnerFromCommand(const EmberAfClusterCommand *cmd)
{
  partner.isInitiator = TRUE;
  //emberAfKeyEstablishmentClusterPrintln("partner.isInitiator setPartnerFromCommand %u",partner.isInitiator);
  partner.isIntraPan = (cmd->interPanHeader == NULL);
  if (partner.isIntraPan) {
    partner.pan.intraPan.nodeId = cmd->source;
    partner.pan.intraPan.endpoint = cmd->apsFrame->sourceEndpoint;
    keyEstablishmentEndpoint = cmd->apsFrame->destinationEndpoint;
  } else {
    if (!(cmd->interPanHeader->options
          & EMBER_AF_INTERPAN_OPTION_MAC_HAS_LONG_ADDRESS)) {
      return FALSE;
    }
    partner.pan.interPan.panId = cmd->interPanHeader->panId;
    MEMMOVE(partner.pan.interPan.eui64,
            cmd->interPanHeader->longAddress,
            EUI64_SIZE);
    keyEstablishmentEndpoint = emberAfPrimaryEndpointForCurrentNetworkIndex();
    if (keyEstablishmentEndpoint == 0xFF) {
      return FALSE;
    }
  }
  return TRUE;
}

static boolean commandIsFromOurPartner(const EmberAfClusterCommand *cmd)
{
  // For intra-PAN commands, we should check that the source and destination
  // endpoints and the sequence numbers of the request/response pairs match.
  // However, devices in the field are sometimes not careful about endpoints
  // or sequence numbers, so strictness here would cause interoperability
  // problems.  Neither endpoint nor sequence number mismatches are likely to
  // cause serious problems in practice because Key Establishment is intended
  // to be device-wide rather than per-endpoint and we only support a single
  // partner at a time, so checking our partner and correlating requests and
  // responses is not difficult.
  
  // We generally make sure that the direction bit is set correctly, but for a 
  // terminate command we do not.  This is due to timeouts and the fact that
  // our 4.3 stack will always assume that if it is not in the middle of
  // key establishment, then it is the server.  In that case it will send the 
  // Terminate message with the direction set 'server-to-client' even if
  // the request was also 'server-to-client'.
  return (((partner.isInitiator
            == (cmd->direction == ZCL_DIRECTION_CLIENT_TO_SERVER))
           || (cmd->commandId == ZCL_TERMINATE_KEY_ESTABLISHMENT_COMMAND_ID))
          && partner.isIntraPan == (cmd->interPanHeader == NULL)
          && (partner.isIntraPan
              ? partner.pan.intraPan.nodeId == cmd->source
              : ((cmd->interPanHeader->options
                  & EMBER_AF_INTERPAN_OPTION_MAC_HAS_LONG_ADDRESS)
                 && (MEMCOMPARE(partner.pan.interPan.eui64,
                                cmd->interPanHeader->longAddress,
                                EUI64_SIZE) == 0))));
}

static void writeKeyEstablishmentClusterAttribute(int8u endpoint)
{
  if (isCbkeKeyEstablishmentSuiteValid()) {
    int16u keSuiteId = emAfAvailableCbkeSuite;
    EmberAfStatus attrWriteStatus;

    // Make the 'mask' 16-bit so that we can bit shift the client mask (0x80) 
    // and detect it is bigger than 0x80 so our for() loop terminates.
    int16u mask;
    for (mask = CLUSTER_MASK_SERVER; mask <= CLUSTER_MASK_CLIENT; mask <<= 1) {
      attrWriteStatus = emberAfWriteAttribute(endpoint,
                                              ZCL_KEY_ESTABLISHMENT_CLUSTER_ID,
                                              // Conveniently the server and client attribute
                                              // ID are the same.  So we pass just the server ID.
                                              ZCL_KEY_ESTABLISHMENT_SUITE_SERVER_ATTRIBUTE_ID,
                                              (int8u)mask,
                                              (int8u*)&keSuiteId,
                                              ZCL_ENUM16_ATTRIBUTE_TYPE);
      if (attrWriteStatus != EMBER_ZCL_STATUS_SUCCESS) {
        emberAfKeyEstablishmentClusterPrintln("%p%p%p%X",
                                              "Key Est. Suite attr write failed (",
                                              ((mask == CLUSTER_MASK_SERVER)
                                               ? "server"
                                               : "client"),
                                              "): 0x",
                                              attrWriteStatus);
      }
    }
  }
}
//------------------------------------------------------------------------------
// DEBUG

#if defined(EMBER_AF_PRINT_ENABLE) && defined(EMBER_AF_PRINT_KEY_ESTABLISHMENT_CLUSTER)
static void debugPrintSmac(boolean initiatorSmac, int8u *smac)
{
  emberAfKeyEstablishmentClusterPrintln("%p SMAC",
                                        (initiatorSmac
                                         ? "Initiator"
                                         : "Responder"));
  // Conveniently Zigbee Keys are the same length as the SMAC
  emberAfPrintZigbeeKey(smac);
}

static void debugPrintOtherSmac(boolean received, int8u *smac)
{
  emberAfKeyEstablishmentClusterPrintln("%p SMAC",
                                        (received ? "Received" : "Calculated"));
  // Conveniently Zigbee Keys are the same length as the SMAC
  emberAfPrintZigbeeKey(smac);
}

static void debugPrintCert(boolean initiatorCert, int8u *cert)
{
  emberAfKeyEstablishmentClusterPrintln("%p Cert",
                                        (initiatorCert
                                         ? "Initiator"
                                         : "Responder"));
  if (isCbkeKeyEstablishmentSuite163k1())
    emberAfPrintCert(cert);
  else if(isCbkeKeyEstablishmentSuite283k1())
    emberAfPrintCert283k1(cert);
}

static void debugPrintKey(boolean initiatorKey, int8u *key)
{
  emberAfKeyEstablishmentClusterPrintln("%p public key",
                                        (initiatorKey
                                         ? "Initiator"
                                         : "Responder"));
    if (isCbkeKeyEstablishmentSuite163k1())
      emberAfPrintPublicKey(key);
    else if(isCbkeKeyEstablishmentSuite283k1())
      emberAfPrintKey283k1(TRUE,key);
}
#endif

static EmberStatus initiateKeyEstablishment(const EmberEUI64 eui64,
                                            int16u nodeIdOrPanId,
                                            int8u endpoint)
{
  int8u validLastEvent = NO_KEY_ESTABLISHMENT_EVENT;
  if (isCbkeKeyEstablishmentSuiteValid() && lastEvent == NO_KEY_ESTABLISHMENT_EVENT) {
    keyEstablishmentEndpoint = emberAfPrimaryEndpointForCurrentNetworkIndex();
    if (keyEstablishmentEndpoint == 0xFF) {
      return EMBER_INVALID_CALL;
    }
    partner.isInitiator = FALSE;
    //emberAfKeyEstablishmentClusterPrintln("partner.isInitiator initiateKeyEstablishment %u",partner.isInitiator);
    partner.isIntraPan = (eui64 == NULL);
    if (partner.isIntraPan) {
      partner.pan.intraPan.nodeId = nodeIdOrPanId;
      partner.pan.intraPan.endpoint = endpoint;
    } else {
      partner.pan.interPan.panId = nodeIdOrPanId;
      MEMMOVE(partner.pan.interPan.eui64, eui64, EUI64_SIZE);
    }
    partner.sequenceNumber = emberAfNextSequence();
    apsSequenceNumbersReceived = 0;

    //The CBKE 283k1 Library is currently only supported in Intra Pan Key Esbtalishment.
    //For interpan key Establishment, we skip the read attributes stage and 
    //force the use of the CBKE 163k1 Library.
    if(!partner.isIntraPan){
      emberAfKeyEstablishmentClusterPrint("Skipping Read attributes stage for inter-pan");
      lastEvent = CHECK_SUPPORTED_CURVES;
      validLastEvent = CHECK_SUPPORTED_CURVES;
      emAfKeyEstablishmentSelectCurve(EMBER_AF_CBKE_KEY_ESTABLISHMENT_SUITE_163K1);
      keyEstablishStateMachine(BEGIN_KEY_ESTABLISHMENT, NULL, NULL);
      return ((lastEvent == validLastEvent)
        ? EMBER_ERR_FATAL
        : EMBER_SUCCESS);
    }

    // The Test Harness can skip the read attributes stage. This is useful 
    // in running curve specific key establishment tests, irrespective of 
    // which binaries are supported.
    if(emUseTestHarnessSuite == EMBER_AF_INVALID_KEY_ESTABLISHMENT_SUITE)
      keyEstablishStateMachine(CHECK_SUPPORTED_CURVES,NULL,NULL);
    else{
      lastEvent = CHECK_SUPPORTED_CURVES;
      validLastEvent = CHECK_SUPPORTED_CURVES;
      emAfKeyEstablishmentSelectCurve(emUseTestHarnessSuite);
      keyEstablishStateMachine(BEGIN_KEY_ESTABLISHMENT, NULL, NULL);
    }

    return ((lastEvent == validLastEvent)
            ? EMBER_ERR_FATAL
            : EMBER_SUCCESS);
  }
  return EMBER_INVALID_CALL;
}

static boolean checkMalformed283k1Command(boolean isCertificate)
{
   EmberAfClusterCommand *cmd = emberAfCurrentCommand();
   int8u validSize = isCertificate ? 
                     INITIATE_KEY_ESTABLISHMENT_LENGTH_283K1 :
                     EMBER_PUBLIC_KEY_283K1_SIZE ;
   if(isCbkeKeyEstablishmentSuite283k1()){
    emberAfKeyEstablishmentClusterPrintln("Checking size Certificate size %u, expected %u",cmd->bufLen - cmd->payloadStartIndex,validSize);
     if (cmd->bufLen < cmd->payloadStartIndex + validSize){
        sendMalformedMessageTerminateCommand(cmd);
        clearKeyEstablishmentState();
        return TRUE;
     }
   }
   return FALSE;                   
}

static EmberAfStatus validateNewCbkeSuite(int8u endpoint,
                                          int16u newCbkeSuite)
{
  if(newCbkeSuite <= emberAfIsFullSmartEnergySecurityPresent()){
    //emAfAvailableCbkeSuite = newCbkeSuite;
    return EMBER_ZCL_STATUS_SUCCESS;
  } else {
    return EMBER_ZCL_STATUS_INVALID_VALUE;
  }
}
//-----------------------------------------------------------------------------
//Helper functions. These get called from key-establishment-curve-support.c

void cleanupAndStopWithDelay(EmberAfKeyEstablishmentNotifyMessage message,
                             int8u delayInSec)
{
  EmberAfAmiKeyEstablishmentStatus status;
  boolean linkKeyEstablished = (message == LINK_KEY_ESTABLISHED);
  EmberStatus storeLinkKeyStatus = emberClearTemporaryDataMaybeStoreLinkKeyForCurve(linkKeyEstablished);
  
  if (delayInSec == 0) {
    delayInSec = INTERNAL_ERROR_BACK_OFF_TIME;
  }

  emberAfKeyEstablishmentClusterFlush();
  emberAfKeyEstablishmentClusterPrintln("End Key Establishment Status: 0x%x, Store Link Key Status: 0x%x",
                                         message,
                                         storeLinkKeyStatus);

  if (linkKeyEstablished && storeLinkKeyStatus != EMBER_SUCCESS) {
    message = NO_LOCAL_RESOURCES;
  }

  status = appNotifyMessageToKeyEstablishStatus[message];

  // This is a notify because either we are terminating key establishment
  // prematurely, or it succeeded.
  askApplicationWithDelay(message, delayInSec);

  if (!partner.isInitiator && linkKeyEstablished) {
    emberAfSendImmediateDefaultResponse(EMBER_ZCL_STATUS_SUCCESS);
  } else if (status != EMBER_ZCL_AMI_KEY_ESTABLISHMENT_STATUS_SUCCESS
             && message != PARTNER_SENT_DEFAULT_RESPONSE_ERROR
             && message != BAD_KEY_ESTABLISHMENT_SUITE
             && (partner.isInitiator || lastEvent != NO_KEY_ESTABLISHMENT_EVENT)) {
    // No point in sending a terminate when this is the first step and we are
    // the initiator.
    sendTerminateMessage(status, BACK_OFF_TIME_REPORTED_TO_PARTNER);
  }

  clearKeyEstablishmentState();
}

void sendNextKeyEstablishMessage(KeyEstablishMessage message,
                                 int8u *data)
{
  int8u *ptr = appResponseData + EMBER_AF_ZCL_OVERHEAD;
  int8u size = emAfKeyEstablishMessageToDataSize[message + cbkeSuiteOffset];
  boolean certMessage = (message == ZCL_INITIATE_KEY_ESTABLISHMENT_REQUEST_COMMAND_ID);
  if (certMessage) {
    *ptr++ = LOW_BYTE(emAfCurrentCbkeSuite);
    *ptr++ = HIGH_BYTE(emAfCurrentCbkeSuite);
    *ptr++ = EM_AF_ADVERTISED_EPHEMERAL_DATA_GEN_TIME_SECONDS;
    *ptr++ = GENERATE_SHARED_SECRET_TIME_SECONDS;
    size -= 4;  // reduce the size for the 4 bytes we already added
  }
  MEMMOVE(ptr, data, size);
  ptr += size;

  appResponseLength = (ptr - appResponseData);
  sendKeyEstablishMessage(message);
}

//-----------------------------------------------------------------------------
//Helper functions for the test-harness.

void emAfSkipCheckSupportedCurves(EmberAfCbkeKeyEstablishmentSuite suite)
{
  emUseTestHarnessSuite = suite;
}

void emAfSetAvailableCurves(EmberAfCbkeKeyEstablishmentSuite suite)
{
  emAfAvailableCbkeSuite = suite;
  writeKeyEstablishmentClusterAttribute(keyEstablishmentEndpoint);
}

void emAfKeyEstablishmentSelectCurve(EmberAfCbkeKeyEstablishmentSuite suite)
{
  emAfCurrentCbkeSuite   = suite;
  if(suite == EMBER_AF_CBKE_KEY_ESTABLISHMENT_SUITE_163K1){
    cbkeSuiteOffset   = CBKE_163K1_SUITE_OFFSET;
    certSubjectOffset = CERT_SUBJECT_OFFSET;
    certIssuerOffset  = CERT_ISSUER_OFFSET;
  } else if(suite == EMBER_AF_CBKE_KEY_ESTABLISHMENT_SUITE_283K1){
    cbkeSuiteOffset   = CBKE_283K1_SUITE_OFFSET;
    certSubjectOffset = CERT_SUBJECT_OFFSET_283K1;
    certIssuerOffset  = CERT_ISSUER_OFFSET_283K1;
  }
}

//-----------------------------------------------------------------------------
EmberAfStatus emberAfKeyEstablishmentClusterClientPreAttributeChangedCallback(int8u endpoint,
                                                                              EmberAfAttributeId attributeId,
                                                                              EmberAfAttributeType attributeType,
                                                                              int8u size,
                                                                              int8u *value)
{
  return emberAfKeyEstablishmentClusterServerPreAttributeChangedCallback(endpoint,
                                                                         attributeId,
                                                                         attributeType,
                                                                         size,
                                                                         value);
}

EmberAfStatus emberAfKeyEstablishmentClusterServerPreAttributeChangedCallback(int8u endpoint,
                                                                              EmberAfAttributeId attributeId,
                                                                              EmberAfAttributeType attributeType,
                                                                              int8u size,
                                                                              int8u *value)
{
  switch(attributeId){
    case ZCL_KEY_ESTABLISHMENT_SUITE_SERVER_ATTRIBUTE_ID:
      {
        int16u newCbkeSuite;
        MEMMOVE(&newCbkeSuite,value,size);
        return validateNewCbkeSuite(endpoint,newCbkeSuite);
      }
    default:
    return EMBER_ZCL_STATUS_SUCCESS;
  }
}

void emberAfKeyEstablishmentClusterClientAttributeChangedCallback(int8u endpoint,
                                                             EmberAfAttributeId attributeId)
{
  int16u availableSuites;
  int16u serverSuites;

  EmberAfStatus status = emberAfReadClientAttribute(endpoint,
                                                    ZCL_KEY_ESTABLISHMENT_CLUSTER_ID,
                                                    attributeId,
                                                    (int8u *)&availableSuites,
                                                    sizeof(availableSuites));
  if(status != EMBER_SUCCESS){
    emberAfCorePrintln("Reading KE client attribute failed");
    return;
  }

  emAfAvailableCbkeSuite = availableSuites;

  status = emberAfReadServerAttribute(endpoint,
                                      ZCL_KEY_ESTABLISHMENT_CLUSTER_ID,
                                      attributeId,
                                      (int8u *)&serverSuites,
                                      sizeof(serverSuites));
  if(status != EMBER_SUCCESS){
    emberAfCorePrintln("Reading KE server attribute failed");
    return;
  }

  if(availableSuites != serverSuites){
    status = emberAfWriteServerAttribute(endpoint,
                                        ZCL_KEY_ESTABLISHMENT_CLUSTER_ID,
                                        attributeId,
                                        (int8u *)&availableSuites,
                                        ZCL_ENUM16_ATTRIBUTE_TYPE);
    if(status != EMBER_SUCCESS){
      emberAfCorePrintln("Updating KE server status failed");
    }
  }

}

void emberAfKeyEstablishmentClusterServerAttributeChangedCallback(int8u endpoint,
                                                                  EmberAfAttributeId attributeId)
{
  int16u clientSuites;
  int16u availableSuites;
  EmberAfStatus status = emberAfReadServerAttribute(endpoint,
                                                    ZCL_KEY_ESTABLISHMENT_CLUSTER_ID,
                                                    attributeId,
                                                    (int8u *)&availableSuites,
                                                    sizeof(availableSuites));
  if(status != EMBER_SUCCESS){
    emberAfCorePrintln("Reading server attribute failed");
    return;
  }

  emAfAvailableCbkeSuite = availableSuites;
  
  status = emberAfReadClientAttribute(endpoint,
                                      ZCL_KEY_ESTABLISHMENT_CLUSTER_ID,
                                      attributeId,
                                      (int8u *)&clientSuites,
                                      sizeof(clientSuites));

  if(status != EMBER_SUCCESS){
    emberAfCorePrintln("Reading KE client attribute failed");
    return;
  }
  if(availableSuites != clientSuites) {
    status = emberAfWriteClientAttribute(endpoint,
                                         ZCL_KEY_ESTABLISHMENT_CLUSTER_ID,
                                         attributeId,
                                         (int8u *)&availableSuites,
                                         ZCL_ENUM16_ATTRIBUTE_TYPE);
    if(status != EMBER_SUCCESS){
      emberAfCorePrintln("Updating server status failed");
    }

    //emberAfCorePrintln("Upading client suite to %u",availableSuites);
  }
}


void emberAfPluginKeyEstablishmentReadAttributesCallback(EmberAfCbkeKeyEstablishmentSuite suite)
{
  emberAfKeyEstablishmentClusterPrintln("Supported Suites %u",suite);
  emberAfKeyEstablishmentClusterPrintln("partner.isInitiator emberAfPluginKeyEstablishmentReadAttributesCallback %u",partner.isInitiator);
  if(!partner.isInitiator){
    //We identify the highest available suite to use.
    if((suite & emAfAvailableCbkeSuite) >=
        EMBER_AF_CBKE_KEY_ESTABLISHMENT_SUITE_283K1){
      emberAfKeyEstablishmentClusterPrintln("Using cbke-283k1");
      emAfKeyEstablishmentSelectCurve(EMBER_AF_CBKE_KEY_ESTABLISHMENT_SUITE_283K1);
    } else if((suite & emAfAvailableCbkeSuite) <=
               EMBER_AF_CBKE_KEY_ESTABLISHMENT_SUITE_163K1){
      //We accept 0x00(EMBER_AF_INVALID_KEY_ESTABLISHMENT_SUITE) as an acceptable value for 
      //163k1 KE because our stacks 4.6 and older don't initialize the KE attribute to 1.
      emberAfKeyEstablishmentClusterPrintln("Using cbke-163k1");
      emAfKeyEstablishmentSelectCurve(EMBER_AF_CBKE_KEY_ESTABLISHMENT_SUITE_163K1);
    } else {
      emberAfKeyEstablishmentClusterPrintln("Error: No valid Cluster");
      cleanupAndStop(BAD_KEY_ESTABLISHMENT_SUITE);
      return;
    }
      keyEstablishStateMachine(BEGIN_KEY_ESTABLISHMENT,NULL,NULL);
  }
}

EmberStatus emberAfInitiateKeyEstablishmentCallback(EmberNodeId nodeId,
                                                    int8u endpoint)
{
  return initiateKeyEstablishment(NULL, // eui64 - unused
                                  nodeId,
                                  endpoint);
}

EmberStatus emberAfInitiateInterPanKeyEstablishmentCallback(EmberPanId panId,
                                                            const EmberEUI64 eui64)
{
  return initiateKeyEstablishment(eui64,
                                  panId,
                                  0); // endpoint - unused
}

boolean emberAfPerformingKeyEstablishmentCallback(void)
{
  return (lastEvent != NO_KEY_ESTABLISHMENT_EVENT);
}

void emberAfKeyEstablishmentClusterServerInitCallback(int8u endpoint)
{
  clearKeyEstablishmentState();
  setupEventTimeouts(0, 0);
  keyEstablishmentEndpoint = endpoint;

//If the test harness is selected, it is expected to force the selection of the ECC 283k1 curve.
//By default, all variables use the ECC 163k1 curve.
  emAfAvailableCbkeSuite = emberAfIsFullSmartEnergySecurityPresent();


  if(emAfCurrentCbkeSuite == EMBER_AF_CBKE_KEY_ESTABLISHMENT_SUITE_283K1){
    cbkeSuiteOffset   = CBKE_283K1_SUITE_OFFSET;
    certSubjectOffset = CERT_SUBJECT_OFFSET_283K1;
    certIssuerOffset  = CERT_ISSUER_OFFSET_283K1;
  }

  // We use a "core" print in hopes that this message will be seen.
  // Key establishment will not work and mysteriously fail when started.
  emberAfCorePrintln("Key Est. Init %p 0x%x", (isCbkeKeyEstablishmentSuiteValid() ? "Success" : "FAILED!"),LOW_BYTE(emAfAvailableCbkeSuite));
  emberAfKeyEstablishmentClusterPrintln("Key Est. Init %p",
                                        (isCbkeKeyEstablishmentSuiteValid()
                                         ? "Success"
                                         : "FAILED!"));

  // (Case 14692) If we do have full SE security, indicate the proper KE suite
  // in the K.E. Suite attribute for the appropriate endpoint.
  // NOTE: We also take this opportunity to set the client-side KE Suite 
  // attribute value to match.  This is really the KEClusterClientInitCallback's
  // job, but since this plugin is a really a package deal (for client+server)
  // and since the values need to match anyway, we do it here.
  // Implementing the KEClusterClientInitCallback just for this setting
  // seems like overkill anyway and would consume more code space.
  writeKeyEstablishmentClusterAttribute(endpoint);
}

void emberAfKeyEstablishmentClusterServerTickCallback(int8u endpoint)
{
  cleanupAndStop(TIMEOUT_OCCURRED);
}

boolean emberAfKeyEstablishmentClusterServerCommandReceivedCallback(EmberAfClusterCommand *cmd)
{
  return commandReceivedHandler(cmd);
}

boolean emberAfKeyEstablishmentClusterInitiateKeyEstablishmentRequestCallback(int16u keyEstablishmentSuite,
                                                                              int8u ephemeralDataGenerateTime,
                                                                              int8u confirmKeyGenerateTime,
                                                                              int8u *identity)
{
  EmberAfClusterCommand *cmd = emberAfCurrentCommand();
  emberAfKeyEstablishmentClusterPrintln("Suite %u\r\n",keyEstablishmentSuite);
  emAfKeyEstablishmentSelectCurve(keyEstablishmentSuite);
  if (checkMalformed283k1Command(TRUE)) {
    return TRUE;
  }
  keyEstablishStateMachine(BEGIN_KEY_ESTABLISHMENT,
                           cmd->buffer + cmd->payloadStartIndex,
                           identity);
  return TRUE;
}



boolean emberAfKeyEstablishmentClusterEphemeralDataRequestCallback(int8u *ephemeralData)
{
  if(checkMalformed283k1Command(FALSE)) return TRUE;
  keyEstablishStateMachine(GENERATE_KEYS, ephemeralData, NULL);
  return TRUE;
}

boolean emberAfKeyEstablishmentClusterConfirmKeyDataRequestCallback(int8u *secureMessageAuthenticationCode)
{
  keyEstablishStateMachine(GENERATE_SHARED_SECRET,
                           secureMessageAuthenticationCode,
                           NULL);
  return TRUE;
}

boolean emberAfKeyEstablishmentClusterTerminateKeyEstablishmentCallback(int8u statusCode,
                                                                        int8u waitTime,
                                                                        int16u keyEstablishmentSuite)
{
  EmberAfClusterCommand *cmd = emberAfCurrentCommand();
  if (cmd != NULL && cmd->type >= EMBER_INCOMING_MULTICAST) {
    emberAfKeyEstablishmentClusterPrintln("Ignoring Broadcast KE terminate");
    return TRUE;
  }

  if (lastEvent != NO_KEY_ESTABLISHMENT_EVENT && cmd != NULL && commandIsFromOurPartner(cmd)) {
    if (partner.isIntraPan) {
      apsSequenceNumbers[apsSequenceNumbersReceived] = cmd->apsFrame->sequence;
      apsSequenceNumbersReceived++;
    }
    emberAfKeyEstablishmentClusterPrintln("Terminate Received, Status(%d): %p",
                                          statusCode,
                                          terminateStatus[((statusCode > UNKNOWN_TERMINATE_STATUS)
                                                           ? UNKNOWN_TERMINATE_STATUS
                                                           : statusCode)]);
    cleanupAndStopWithDelay(PARTNER_SENT_TERMINATE, waitTime);
  }
  emberAfSendDefaultResponse(cmd, EMBER_ZCL_STATUS_SUCCESS);
  return TRUE;
}

void emberAfKeyEstablishmentClusterServerMessageSentCallback(EmberOutgoingMessageType type,
                                                             int16u indexOrDestination,
                                                             EmberApsFrame *apsFrame,
                                                             int16u msgLen,
                                                             int8u *message,
                                                             EmberStatus status)
{
  messageSentHandler(type, indexOrDestination, apsFrame, msgLen, message, status);
}

static void defaultResponseCallback(int8u endpoint, 
                                    int8u commandId, 
                                    EmberAfStatus status)
{
  EmberAfClusterCommand *cmd = emberAfCurrentCommand();

  if (cmd != NULL && cmd->type >= EMBER_INCOMING_MULTICAST) {
    emberAfKeyEstablishmentClusterPrintln("Ignoring Broadcast KE default resp");
    return;
  }

  if (status != EMBER_ZCL_STATUS_SUCCESS
      && lastEvent != NO_KEY_ESTABLISHMENT_EVENT
      && commandIsFromOurPartner(cmd)) {
    emberAfKeyEstablishmentClusterPrintln("Got Default Response with error code: %d", 
                                          status);
    // While the actual status code may be more meaningful, we don't really care.
    // We interpret it as a transient failure.  Otherwise we would have got an 
    // explicit Terminate Message.
    cleanupAndStop(PARTNER_SENT_DEFAULT_RESPONSE_ERROR);
  }
}

void emberAfKeyEstablishmentClusterClientDefaultResponseCallback(int8u endpoint, 
                                                                 int8u commandId, 
                                                                 EmberAfStatus status)
{
  defaultResponseCallback(endpoint, commandId, status);
}

void emberAfKeyEstablishmentClusterServerDefaultResponseCallback(int8u endpoint, 
                                                                 int8u commandId, 
                                                                 EmberAfStatus status)
{
  defaultResponseCallback(endpoint, commandId, status);
}


boolean emberAfKeyEstablishmentClusterClientCommandReceivedCallback(EmberAfClusterCommand *cmd)
{
  return commandReceivedHandler(cmd);
}

boolean emberAfKeyEstablishmentClusterInitiateKeyEstablishmentResponseCallback(int16u requestedKeyEstablishmentSuite,
                                                                               int8u ephemeralDataGenerateTime,
                                                                               int8u confirmKeyGenerateTime,
                                                                               int8u *identity)
{
  EmberAfClusterCommand *cmd = emberAfCurrentCommand();
  if(checkMalformed283k1Command(TRUE)) return TRUE;
  keyEstablishStateMachine(GENERATE_KEYS,
                           cmd->buffer + cmd->payloadStartIndex,
                           identity);
  return TRUE;
}

boolean emberAfKeyEstablishmentClusterEphemeralDataResponseCallback(int8u *ephemeralData)
{
  if(checkMalformed283k1Command(FALSE)) return TRUE;
  keyEstablishStateMachine(GENERATE_SHARED_SECRET, ephemeralData, NULL);
  return TRUE;
}

boolean emberAfKeyEstablishmentClusterConfirmKeyDataResponseCallback(int8u *secureMessageAuthenticationCode)
{
  keyEstablishStateMachine(INITIATOR_RECEIVED_CONFIRM_KEY,
                           secureMessageAuthenticationCode,
                           NULL);
  return TRUE;
}

void emberAfKeyEstablishmentClusterClientMessageSentCallback(EmberOutgoingMessageType type,
                                                             int16u indexOrDestination,
                                                             EmberApsFrame *apsFrame,
                                                             int16u msgLen,
                                                             int8u *message,
                                                             EmberStatus status)
{
  messageSentHandler(type, indexOrDestination, apsFrame, msgLen, message, status);
}

//------------------------------------------------------------------------------
// CBKE Library Callbacks

void emAfPluginKeyEstablishmentGenerateCbkeKeysHandler(EmberStatus status,
                                                       EmberPublicKeyData *ephemeralPublicKey)
{
  emberAfKeyEstablishmentClusterPrintln("GenerateCbkeKeysHandler() returned: 0x%x",
                                         status);
  emAfCryptoOperationComplete();

  if (status != EMBER_SUCCESS) {
    cleanupAndStop(NO_LOCAL_RESOURCES);
    return;
  }

  if (emAfKeyEstablishmentTestHarnessCbkeCallback(CBKE_OPERATION_GENERATE_KEYS,
                                                  ephemeralPublicKey->contents,
                                                  NULL)) {
    return;
  }

  keyEstablishStateMachine(SEND_EPHEMERAL_DATA_MESSAGE,
                           emberPublicKeyContents(ephemeralPublicKey),
                           NULL);
}


void emAfPluginKeyEstablishmentCalculateSmacsHandler(EmberStatus status,
                                                     EmberSmacData *initiatorSmacReturn,
                                                     EmberSmacData *responderSmacReturn)
{
  emberAfKeyEstablishmentClusterPrintln("CalculateSmacsHandler() returned: 0x%x",
                                         status);
  emAfCryptoOperationComplete();
  debugPrintSmac(TRUE,  emberSmacContents(initiatorSmacReturn));
  debugPrintSmac(FALSE, emberSmacContents(responderSmacReturn));

  if (status != EMBER_SUCCESS) {
    cleanupAndStop(NO_LOCAL_RESOURCES);
    return;
  }

  if (emAfKeyEstablishmentTestHarnessCbkeCallback(CBKE_OPERATION_GENERATE_SECRET,
                                                  initiatorSmacReturn->contents,
                                                  responderSmacReturn->contents)) {
    return;
  }
  
  keyEstablishStateMachine(SEND_CONFIRM_KEY_MESSAGE,
                           emberKeyContents(initiatorSmacReturn),
                           emberKeyContents(responderSmacReturn));
}

void emAfPluginKeyEstablishmentGenerateCbkeKeysHandler283k1(EmberStatus status,
                                                            EmberPublicKey283k1Data *ephemeralPublicKey)
{
  emberAfKeyEstablishmentClusterPrintln("GenerateCbkeKeysHandler283k1() returned: 0x%x",
                                         status);
   emAfCryptoOperationComplete();

  if (status != EMBER_SUCCESS) {
    cleanupAndStop(NO_LOCAL_RESOURCES);
    return;
  }
  
  keyEstablishStateMachine(SEND_EPHEMERAL_DATA_MESSAGE,
                         emberPublicKey283k1Contents(ephemeralPublicKey),
                         NULL);
}

void emAfPluginKeyEstablishmentCalculateSmacsHandler283k1(EmberStatus status,
                                                          EmberSmacData* initiatorSmac,
                                                          EmberSmacData* responderSmac)
{
  emberAfKeyEstablishmentClusterPrintln("CalculateSmacsHandler() returned: 0x%x",
                                         status);
  emAfCryptoOperationComplete();

  if (status != EMBER_SUCCESS) {
    cleanupAndStop(NO_LOCAL_RESOURCES);
    return;
  }

  keyEstablishStateMachine(SEND_CONFIRM_KEY_MESSAGE,
                           emberKeyContents(initiatorSmac),
                           emberKeyContents(responderSmac));
}
