#ifndef DATE_H
#define DATE_H

#include "base/defs.h"
#include "base/strings.h"

#ifdef PLAT_MAC
#include <CoreFoundation/CFTimeZone.h>
#undef nil
#endif

namespace iso {

// microseconds from 00:00 1st Jan 1BC (year 0)
// +- 292471 years

class DateTime;
struct filetime_t;

class Duration {
	friend DateTime;
	int64	t;
public:
	Duration()					{}
	constexpr Duration(int64 t) : t(t)	{}

	//friend DateTime operator+(DateTime a, Duration b);
	friend constexpr Duration operator+(Duration a, Duration b)	{ return a.t + b.t; }
	friend constexpr float	operator/(Duration a, Duration b)	{ int64 x = a.t / b.t, y = a.t % b.t; return x + float(y) / float(b.t); }

	template<typename T> static	constexpr Duration uSecs(T t)	{ return int64(t);						}
	template<typename T> static	constexpr Duration Secs(T t)	{ return int64(t * int64(1000000));		}
	template<typename T> static	constexpr Duration Mins(T t)	{ return int64(t * int64(60000000));	}
	template<typename T> static	constexpr Duration Hours(T t)	{ return int64(t * int64(3600000000));	}
	template<typename T> static	constexpr Duration Days(T t)	{ return int64(t * (int64(86400) * 1000000)); }

	constexpr int64		uSecs()	const { return t;						}
	constexpr double	fSecs()	const { return t / 1000000.0;			}
	constexpr int64		Secs()	const { return t / int64(1000000);		}
	constexpr int64		Mins()	const { return t / int64(60000000);		}
	constexpr int64		Hours()	const { return t / int64(3600000000);	}
	constexpr int		Days()	const { return int(t / (int64(86400) * 1000000)); }
};

class DateTime {
	int64	t;
public:
	DateTime()					{}
	constexpr DateTime(int64 t) : t(t)	{}
	constexpr DateTime(int year, int day);
	constexpr DateTime(int year, int month, int day);

	template<typename T> static	constexpr DateTime Day(T t)	{ return int64(t * (int64(86400) * 1000000)); }

	constexpr int	Day()			const { return int(t / (int64(86400) * 1000000)); }
	constexpr float	TimeOfDay()		const { return (t % (int64(86400) * 1000000)) / 1000000.f; }

	DateTime&	operator+=(Duration b)	{ t += b.t; return *this; }
	DateTime&	operator-=(Duration b)	{ t -= b.t; return *this; }

	friend constexpr DateTime	operator+(DateTime a, Duration b)	{ return a.t + b.t; }
	friend constexpr DateTime	operator-(DateTime a, Duration b)	{ return a.t - b.t; }
	friend constexpr Duration	operator-(DateTime a, DateTime b)	{ return a.t - b.t; }
	friend constexpr bool		operator==(DateTime a, DateTime b)	{ return a.t == b.t; }
	friend constexpr bool		operator!=(DateTime a, DateTime b)	{ return a.t != b.t; }
	friend constexpr bool		operator< (DateTime a, DateTime b)	{ return a.t <  b.t; }
	friend constexpr bool		operator<=(DateTime a, DateTime b)	{ return a.t <= b.t; }
	friend constexpr bool		operator> (DateTime a, DateTime b)	{ return a.t >  b.t; }
	friend constexpr bool		operator>=(DateTime a, DateTime b)	{ return a.t >= b.t; }

	uint32		Read_ISO_8601(string_scan &s);

	static DateTime FromUnixTime(Duration t)	{ return DateTime(1970, 1, 1) + t; }
	time_t			ToUnixTime() const			{ return (*this - FromUnixTime(0)).Secs(); }

#ifdef _WINDEF_
	//PC & X360
	static DateTime FromFILETIME(Duration t)	{ return DateTime(1601, 1, 1) + t; }

