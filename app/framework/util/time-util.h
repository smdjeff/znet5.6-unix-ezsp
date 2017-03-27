/**
 * @file time-util.h
 *
 * <!--Copyright 2015 Silicon Laboratories, Inc.                         *80*-->
 */

#ifndef __TIME_UTIL_H__
#define __TIME_UTIL_H__

#define SECONDS_IN_MINUTE 60
#define SECONDS_IN_HOUR 3600
#define SECONDS_IN_DAY   (SECONDS_IN_MINUTE * 60 * 24)
#define SECONDS_IN_WEEK  (SECONDS_IN_DAY * 7)
#define DURATION_FOREVER_U32  0xFFFFFFFF

/**
 * @brief ZCL Date comparison function.
 * The results are undefined for dates that contain the do not care value
 * in any of the subfields.
 * @return -1, if val1 is smaller
 *          0, if they are the same
 *          1, if val2 is smaller
 */
int8s emberAfCompareDates(EmberAfDate* date1, EmberAfDate* date2);

/**
 * @brief function that copies a ZigBee Date into a buffer
 */
void emberAfCopyDate(int8u *dest, int16u index, EmberAfDate *src);

 /**
  * @brief Decode the given int32u into a ZCL Date object where
  * the int32u is formatted as follows:
  *
  * (0xFF000000 & value) = year
  * (0x00FF0000 & value) = month
  * (0x0000FF00 & value) = day of month
  * (0x000000FF & value) = day of week
  *
  */
void emberAfDecodeDate(int32u src, EmberAfDate* dest);

/**
 * @brief Encode and return the given ZCL Date object as an int32u.
 * Refer to emberAFDecodeDate for details on how the information is stored
 * within an int32u.
 */
int32u emberAfEncodeDate(EmberAfDate* date);

/**
 * @brief Fills the a time structure based on the passed UTC time.
 *
 */
void emberAfFillTimeStructFromUtc(int32u utcTime,
                                  EmberAfTimeStruct* returnTime);
/**
 * @brief Returns the number of days in the month specified in the EmberAfTimeStruct.
 *
 */
int8u emberAfGetNumberDaysInMonth( EmberAfTimeStruct *time );

/**
 * @brief Calculate a UTC time from the passed time structure.
 *
 */
int32u emberAfGetUtcFromTimeStruct( EmberAfTimeStruct *time );

/**
 * @brief Determine the week day (Monday=0 ... Sunday=6) based on
 * a specified UTC time.
 */
int8u emberAfGetWeekdayFromUtc( int32u utcTime );

/*
 * @brief Prints out a human readable date form from the given ZCL data type.
 */
void emberAfPrintDate(const EmberAfDate * date);
void emberAfPrintDateln(const EmberAfDate * date);

/**
 * @brief Sets current time.
 * Convenience function for setting the time to a value.
 * If the time server cluster is implemented on this device,
 * then this call will be passed along to the time cluster server
 * which will update the time. Otherwise the emberAfSetTimeCallback
 * is called, which in the case of the stub does nothing.
 *
 * @param utcTime: A ZigBee time, the number of seconds since the
 *                 year 2000.
 */
void emberAfSetTime(int32u utcTime);

/**
 * @brief Retrieves current time.
 *
 * Convienience function for retrieving the current time.
 * If the time server cluster is implemented, then the time
 * is retrieved from that cluster's time attribute. Otherwise,
 * the emberAfGetCurrentTimeCallback is called.
 *
 * A real time is expected to in the ZigBee time format, the number
 * of seconds since the year 2000.
 */
int32u emberAfGetCurrentTime(void);

/**
 * @brief Prints time.
 *
 * Convenience function for all clusters to print time.
 * This function expects to be passed a ZigBee time which
 * is the number of seconds since the year 2000. If
 * EMBER_AF_PRINT_CORE is defined, this function will print
 * a human readable time from the passed value. If not, this
 * function will print nothing.
 *
 * @param utcTime: A ZigBee time, the number of seconds since the
 *                 year 2000.
 */
void emberAfPrintTime(int32u utcTime);

#endif // __TIME_UTIL_H__