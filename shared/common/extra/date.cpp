#include "date.h"

using namespace iso;

const uint8		month_lengths[]	= {31,28,31,30,31,30,31,31,30,31,30,31};
const uint16	month_starts[]	= {0,31,59,90,120,151,181,212,243,273,304,334};

uint32 DateTime::Read_ISO_8601(string_scan &s) {
	uint32	flags		= 0;
	int		year		= 0;
	int		day			= 0;
	uint32	secs		= 0;
	uint32	micro		= 0;

	if (s.peekc() == '+' || s.peekc() == '-') {
		bool	neg	= s.getc() == '-';
		year	= from_string<uint32>(s.get_token(char_set::digit));
		if (neg)
			year = -year;
		flags	= ISO_8601::YEAR;

	} else {
		auto	n = s.get_token(char_set::digit);
		year	= from_string<uint32>(n);
		flags	= ISO_8601::YEAR;
		if (n.length() <= 2) {
			year	*= 100;
			flags	= ISO_8601::CENTURY;
		}
	}

	s.check('-');

	if (s.check('W')) {
		uint32	week = from_string<uint32>(s.get_n(2));
		flags	= ISO_8601::WEEK;
		s.check('-');

		day = (week - 1) * 7;
		//first week must contain year's first thursday
		if (Date::Days(year, 0) % 7 >= 4)
			day -= 7;

		if (is_digit(s.peekc())) {
			flags	= ISO_8601::DAY;
			day		+= s.getc() - '1';
		}

	} else {
		auto	n = s.get_token(char_set::digit);
		switch (n.length()) {
			case 2: {
				uint32	month = from_string<uint32>(n);
				day	= month_starts[month - 1];
				if (month > 2 && Date::IsLeap(year))
					day++;

				s.check('-');

				if (is_digit(s.peekc())) {
					day		+= from_string<uint32>(s.get_n(2)) - 1;
					flags	= ISO_8601::DAY;
				} else {
					flags	= ISO_8601::MONTH;
				}
				break;
			}
			case 3:
				day		= from_string<uint32>(n);
				flags	= ISO_8601::DAY;
				break;

			default:
				break;
		}
	}

	if (s.check('T')) {
		secs	= from_string<uint32>(s.get_n(2)) * 3600;
		flags	= ISO_8601::HOUR;

		if (s.remaining()) {
			s.check(':');
			secs	+= from_string<uint32>(s.get_n(2)) * 60;
			flags		= ISO_8601::MINUTE;

			if (s.remaining()) {
				s.check(':');
				secs	+= from_string<uint32>(s.get_n(2));
				flags	= ISO_8601::SECOND;

				if (s.check('.')) {
					auto	n = s.get_token(char_set::digit);
					micro = pow_mul(10u, n.size32() - 6, from_string<uint32>(n));
				}
			}
		}
		if (!s.check('Z') && (s.peekc() == '+' || s.peekc() == '-')) {
			bool	neg		= s.getc() == '-';
			uint32	offset	= from_string<uint32>(s.get_n(2)) * 3600;
			if (s.remaining()) {
				s.check(':');
				offset += from_string<uint32>(s.get_n(2)) * 60;
			}
			if (neg)
				secs += offset;
			else
				secs -= offset;
			flags |= ISO_8601::TIMEZONE;
		}
	}

	*this = DateTime(year, day) + Secs(secs) + micro;
	return flags;
};

void Date::ToMonthDay(int d) {
	int	m	= 0;
	while (d >= month_lengths[m])
		d -= month_lengths[m++];
	month	= m + 1;
	day		= d + 1;
}

int Date::FromMonthDay(int month, int day) {
	return day - 1 + month_starts[month - 1];
}

int Date::DaysInMonth(int month, int year) {
	return month_lengths[month - 1] + int(month == 2 && IsLeap(year));
}

Date::Date(int _d) {
	uint32	d = (bc = _d < 0) ? -_d : _d;
	dow		= d % 7;
	if (bc)
		dow = 7 - dow;

	uint32	y = 0;
	if (d >= 31 + 28) {
		uint32	t;
		d -= 31 + 28;
		t = d / (365 * 400 + 97);
		d -= t * (365 * 400 + 97);
		y = t * 400;

		if (d != 0) {
			t = d / (365 * 100 + 24);
			d -= t * (365 * 100 + 24);
			y += t * 100;

			bool	leapday = d == 0;
			if (!leapday) {
				t = d / (365 * 4 + 1);
				d -= t * (365 * 4 + 1);
				y += t * 4;
				if (leapday = d == 4 * 365)
					y += 4;
			}
			if (leapday) {
				year	= y;
				month	= 2;
				day		= 29;
				return;
			}
		}

		d += 31 + 28;
		t = d / 365;
		d -= t * 365;
		y += t;
	}

	year	= y;
	ToMonthDay(d);
}

/*
JulianDate::JulianDate(int _d) {
	uint32	d = (bc = _d < 0) ? -_d : _d;
	dow		= d % 7;
	if (bc)
		dow = 7 - dow;

	uint32	y = 0;
	if (d >= 31 + 28) {
		uint32	t;
		d -= 31 + 28;
		t = d / (365 * 4 + 1);
		d -= t * (365 * 4 + 1);
		y += t * 4;
		if (d == 4 * 365) {
			year	= y + 4;
			month	= 2;
			day		= 29;
			return;
		}

		d += 31 + 28;
		t = d / 365;
		d -= t * 365;
		y += t;
	}

	year	= y;
	ToMonthDay(d);
}
*/
template<> const char *DateFormat<ShortDate>::months[12]	= {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
template<> const char *DateFormat<ShortDate>::days[7]		= {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

template<> const char *DateFormat<LongDate>::months[12]		= {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
template<> const char *DateFormat<LongDate>::days[7]		= {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
