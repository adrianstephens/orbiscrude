#include "geo.h"
#include "base/maths.h"

using namespace iso;

//----------------------------------------------------------------------
//	RefEllipsLink
//----------------------------------------------------------------------
RefEllipse relist[] = {
	RefEllipse("Airy",					6377563,	0.00667054	),
	RefEllipse("Australian National",	6378160,	0.006694542	),
	RefEllipse("Bessel 1841",			6377397,	0.006674372	),
	RefEllipse("Bessel 1841 (Nambia)",	6377484,	0.006674372	),
	RefEllipse("Clarke 1866",			6378206,	0.006768658	),
	RefEllipse("Clarke 1880",			6378249,	0.006803511	),
	RefEllipse("Everest",				6377276,	0.006637847	),
	RefEllipse("Fischer 1960 (Mercury)",6378166,	0.006693422	),
	RefEllipse("Fischer 1968",			6378150,	0.006693422	),
	RefEllipse("GRS 1967",				6378160,	0.006694605	),
	RefEllipse("GRS 1980",				6378137,	0.00669438	),
	RefEllipse("Helmert 1906",			6378200,	0.006693422	),
	RefEllipse("Hough",					6378270,	0.00672267	),
	RefEllipse("International",			6378388,	0.00672267	),
	RefEllipse("Krassovsky",			6378245,	0.006693422	),
	RefEllipse("Modified Airy",			6377340,	0.00667054	),
	RefEllipse("Modified Everest",		6377304,	0.006637847	),
	RefEllipse("Modified Fischer 1960",	6378155,	0.006693422	),
	RefEllipse("South American 1969",	6378160,	0.006694542	),
	RefEllipse("WGS 60",				6378165,	0.006693422	),
	RefEllipse("WGS 66",				6378145,	0.006694542	),
	RefEllipse("WGS 72",				6378135,	0.006694318	),
	RefEllipse("WGS 84",				6378137,	0.00669438	)
};

//----------------------------------------------------------------------
//	GeoDatumLink
//----------------------------------------------------------------------
GeoDatum gdlist[] = {
	GeoDatum("North American 1927 (CONUS)",			RefEllipse::get("Clarke 1866"),	ECEF(-8, 160, 176)),
	GeoDatum("North American 1983 (CONUS)",			RefEllipse::get("GRS 1980"),	ECEF(0, 0, 0)),
	GeoDatum("World Geodetic System 1984 (Global)",	RefEllipse::get("WGS 84"),		ECEF(0, 0, 0))
};

//----------------------------------------------------------------------
//	GeoConvert
//----------------------------------------------------------------------
UTM LLA::to_UTM(const RefEllipse *re) const {
	double	a			= re->r;
	double	eccSquared	= re->e2;
	double	k0			= 0.9996;

	// make sure the longitude is between -180.00 .. 179.9
	double	LongTemp	= (l + 180) - int((l + 180) / 360) * 360 - 180;

	double	Lat			= degrees(la);
	double	Long		= degrees(LongTemp);
	int		ZoneNumber	= int((LongTemp + 180) / 6) + 1;

	if (la >= 56.0 && la < 64.0 && LongTemp >= 3.0 && LongTemp < 12.0)
		ZoneNumber = 32;

	// Special zones for Svalbard
	if (la >= 72.0 && la < 84.0) {
		if (LongTemp >= 0.0  && LongTemp < 9.0)
			ZoneNumber = 31;
		else if (LongTemp >= 9.0  && LongTemp < 21.0)
			ZoneNumber = 33;
		else if (LongTemp >= 21.0 && LongTemp < 33.0)
			ZoneNumber = 35;
		else if (LongTemp >= 33.0 && LongTemp < 42.0)
			ZoneNumber = 37;
	}
	double	LongOrigin		= degrees((ZoneNumber - 1) * 6 - 180 + 3); //+3 puts origin in middle of zone
	double	eccPrimeSquared	= (eccSquared) / (1 - eccSquared);

	double	N	= a / sqrt(1 - eccSquared * square(sin(Lat)));
	double	T	= square(tan(Lat));
	double	C	= eccPrimeSquared * square(cos(Lat));
	double	A	= cos(Lat) * (Long - LongOrigin);

	double	M	= a*((1 - eccSquared / 4 - 3 * square(eccSquared) / 64 - 5 * cube(eccSquared) / 256) * Lat
		- (3 * eccSquared / 8 + 3 * square(eccSquared) / 32 + 45 * cube(eccSquared) / 1024) * sin(2 * Lat)
		+ (15 * square(eccSquared) / 256 + 45 * cube(eccSquared) / 1024) * sin(4 * Lat)
		- (35 * cube(eccSquared) / 3072) * sin(6 * Lat));

	double x	= k0 * N * (A + (1 - T + C) * A * A * A / 6 + (5 - 18 * T + T * T + 72 * C - 58 * eccPrimeSquared) * A * A * A * A * A / 120);
	double y	= k0 * (M + N * tan(Lat) * (square(A) / 2 + (5 - T + 9 * C + 4 * C * C) * A * A * A * A / 24 + (61 - 58 * T + square(T) + 600 * C - 330 * eccPrimeSquared) * A * A * A * A * A * A / 720));

	// 10000000 meter offset for southern hemisphere
	return UTM(x + 500000.0, la < 0 ? y + 10000000.0 : y, la < 0 ? -ZoneNumber : ZoneNumber);
}

