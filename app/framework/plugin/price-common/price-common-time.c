#include "app/framework/include/af.h"
#include "app/framework/util/time-util.h"
#include "price-common-time.h"

int32u emberAfPluginPriceCommonClusterGetAdjustedStartTime(int32u startTimeUtc,
                                                           int8u durationType ){
  // According to the SE spec, for non-MINUTE times, the duration control indicates if the duration
  // shall run from the START or END of the respective day, week, or month.
  // See SE-1.2a.09 - section D.4.2.4.2 (Publish Block Period command, start time description)
  // for more details.
  //   Start time set to 00:00:00 on applicable date for START_TIME, or to 23:59:59 on applicable
  //   date for end time.
  EmberAfTimeStruct time;
  int32u adjustedStartTimeUtc;
  int8u durationTimebase = (durationType & 0x0F);
  int8u durationControl  = (durationType & 0xF0) >> 4;
  int8u weekday;

  // startTime of 0x00 means "now"
  if (startTimeUtc == 0x00){
    return emberAfGetCurrentTime();
  }

  // Set adjusted time the same for DURATION_MINS, or if any inputs are invalid.
  adjustedStartTimeUtc = startTimeUtc;

  if( durationTimebase != EMBER_ZCL_BLOCK_PERIOD_DURATION_TYPE_TIMEBASE_MINUTES ){
    // Adjust start time based on Start/End of day, week, month
    emberAfFillTimeStructFromUtc( startTimeUtc, &time );
    if( durationTimebase == EMBER_ZCL_BLOCK_PERIOD_DURATION_TYPE_TIMEBASE_DAYS ){
      // Set adjusted start time to start or end of the current day
      time.hours = 0;
      time.minutes = 0;
      time.seconds = 0;
      adjustedStartTimeUtc = emberAfGetUtcFromTimeStruct( &time );
      if( durationControl == END_OF_TIMEBASE ){
        adjustedStartTimeUtc += (SECONDS_IN_DAY - 1);
      }
    }
    else if( durationTimebase == EMBER_ZCL_BLOCK_PERIOD_DURATION_TYPE_TIMEBASE_WEEKS ){
      // First adjust to start of current day.
      time.hours = 0;
      time.minutes = 0;
      time.seconds = 0;
      adjustedStartTimeUtc = emberAfGetUtcFromTimeStruct( &time );
      // now adjust that to the start of the week
      // weekday ranges from 0 to 6.
      weekday = emberAfGetWeekdayFromUtc( adjustedStartTimeUtc );
      while( weekday > 0 ){
        adjustedStartTimeUtc -= SECONDS_IN_DAY;
        weekday--;
      }
      if( durationControl == END_OF_TIMEBASE ){
        // Jump to last second of current week.
        adjustedStartTimeUtc += (SECONDS_IN_WEEK - 1);
      }
    }
    else if( durationTimebase == EMBER_ZCL_BLOCK_PERIOD_DURATION_TYPE_TIMEBASE_MONTHS ){
      // Get beginning of current month.
      time.hours = 0;
      time.minutes = 0;
      time.seconds = 0;
      time.day = 1;
      if( durationControl == END_OF_TIMEBASE ){
        // Calculate time at beginning of next month, then subtract 1 second
        // to get the end of the current month.
        time.month++;
        if( time.month >= 13 ){
          time.month = 1;
          time.year++;
        }
        adjustedStartTimeUtc = emberAfGetUtcFromTimeStruct( &time ) - 1;
      }
      else{
        adjustedStartTimeUtc = emberAfGetUtcFromTimeStruct( &time );
      }
    }
  }
  return adjustedStartTimeUtc;
}

int32u emberAfPluginPriceCommonClusterConvertDurationToSeconds(int32u startTimeUtc,
                                                               int32u duration,
                                                               int8u durationType ){
  // Convert the duration from durationType units (minutes, days, weeks, etc) into seconds.
  EmberAfTimeStruct time;
  int8u durationTimebase = (durationType & 0x0F);
  int8u durationControl  = (durationType & 0xF0) >> 4;
  int32u endTimeUtc;


  if( duration == DURATION_FOREVER_U32 ){
    duration = 0xFFFFFFFF - startTimeUtc;
  }
  else{
    if( durationTimebase == EMBER_ZCL_BLOCK_PERIOD_DURATION_TYPE_TIMEBASE_MINUTES ){
      duration *= SECONDS_IN_MINUTE;
    }
    else if( durationTimebase == EMBER_ZCL_BLOCK_PERIOD_DURATION_TYPE_TIMEBASE_DAYS ){
      duration *= SECONDS_IN_DAY;
    }
    else if( durationTimebase == EMBER_ZCL_BLOCK_PERIOD_DURATION_TYPE_TIMEBASE_WEEKS ){
      duration *= SECONDS_IN_WEEK;
    }
    else if( durationTimebase == EMBER_ZCL_BLOCK_PERIOD_DURATION_TYPE_TIMEBASE_MONTHS ){
      // Convert startTime into date stamp.
      // Add number of months, then convert back into UTC.
      // Calculate delta.
      // Be sure to start with the adjusted start time!!
      //   Recall 1 month starting at beginning of month has the # days of current month.
      //   1 month starting at end of month has the # days of the next month.
      startTimeUtc = emberAfPluginPriceCommonClusterGetAdjustedStartTime( startTimeUtc, durationType );

      emberAfFillTimeStructFromUtc( startTimeUtc, &time );
      while( duration > 12 ){
        time.year++;
        duration -= 12;
      }
      time.month += duration;
      if( time.month > 12 ){
        time.month -= 12;
        time.year++;
      }
      if( durationControl == END_OF_TIMEBASE ){
        // Update time day to the last day of the month
        time.day = emberAfGetNumberDaysInMonth( &time );
      }

      endTimeUtc = emberAfGetUtcFromTimeStruct( &time );
      duration = endTimeUtc - startTimeUtc;
    }
  }
  return duration;
}
