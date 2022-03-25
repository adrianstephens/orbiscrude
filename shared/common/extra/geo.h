#ifndef GEO_H
#define GEO_H

#include "base/defs.h"
#include "base/strings.h"
#include "base/hash.h"

namespace iso {

/*
Reference Ellipsoids:
Peter H. Dana (pdana@mail.utexas.edu)
http://www.colorado.edu/geography/gcraft/notes/datum/edlist.html
Defense Mapping Agency. 1987b. DMA Technical Report: Supplement to Department of Defense World Geodetic System
1984 Technical Report. Part I and II. Washington, DC: Defense Mapping Agency
Long/Lat to UTM Equations:
Chuck Gantz (chuck.gantz@globalstar.com)
USGS Bulletin 1532
Datum Transformation Equations:
Peter H. Dana website (pdana@mail.utexas.edu)
http://www.colorado.edu/geography/gcraft/notes/datum/datum.html
*/

//----------------------------------------------------------------------
//	RefEllipse, RefEllipsLink
//----------------------------------------------------------------------
class RefEllipse : public static_hash<RefEllipse, const char*> {
	string				name;
public:
	double				r;
	double				e2;

	RefEllipse(const char *_name, double _r, double _e2) : base(_name), name(_name), r(_r), e2(_e2)	{}
//	bool operator==(const char *_name) const { return name == _name; }
};

//----------------------------------------------------------------------
//	ECEF (Earth centered, Earth fixed X,Y,Z)
//----------------------------------------------------------------------
class ECEF {
	double	x, y, z;
public:
	ECEF()														{}
	ECEF(double _x, double _y, double _z) : x(_x), y(_y), z(_z)	{}

	bool	operator==(const ECEF &v)	const	{ return x == v.x && y == v.y && z == v.z; }
	ECEF	operator-(const ECEF &v)	const	{ return ECEF(x - v.x, y - v.y, z - v.z); }
	ECEF	operator+(const ECEF &v)	const	{ return ECEF(x + v.x, y + v.y, z + v.z); }
	ECEF	operator*(float c)			const	{ return ECEF(x * c, y * c, z * c); }

	class LLA	to_LLA(const RefEllipse *re) const;

	friend ECEF	max(const ECEF &v0, const ECEF &v1)	{ return ECEF(max(v0.x, v1.x), max(v0.y, v1.y), max(v0.z, v1.z)); }
	friend ECEF	min(const ECEF &v0, const ECEF &v1)	{ return ECEF(min(v0.x, v1.x), min(v0.y, v1.y), min(v0.z, v1.z)); }
};

//----------------------------------------------------------------------
//	GeoDatum
//----------------------------------------------------------------------
class GeoDatum : public static_list<GeoDatum> {
	string				name;
public:
	const RefEllipse	*re;
	ECEF				d;				// datum transformation delta to WGS 84

	GeoDatum(const char *_name, const RefEllipse *_re, const ECEF &_d) : name(_name), re(_re), d(_d)	{}
	bool operator==(const char *_name) const { return name == _name; }
};

//----------------------------------------------------------------------
//	LLA (Longitude, Latitude)
//----------------------------------------------------------------------
class LLA {
	double	l, la;
public:
	LLA()										{}
	LLA(double _l, double _la) : l(_l), la(_la)	{}

	bool	operator==(const LLA &v)	const	{ return v.l == l && v.la == la; }
	LLA		operator-(const LLA &v)		const	{ return LLA(l - v.l, la - v.la); }
	LLA		operator+(const LLA &v)		const	{ return LLA(l + v.l, la + v.la); }
	LLA		operator*(float c)			const	{ return LLA(l * c, la * c); }

	class UTM	to_UTM(const RefEllipse *re)	const;
	class ECEF	to_ECEF(const RefEllipse *re)	const;

	LLA			to_LLA(const GeoDatum *d1, const GeoDatum *d2) const	{ return d1 == d2 ? *this : (to_ECEF(d1->re) + d1->d - d2->d).to_LLA(d2->re); }
	UTM			to_UTM(const GeoDatum *d1, const GeoDatum *d2) const;

	friend LLA	max(const LLA &v0, const LLA &v1)	{ return LLA(max(v0.l, v1.l), max(v0.la, v1.la)); }
	friend LLA	min(const LLA &v0, const LLA &v1)	{ return LLA(min(v0.l, v1.l), min(v0.la, v1.la)); }
};


//----------------------------------------------------------------------
//	UTM (Universal Transverse Mercator)
//----------------------------------------------------------------------
class UTM {
public:
	double	x, y;
	int		zone;
public:
	UTM()																{}
	UTM(double _x, double _y, int _zone) : x(_x), y(_y), zone(_zone)	{}

	bool	operator==(const UTM &v)	const	{ return x == v.x && y == v.y && zone == v.zone; }
	UTM		operator-(const UTM &v)		const	{ return UTM(x - v.x, y - v.y, zone); }
	UTM		operator+(const UTM &v)		const	{ return UTM(x + v.x, y + v.y, zone); }
	UTM		operator*(float c)			const	{ return UTM(x * c, y * c, zone); }

	class LLA	to_LLA(const RefEllipse *re)	const;
	inline UTM	to_UTM(const GeoDatum *d1, const GeoDatum *d2)	const	{ return d1 == d2 ? *this : to_LLA(d1->re).to_LLA(d1, d2).to_UTM(d2->re); }
	inline LLA	to_LLA(const GeoDatum *d1, const GeoDatum *d2)	const	{ return to_LLA(d1->re).to_LLA(d1, d2); }

	friend UTM	max(const UTM &v0, const UTM &v1)	{ return UTM(max(v0.x, v1.x), max(v0.y, v1.y), v0.zone); }
	friend UTM	min(const UTM &v0, const UTM &v1)	{ return UTM(min(v0.x, v1.x), min(v0.y, v1.y), v0.zone); }
};

inline UTM LLA::to_UTM(const GeoDatum *d1, const GeoDatum *d2) const	{ return to_LLA(d1, d2).to_UTM(d2->re); }


//----------------------------------------------------------------------
//	GeoConvert
//----------------------------------------------------------------------


} // namespace iso

#endif // GEO_H