LLA UTM::to_LLA(const RefEllipse *re) const {
	double	a = re->r;
	double	eccSquared = re->e2;
	double	k0 = 0.9996;

	double	LongOrigin		= (abs(zone) - 1) * 6 - 180 + 3;  //+3 puts origin in middle of zone
	double	eccPrimeSquared = eccSquared / (1 - eccSquared);

	// if point is in southern hemisphere, remove 10,000,000 meter offset used for southern hemisphere
	double	mu = ((zone < 0 ? y - 10000000.0 : y) / k0) / (a * (1 - eccSquared / 4 - 3 * square(eccSquared) / 64 - 5 * cube(eccSquared) / 256));

	double	e1	= (1 - sqrt(1 - eccSquared)) / (1 + sqrt(1 - eccSquared));
	double	phi1 = mu + (3 * e1 / 2 - 27 * cube(e1) / 32)*sin(2 * mu) + e1 * (21 * e1 / 16 - 55 * cube(e1) / 32) * sin(4 * mu) + (151 * cube(e1) / 96) * sin(6 * mu);

	double	N1	= a / sqrt(1 - eccSquared * square(sin(phi1)));
	double	T1	= square(tan(phi1));
	double	C1	= eccPrimeSquared * cos(phi1) * cos(phi1);
	double	R1	= a * (1 - eccSquared) / pow(1 - eccSquared * square(sin(phi1)), 1.5);
	double	D	= (x - 500000.0) / (N1 * k0);	// remove 500,000 meter offset for longitude

	double la	= phi1 - (N1 * tan(phi1) / R1) * (square(D) / 2 - (5 + 3 * T1 + 10 * C1 - 4 * square(C1) - 9 * eccPrimeSquared) * D * D * D * D / 24 + (61 + 90 * T1 + 298 * C1 + 45 * square(T1) - 252 * eccPrimeSquared - 3 * square(C1)) * D * D * D * D * D * D / 720);
	double l	= (D - (1 + 2 * T1 + C1) * cube(D) / 6 + (5 - 2 * C1 + 28 * T1 - 3 * square(C1) + 8 * eccPrimeSquared + 24 * square(T1)) * D * D * D * D * D / 120) / cos(phi1);

	return LLA(LongOrigin + to_degrees(l), to_degrees(la));
}

ECEF LLA::to_ECEF(const RefEllipse *re) const {
	double	l	= degrees(this->l);
	double	la	= degrees(this->la);
	double	e2	= re->e2;
	double	a	= re->r;
	double	sla	= sin(la), cla = cos(la);
	double	n	= a / sqrt(1 - e2 * square(sla));
	return ECEF(n * cla * cos(l), n * cla * sin(l), n * (1 - e2) * sla);
}

LLA ECEF::to_LLA(const RefEllipse *re) const {
	double	e2	= re->e2,
			a	= re->r,
			b	= sqrt(square(a) * (1 - e2));

	double	p	= sqrt(x * x + y * y),
			k	= atan((z * a) / (p * b));

	return LLA(to_degrees(atan2(y, x)), to_degrees(atan((z + (square(a) / square(b) - 1) * b * cube(sin(k))) / (p - e2 * a * cube(cos(k))))));
}
