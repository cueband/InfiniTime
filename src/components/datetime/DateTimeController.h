#pragma once

#include "cueband.h"

#include <cstdint>
#include <chrono>
#include <string>
#include "components/settings/Settings.h"

#ifdef CUEBAND_UPTIME_1024                 // Track system uptime in units of 1024
typedef uint64_t uptime1024_t;
#endif

namespace Pinetime {
  namespace System {
    class SystemTask;
  }
  namespace Controllers {
    class DateTime {
    public:
      DateTime(Controllers::Settings& settingsController);
      enum class Days : uint8_t { Unknown, Monday, Tuesday, Wednesday, Thursday, Friday, Saturday, Sunday };
      enum class Months : uint8_t {
        Unknown,
        January,
        February,
        March,
        April,
        May,
        June,
        July,
        August,
        September,
        October,
        November,
        December
      };

      void SetTime(uint16_t year,
                   uint8_t month,
                   uint8_t day,
                   uint8_t dayOfWeek,
                   uint8_t hour,
                   uint8_t minute,
                   uint8_t second,
                   uint32_t systickCounter);
      void UpdateTime(uint32_t systickCounter);
      uint16_t Year() const {
        return year;
      }
      Months Month() const {
        return month;
      }
      uint8_t Day() const {
        return day;
      }
      Days DayOfWeek() const {
        return dayOfWeek;
      }
      uint8_t Hours() const {
        return hour;
      }
      uint8_t Minutes() const {
        return minute;
      }
      uint8_t Seconds() const {
        return second;
      }

      const char* MonthShortToString() const;
      const char* DayOfWeekShortToString() const;
      static const char* MonthShortToStringLow(Months month);

      std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> CurrentDateTime() const {
        return currentDateTime;
      }
      std::chrono::seconds Uptime() const {
        return uptime;
      }
#ifdef CUEBAND_UPTIME_1024                 // Track system uptime in units of 1024
      uptime1024_t uptime1024 = 0;
      uptime1024_t Uptime1024() const {
        return uptime1024;
      }
#endif
#ifdef CUEBAND_DETECT_UNSET_TIME
      bool IsUnset() const {
          uint32_t now = std::chrono::duration_cast<std::chrono::seconds>(CurrentDateTime().time_since_epoch()).count();
          return now < CUEBAND_DETECT_UNSET_TIME;
      }
#endif

      void Register(System::SystemTask* systemTask);
      void SetCurrentTime(std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> t);
      std::string FormattedTime();

    private:
      uint16_t year = 0;
      Months month = Months::Unknown;
      uint8_t day = 0;
      Days dayOfWeek = Days::Unknown;
      uint8_t hour = 0;
      uint8_t minute = 0;
      uint8_t second = 0;

      uint32_t previousSystickCounter = 0;
      std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> currentDateTime;
      std::chrono::seconds uptime {0};

      bool isMidnightAlreadyNotified = false;
      bool isHourAlreadyNotified = true;
      bool isHalfHourAlreadyNotified = true;
      System::SystemTask* systemTask = nullptr;
      Controllers::Settings& settingsController;
    };
  }
}
