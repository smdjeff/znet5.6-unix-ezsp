// *******************************************************************                                                                                        
// * events-server-cli.c
// *
// *
// * Copyright 2014 by Silicon Laboratories. All rights reserved.           *80*                                                                              
// ******************************************************************* 

#include "app/framework/include/af.h"
#include "events-server.h"

#ifndef EMBER_AF_GENERATE_CLI
  #error The Events Server plugin is not compatible with the legacy CLI.
#endif

// plugin events-server clear <endpoint:1> <logId:1>
void emAfEventsServerCliClear(void)
{
  int8u endpoint = (int8u)emberUnsignedCommandArgument(0);
  EmberAfEventLogId logId = (EmberAfEventLogId)emberUnsignedCommandArgument(1);
  emberAfEventsServerClearEventLog(endpoint, logId);
}

// plugin events-server print <endpoint:1> <logId:1>
void emAfEventsServerCliPrint(void) 
{
  int8u endpoint = (int8u)emberUnsignedCommandArgument(0);
  EmberAfEventLogId logId = (EmberAfEventLogId)emberUnsignedCommandArgument(1);
  emberAfEventsServerPrintEventLog(endpoint, logId);
}

// plugin events-server eprint <endpoint:1> <logId:1> <index:1>
void emAfEventsServerCliPrintEvent(void) 
{
  int8u endpoint = (int8u)emberUnsignedCommandArgument(0);
  EmberAfEventLogId logId = (EmberAfEventLogId)emberUnsignedCommandArgument(1);
  int8u index = (int8u)emberUnsignedCommandArgument(2);
  EmberAfEvent event;
  if (emberAfEventsServerGetEvent(endpoint, logId, index, &event)) {
    emberAfEventsClusterPrintln("Event at index 0x%x in log 0x%x", index, logId);
    emberAfEventsServerPrintEvent(&event);
  } else {
    emberAfEventsClusterPrintln("Event at index 0x%x in log 0x%x is not present", index, logId);
  }
}

// plugin events-server set <endpoint:1> <logId:1> <index:1> <eventId:2> <eventTime:4> <data:?>
void emAfEventsServerCliSet(void)
{
  EmberAfEvent event;
  int8u length;
  int8u endpoint = (int8u)emberUnsignedCommandArgument(0);
  EmberAfEventLogId logId = (EmberAfEventLogId)emberUnsignedCommandArgument(1);
  int8u index = (int8u)emberUnsignedCommandArgument(2);
  event.eventId = (int16u)emberUnsignedCommandArgument(3);
  event.eventTime = (int32u)emberUnsignedCommandArgument(4);
  length = emberCopyStringArgument(5,
                                   event.eventData + 1,
                                   EMBER_AF_PLUGIN_EVENTS_SERVER_EVENT_DATA_LENGTH,
                                   FALSE);
  event.eventData[0] = length;
  if (!emberAfEventsServerSetEvent(endpoint, logId, index, &event)) {
    emberAfEventsClusterPrintln("Event at index 0x%x in log 0x%x is not present", index, logId);
  } else {
    emberAfEventsClusterPrintln("Event added to log 0x%x at index 0x%x", logId, index);
  }
}

// plugin events-server add <endpoint:1> <logId:1> <eventId:2> <eventTime:4> <data:?>
void emAfEventsServerCliAdd(void)
{
  int8u length;
  int8u endpoint = (int8u)emberUnsignedCommandArgument(0);
  EmberAfEventLogId logId = (EmberAfEventLogId)emberUnsignedCommandArgument(1);
  EmberAfEvent event;
  event.eventId = (int16u)emberUnsignedCommandArgument(2);
  event.eventTime = (int32u)emberUnsignedCommandArgument(3);
  length = emberCopyStringArgument(4,
                                   event.eventData + 1,
                                   EMBER_AF_PLUGIN_EVENTS_SERVER_EVENT_DATA_LENGTH,
                                   FALSE);
  event.eventData[0] = length;
  emberAfEventsServerAddEvent(endpoint, logId, &event);
}

// plugin events-server publish <nodeId:2> <srcEndpoint:1> <dstEndpoint:1> <logId:1> <index:1> <eventControl:1>
void emAfEventsServerCliPublish(void)
{
  EmberNodeId nodeId = (EmberNodeId)emberUnsignedCommandArgument(0);
  int8u srcEndpoint = (int8u)emberUnsignedCommandArgument(1);
  int8u dstEndpoint = (int8u)emberUnsignedCommandArgument(2);
  EmberAfEventLogId logId = (EmberAfEventLogId)emberUnsignedCommandArgument(3);
  int8u index = (int8u)emberUnsignedCommandArgument(4);
  int8u eventControl = (int8u)emberUnsignedCommandArgument(5);
  emberAfEventsServerPublishEventMessage(nodeId,
                                         srcEndpoint,
                                         dstEndpoint,
                                         logId,
                                         index,
                                         eventControl);
}
