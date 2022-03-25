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

class DateTime {
	friend DateTime operator+(DateTime a, DateTime b);//	{ return a.t + b.t; }
	friend DateTime operator-(DateTime a, DateTime b)	{ return a.t - b.t; }
	friend float	operator/(DateTime a, DateTime b)	{ int64 x = a.t / b.t, y = a.t % b.t; return x + float(y) / float(b.t); }
	friend bool		operator==(DateTime a, DateTime b)	{ return a.t == b.t; }
	friend bool		operator!=(DateTime a, DateTime b)	{ return a.t != b.t; }
	friend bool		operator< (DateTime a, DateTime b)	{ return a.t <  b.t; }
	friend bool		operator<=(DateTime a, DateTime b)	{ return a.t <= b.t; }
	friend bool		operator> (DateTime a, DateTime b)	{ return a.t >  b.t; }
	friend bool		operator>=(DateTime a, DateTime b)	{ return a.t >= b.t; }
	int64	t;
public:
	DateTime()					{}
	DateTime(int64 t) : t(t)	{}
	inline DateTime(int year, int day);
	inline DateTime(int year, int month, int day);

	template<typename T> static	DateTime uSecs(T t)	{ return int64(t);						}
	template<typename T> static	DateTime Secs(T t)	{ return int64(t * int64(1000000));		}
	template<typename T> static	DateTime Mins(T t)	{ return int64(t * int64(60000000));	}
	template<typename T> static	DateTime Hours(T t)	{ return int64(t * int64(3600000000));	}
	template<typename T> static	DateTime Days(T t)	{ return int64(t * (int64(86400) * 1000000)); }

	int64		uSecs()			const { return t;						}
	double		fSecs()			const { return t / 1000000.0;			}
	int64		Secs()			const { return t / int64(1000000);		}
	int64		Mins()			const { return t / int64(60000000);		}
	int64		Hours()			const { return t / int64(3600000000);	}
	int			Days()			const { return int(t / (int64(86400) * 1000000)); }
	float		TimeOfDay()		const { return (t % (int64(86400) * 1000000)) / 1000000.f; }

	DateTime&	operator+=(DateTime b)	{ t += b.t; return *this; }
	DateTime&	operator-=(DateTime b)	{ t -= b.t; return *this; }

	uint32		Read_ISO_8601(string_scan &s);

	static DateTime FromUnixTime(const DateTime &t) {
		static DateTime offset = DateTime(1970, 1, 1);
		return t + offset;
	}
	time_t			ToUnixTime() const {
		return (*this - FromUnixTime(0)).Secs();
	}

#ifdef _WINDEF_
	//PC & X360
	static DateTime TimeZone()	{
		struct Z : TIME_ZONE_INFORMATION {
			Z() { GetTimeZoneInformation(this); }
		};
		static DateTime zone = Mins(Z().Bias);
		return zone;
	}
	static DateTime Now(bool utc = false) {
		FILETIME ft;
		GetSystemTimeAsFileTime(&ft);
		return DateTime(ft);
	}
	DateTime(FILETIME ft, bool utc = false)	{
		static DateTime offset = DateTime(1601, 1, 1);
		if (!utc)
			FileTimeToLocalFileTime(&ft, &ft);
		t = (int64&)ft / 10 + offset.t;
	}
#elif defined(__COREFOUNDATION_CFTIMEZONE__)
	//MAC + IOS
	static DateTime TimeZone()	{
		struct Z {
			CFTimeInterval s;
			Z() { CFTimeZoneRef tz = CFTimeZoneCopySystem(); s = -CFTimeZoneGetSecondsFromGMT(tz, CFAbsoluteTimeGetCurrent()); CFRelease(tz); }
		};
		static DateTime zone = Secs(Z().s);
		return zone;
	}
	static DateTime Now(bool utc = false) {
		static DateTime offset = DateTime(2001, 1, 1);
		return Secs(CFAbsoluteTimeGetCurrent()) + (utc ? offset : (offset - TimeZone()));
	}
#elif defined(__SYS_SYS_SYS_TIME_H__) && defined(_CELL_SYSUTIL_SYSPARAM_H_)
	//PS3
	static DateTime TimeZone()	{
		struct Z {
			int v;
			Z() { cellSysutilGetSystemParamInt(CELL_SYSUTIL_SYSTEMPARAM_ID_TIMEZONE, &v); }
		};
		static DateTime zone = Mins(Z().v);
		return zone;
	}
	static DateTime Now(bool utc = false) {
		sys_time_sec_t	s;
		sys_time_nsec_t	ns;
		sys_time_get_current_time(&s, &ns);
		return UnixTime(Secs(s) + uSecs(ns / 1000)) - (utc ? 0 : TimeZone());
	}
#elif defined(_SYS_TIME_H_) && defined(PLATFORM_PS4)
	//PS4
	static DateTime Now(bool utc = false) {
		SceKernelTimeval	t;
		sceKernelGettimeofday(&t);
		return UnixTime(Secs(t.tv_sec) + uSecs(t.tv_usec));//(utc ? offset : offset - TimeZone());
	}
#endif

#ifdef _WINDEF_
	DateTime(const struct filetime_t &ft, bool utc = false) : DateTime(reinterpret_cast<const FILETIME&>(ft), utc) {}
#else
	DateTime(const struct filetime_t &ft) : DateTime(FromUnixTime(Secs((const int64&)ft))) {}
#endif

};