	static auto TimeZone()	{
		struct Z : TIME_ZONE_INFORMATION {
			Z() { GetTimeZoneInformation(this); }
		};
		static auto zone = Duration::Mins(Z().Bias);
		return zone;
	}
	static DateTime Now(bool utc = false) {
		FILETIME ft;
		GetSystemTimeAsFileTime(&ft);
		return DateTime(ft, utc);
	}
	DateTime(FILETIME ft, bool utc = false)	{
		if (!utc)
			FileTimeToLocalFileTime(&ft, &ft);
		*this = FromFILETIME((const int64&)ft / 10);
	}
	operator FILETIME() const {
		return force_cast<FILETIME>((*this - FromFILETIME(0)).uSecs() * 10);
	}

#elif defined(__COREFOUNDATION_CFTIMEZONE__)
	//MAC + IOS
	static auto TimeZone()	{
		struct Z {
			CFTimeInterval s;
			Z() { CFTimeZoneRef tz = CFTimeZoneCopySystem(); s = -CFTimeZoneGetSecondsFromGMT(tz, CFAbsoluteTimeGetCurrent()); CFRelease(tz); }
		};
		static auto zone = Duration::Secs(Z().s);
		return zone;
	}
	static DateTime Now(bool utc = false) {
		return Secs(CFAbsoluteTimeGetCurrent()) + DateTime(2001, 1, 1) - (utc ? 0 : TimeZone());
	}
#elif defined(__SYS_SYS_SYS_TIME_H__) && defined(_CELL_SYSUTIL_SYSPARAM_H_)
	//PS3
	static auto TimeZone()	{
		struct Z {
			int v;
			Z() { cellSysutilGetSystemParamInt(CELL_SYSUTIL_SYSTEMPARAM_ID_TIMEZONE, &v); }
		};
		static auto zone = Duration::Mins(Z().v);
		return zone;
	}
	static DateTime Now(bool utc = false) {
		sys_time_sec_t	s;
		sys_time_nsec_t	ns;
		sys_time_get_current_time(&s, &ns);
		return UnixTime(Duration::Secs(s) + Duration::uSecs(ns / 1000)) - (utc ? 0 : TimeZone());
	}
#elif defined(_SYS_TIME_H_) && defined(PLATFORM_PS4)
	//PS4
	static DateTime Now(bool utc = false) {
		SceKernelTimeval	t;
		sceKernelGettimeofday(&t);
		return UnixTime(Duration::Secs(t.tv_sec) + Duration::uSecs(t.tv_usec));//(utc ? offset : offset - TimeZone());
	}
#endif

#ifdef _WINDEF_
	DateTime(const filetime_t &ft, bool utc = false) : DateTime(reinterpret_cast<const FILETIME&>(ft), utc) {}
#else
	DateTime(const filetime_t &ft) : DateTime(FromUnixTime(Secs((const int64&)ft))) {}
	operator filetime_t() const { return force_cast<filetime_t>(ToUnixTime()); }
#endif
};

struct TimeOfDay {
	float	t;
	
	static constexpr bool	IsValid(int hour, int min, int sec) {
		return between(hour, 0, 24) && between(min, 0, 60) && between(sec, 0, 60);
	}

	explicit TimeOfDay(float t) : t(t)	{}
	constexpr TimeOfDay(uint8 hour, uint8 min, float sec) : t((hour * 60 + min) * 60 + sec)	{}
	constexpr float		Sec()		const	{ return t - int(t / 60) * 60;		}
	constexpr uint8		Min()		const	{ return uint8((int(t) / 60) % 60);	}
	constexpr uint8		Hour()		const	{ return uint8(int(t) / 3600);		}
	constexpr operator	Duration()	const	{ return int64(t * 1000000);		}
};

struct Date {
	uint32	dow:3, day:5, month:4, year:19, bc:1;
	void					ToMonthDay(int d);
	static constexpr int	FromMonthDay(int month, int day)	{ return meta::make_array0<uint16>(0,31,59,90,120,151,181,212,243,273,304,334)[month - 1] + day - 1;}
	static int				DaysInMonth(int month, int year);
	static constexpr bool	IsValid(int year, int month, int day) {
		return between(month, 1, 12) && between(day, 1, DaysInMonth(month, year));
	}

	static constexpr int	NumLeaps(int year)		{ return (year / 4) - (year / 100) + (year / 400); }
	static constexpr bool	IsLeap(int year)		{ return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0); }
	static constexpr int	Days(int year, int day) { return year * 365 + NumLeaps(year - 1) + day; }
	static constexpr int	Days(int year, int month, int day) {
		return Days(year, FromMonthDay(month, day)) + int(month > 2 && IsLeap(year));
	}

	Date()				{}
	explicit Date(int d);
	constexpr int			Days()		const		{ return Days(bc ? -int(year) : year, month, day); }
	constexpr operator		DateTime()	const		{ return DateTime::Day(Days()); }
};

constexpr DateTime::DateTime(int year, int day)				: DateTime(Day(Date::Days(year, day)).t)		{}
constexpr DateTime::DateTime(int year, int month, int day)	: DateTime(Day(Date::Days(year, month, day)).t)	{}

