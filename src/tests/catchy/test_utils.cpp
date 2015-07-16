// (C) 2015 Simon Warta (Kullo GmbH)
// Botan is released under the Simplified BSD License (see license.txt)

#include "catch.hpp"

#include <botan/calendar.h>
#include <botan/internal/rounding.h>

using namespace Botan;

TEST_CASE("round_up strictly positive", "[utils]")
   {
   CHECK(( round_up( 1, 10) == 10 ));
   CHECK(( round_up( 3, 10) == 10 ));
   CHECK(( round_up( 9, 10) == 10 ));
   CHECK(( round_up(10, 10) == 10 ));

   CHECK(( round_up( 1, 4) ==  4 ));
   CHECK(( round_up( 3, 4) ==  4 ));
   CHECK(( round_up( 4, 4) ==  4 ));
   CHECK(( round_up( 9, 4) == 12 ));
   CHECK(( round_up(10, 4) == 12 ));
   }

/*
This was broken

TEST_CASE("round_up strictly negative", "[utils]")
   {
   CHECK(( round_up( -1, 10) ==   0 ));
   CHECK(( round_up( -3, 10) ==   0 ));
   CHECK(( round_up( -9, 10) ==   0 ));
   CHECK(( round_up(-10, 10) == -10 ));

   CHECK(( round_up( -1, 3) ==  0 ));
   CHECK(( round_up( -3, 3) == -3 ));
   CHECK(( round_up( -8, 3) == -3 ));
   CHECK(( round_up( -9, 3) == -9 ));
   CHECK(( round_up(-10, 3) == -9 ));
   }
*/

TEST_CASE("round_up zero", "[utils]")
   {
   CHECK(( round_up(0, 2) == 0 ));
   CHECK(( round_up(0, 10) == 0 ));
   CHECK(( round_up(0, 1000) == 0 ));
   CHECK(( round_up(0, 99999) == 0 ));
   CHECK(( round_up(0, 2222222) == 0 ));
   }

TEST_CASE("round_up invalid input", "[utils]")
   {
   CHECK_THROWS( round_up(3, 0) );
   CHECK_THROWS( round_up(5, 0) );
   }

TEST_CASE("calendar_point constructor works", "[utils]")
   {
      {
      auto point1 = calendar_point(1988, 04, 23, 14, 37, 28);
      CHECK(( point1.year == 1988 ));
      CHECK(( point1.month == 4 ));
      CHECK(( point1.day == 23 ));
      CHECK(( point1.hour == 14 ));
      CHECK(( point1.minutes == 37 ));
      CHECK(( point1.seconds == 28 ));
      }

      {
      auto point2 = calendar_point(1800, 01, 01, 0, 0, 0);
      CHECK(( point2.year == 1800 ));
      CHECK(( point2.month == 1 ));
      CHECK(( point2.day == 1 ));
      CHECK(( point2.hour == 0 ));
      CHECK(( point2.minutes == 0 ));
      CHECK(( point2.seconds == 0 ));
      }

      {
      auto point = calendar_point(2037, 12, 31, 24, 59, 59);
      CHECK(( point.year == 2037 ));
      CHECK(( point.month == 12 ));
      CHECK(( point.day == 31 ));
      CHECK(( point.hour == 24 ));
      CHECK(( point.minutes == 59 ));
      CHECK(( point.seconds == 59 ));
      }

      {
      auto point = calendar_point(2100, 5, 1, 0, 0, 0);
      CHECK(( point.year == 2100 ));
      CHECK(( point.month == 5 ));
      CHECK(( point.day == 1 ));
      CHECK(( point.hour == 0 ));
      CHECK(( point.minutes == 0 ));
      CHECK(( point.seconds == 0 ));
      }
   }

TEST_CASE("calendar_point to stl timepoint and back", "[utils]")
   {
      {
      auto in = calendar_point(1988, 04, 23, 14, 37, 28);
      auto out = calendar_value(in.to_std_timepoint());
      CHECK(( out.year    == 1988 ));
      CHECK(( out.month   == 4 ));
      CHECK(( out.day     == 23 ));
      CHECK(( out.hour    == 14 ));
      CHECK(( out.minutes == 37 ));
      CHECK(( out.seconds == 28 ));
      }

      SECTION("latest possible time point")
      {
      auto in = calendar_point(2037, 12, 31, 23, 59, 59);
      auto out = calendar_value(in.to_std_timepoint());
      CHECK(( out.year    == 2037 ));
      CHECK(( out.month   == 12 ));
      CHECK(( out.day     == 31 ));
      CHECK(( out.hour    == 23 ));
      CHECK(( out.minutes == 59 ));
      CHECK(( out.seconds == 59 ));
      }

      SECTION("year too early")
      {
      auto in = calendar_point(1800, 01, 01, 0, 0, 0);
      CHECK_THROWS( in.to_std_timepoint() );
      }

      SECTION("year too late")
      {
      auto in = calendar_point(2038, 01, 01, 0, 0, 0);
      CHECK_THROWS( in.to_std_timepoint() );
      }
   }