inline DateTime operator+(DateTime a, DateTime b)	{ return a.t + b.t; }


struct TimeOfDay {
	float	t;
	
	static bool	IsValid(int hour, int min, int sec) {
		return between(hour, 0, 24) && between(min, 0, 60) && between(sec, 0, 60);
	}

	TimeOfDay(float t) : t(t)	{}
	TimeOfDay(uint8 hour, uint8 min, float sec) : t((hour * 60 + min) * 60 + sec)	{}
	float		Sec()		const	{ return t - int(t / 60) * 60;		}
	uint8		Min()		const	{ return uint8((int(t) / 60) % 60);	}
	uint8		Hour()		const	{ return uint8(int(t) / 3600);		}
	operator	DateTime()	const	{ return int64(t * 1000000);		}
};

struct Date {
	uint32	dow:3, day:5, month:4, year:19, bc:1;
	void				ToMonthDay(int d);
	static int			FromMonthDay(int month, int day);
	static int			DaysInMonth(int month, int year);
	static bool			IsValid(int year, int month, int day) {
		return between(month, 1, 12) && between(day, 1, DaysInMonth(month, year));
	}

	static inline int	NumLeaps(int year)		{ return (year / 4) - (year / 100) + (year / 400); }
	static inline bool	IsLeap(int year)		{ return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0); }
	static inline int	Days(int year, int day) { return year * 365 + NumLeaps(year - 1) + day; }
	static inline int	Days(int year, int month, int day) {
		return Days(year, FromMonthDay(month, day)) + int(month > 2 && IsLeap(year));
	}

	Date()				{}
	Date(int d);
	int					Days()		const		{ return Days(bc ? -int(year) : year, month, day); }
	operator			DateTime()	const		{ return DateTime::Days(Days()); }
};

DateTime::DateTime(int year, int day)				{ t = Days(Date::Days(year, day)).t;		}
DateTime::DateTime(int year, int month, int day)	{ t = Days(Date::Days(year, month, day)).t;	}

struct ShortDate : Date {
	ShortDate(const Date &d): Date(d)	{}
	ShortDate(int d)		: Date(d)	{}
};

struct LongDate : Date {
	LongDate(const Date &d)	: Date(d)	{}
	LongDate(int d)			: Date(d)	{}
};

struct JulianDate : Date {
	static inline int	NumLeaps(int year)		{ return year / 4; }
	static inline bool	IsLeap(int year)		{ return year % 4 == 0; }
	static inline int	Days(int year, int day) { return year * 365 + NumLeaps(year - 1) + day; }
	static inline int	Days(int year, int month, int day) {
		return Days(year, FromMonthDay(month, day)) + int(month > 2 && IsLeap(year));
	}

	JulianDate(int d);
	int					Days()		const		{ return Days(bc ? -int(year) : year, month, day); }
	operator			DateTime()	const		{ return DateTime::Days(Days()); }
};

//-----------------------------------------------------------------------------

template<typename T> struct DateFormat {
	static const char *months[12], *days[7];
	static size_t to_string(char *s, const Date &d) {
		fixed_accum	a(s, 64);
		a << days[d.dow] << ", " << months[d.month-1] << ' ' << d.day << ", " << d.year;
		return a.getp() - s;
	}
};

template<typename T> const char *DateFormat<T>::months[12];
template<typename T> const char *DateFormat<T>::days[7];

template<> struct DateFormat<Date> : DateFormat<ShortDate> {};

template<typename C> inline size_t to_string(C *s, const Date &d)		{ return DateFormat<Date>::to_string(s, d); }
template<typename C> inline size_t to_string(C *s, const ShortDate &d)	{ return DateFormat<ShortDate>::to_string(s, d); }
template<typename C> inline size_t to_string(C *s, const LongDate &d)	{ return DateFormat<LongDate>::to_string(s, d); }

template<typename C> inline size_t to_string(C *s, const TimeOfDay &t) {
	fixed_accum	a(s, 64);
	int	i	= int(t.t);
	int	h	= i / (60 * 60);
	int	m	= (i / 60) % 60;
	int	x	= i % 60;
	a << ((h + 11) % 12 + 1) << ':' << rightjustify(2, '0') << m << terminate << ':' << rightjustify(2, '0') << x << terminate << (h < 12 ? " am" : " pm");
	return a.getp() - s;
}

template<typename C> inline size_t to_string(C *s, const DateTime &t) {
	size_t	n = to_string(s, Date(t.Days()));
	s[n] = ' ';
	return n + 1 + to_string(s + n + 1, TimeOfDay(t.TimeOfDay()));
}

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
	ISO_8601(const DateTime &d, uint32 flags = 0) : DateTime(d), flags(flags)	{}
};

template<typename C> inline size_t to_string(C *s, const ISO_8601 &t) {
	Date		date	= t.Days();
	TimeOfDay	time	= t.TimeOfDay();

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