struct ShortDate : Date {
	ShortDate(const Date &d): Date(d)	{}
	ShortDate(int d)		: Date(d)	{}
};

struct LongDate : Date {
	LongDate(const Date &d)	: Date(d)	{}
	LongDate(int d)			: Date(d)	{}
};

struct JulianDate : Date {
	static constexpr int	NumLeaps(int year)		{ return year / 4; }
	static constexpr bool	IsLeap(int year)		{ return year % 4 == 0; }
	static constexpr int	Days(int year, int day) { return year * 365 + NumLeaps(year - 1) + day; }
	static constexpr int	Days(int year, int month, int day) {
		return Days(year, FromMonthDay(month, day)) + int(month > 2 && IsLeap(year));
	}

	JulianDate(int d);
	constexpr int		Days()		const		{ return Days(bc ? -int(year) : year, month, day); }
	constexpr operator	DateTime()	const		{ return DateTime::Day(Days()); }
};

//-----------------------------------------------------------------------------

template<typename T> struct DateFormat {
	static const char *months[12], *days[7];
	template<typename C> static size_t to_string(C *s, const Date &d) {
		fixed_accumT<C>	a(s, 64);
		a << days[d.dow] << ", " << months[d.month-1] << ' ' << d.day << ", " << d.year;
		return a.getp() - s;
	}
};

template<> struct DateFormat<Date> : DateFormat<ShortDate> {};

template<typename C> inline size_t to_string(C *s, const Date &d)		{ return DateFormat<Date>::to_string(s, d); }
template<typename C> inline size_t to_string(C *s, const ShortDate &d)	{ return DateFormat<ShortDate>::to_string(s, d); }
template<typename C> inline size_t to_string(C *s, const LongDate &d)	{ return DateFormat<LongDate>::to_string(s, d); }

template<typename C> inline size_t to_string(C *s, const TimeOfDay &t) {
	fixed_accumT<C>	a(s, 64);
	int	i	= int(t.t), h = i / (60 * 60), m = (i / 60) % 60, x = i % 60;
	a << ((h + 11) % 12 + 1) << ':' << formatted(m, FORMAT::ZEROES, 2) << ':' << formatted(x, FORMAT::ZEROES, 2) << (h < 12 ? " am" : " pm");
	return a.getp() - s;
}

template<typename C> inline size_t to_string(C *s, const DateTime &t) {
	size_t	n = to_string(s, Date(t.Day()));
	s[n] = ' ';
	return n + 1 + to_string(s + n + 1, TimeOfDay(t.TimeOfDay()));
}

string_accum& operator<<(string_accum &a, Duration t);

class ISO_8601 : public DateTime {
public:
	enum {
		SECOND,
		MINUTE,
		HOUR,
		DAY,
		WEEK,
		MONTH,
		YEAR,
		CENTURY,
		TIMEZONE	= 1 << 3,
		MINIMAL		= 1 << 4,
	};
	uint32	flags;
	ISO_8601()					: DateTime(0), flags(0)	{}
	ISO_8601(string_scan &s)	: flags(Read_ISO_8601(s)) {}
	ISO_8601(const char *s)		: ISO_8601(lvalue(string_scan(s))) {}
	ISO_8601(const DateTime &d, uint32 flags = 0) : DateTime(d), flags(flags)	{}
};

template<typename C> inline size_t to_string(C *s, const ISO_8601 &t) {
	Date		date(t.Day());
	TimeOfDay	time(t.TimeOfDay());

	return _format(s,
		t.flags & ISO_8601::MINIMAL ? "%04i%02i%02iT%02i%02i%02i" : "%04i-%02i-%02iT%02i:%02i:%02i",
		date.year, date.month, date.day,
		time.Hour(), time.Min(), int(time.Sec())
	);
}

inline fixed_string<64> to_string(const Date &d)		{ return _to_string<64>(d); }
inline fixed_string<64> to_string(const TimeOfDay &t)	{ return _to_string<64>(t); }
inline fixed_string<64> to_string(const DateTime &t)	{ return _to_string<64>(t); }
inline fixed_string<64> to_string(const ISO_8601 &t)	{ return _to_string<64>(t); }

inline string_scan& operator>>(string_scan &a, ISO_8601 &t)	{ t.flags = t.Read_ISO_8601(a); return a; }

//inline size_t	from_string(const char *s, ISO_8601 &t)	{ return t = DateTime::Read_ISO_8601(move(string_scan(s))); }

} // namespace iso
#endif	//DATE_H
