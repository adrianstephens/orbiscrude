#include "iso/iso_binary.h"
#include "iso/iso_files.h"
#include "comms/http.h"
#include "comms/upnp.h"
#include "hashes/md5.h"
#include "extra/date.h"
#include "extra/json.h"
#include "extra/random.h"
#include "thread.h"
#include "jobs.h"

#include "main.h"
#include "device.h"
#include "base/vector.h"

using namespace app;

//{
//    "state": {
//        "on": false,                 // true if the light is on, false if off
//        "bri": 240,                  // brightness between 0-254 (NB 0 is not off!)
//        "hue": 15331,                // hs mode: the hue (expressed in ~deg*182) - see note below
//        "sat": 121,                  // hs mode: saturation between 0-254
//        "xy": [0.4448, 0.4066],      // xy mode: CIE 1931 colour co-ordinates
//        "ct": 343,                   // ct mode: colour temp (expressed in mireds range 154-500)
//        "alert": "none",             // 'select' flash the lamp once, 'lselect' repeat flash
//        "effect": "none",            // not sure what this does yet
//        "colormode": "ct",           // the current colour mode (see above)
//        "reachable": true            // whether or not the lamp can be seen by the hub
//    },
//    "type": "Extended color light",  // type of lamp (all "Extended colour light" for now)
//    "name": "Hue Lamp 1",            // the name as set through the web UI or app
//    "modelid": "LCT001",             // the model number of the lamp (all are LCT001)
//    "swversion": "65003148",         // the software version of the lamp
//    "pointsymbol": { }               // 40 hex digits x 8;not sure what this does yet
//    }
//}

//CHANGING BULBS:
//"transitiontime":1800,
//{"bri": 254, "on": true}             // turn lamp on at full brightness
//{"hue": 25000, "sat": 254}           // hue 125°, saturation 100%
//{"ct": 500, "bri": 254}              // warm white, full brightness

//COLOUR MODES
//hue/sat:	The 'hue' parameter has the range 0-65535 so represents approximately 182*degrees
//xy:		CIE 1931 space
//ct:		colour temperature (white only) in Mireds, equivalent to 1000000/Kelvin corresponding to around 6500K (154) to 2000K (500)

//NEW CLIENT ASSOCIATION WITH BASE STATION
//GET /api/0123456789abdcef0123456789abcdef/lights
//while error {
//	POST /api		{"username":"0123456789abdcef0123456789abcdef","devicetype":"iPhone 5"}
//}

//SHOW BULB AND BASE STATION CONFIGURATION
//GET /api/0123456789abdcef0123456789abcdef

//SHOW BASE STATION CONFIGURATION
//GET /api/0123456789abdcef0123456789abcdef/config

//SHOW BULB NAMES
//GET /api/0123456789abdcef0123456789abcdef/lights

//SHOW BULB GROUPS
//GET /api/0123456789abdcef0123456789abcdef/groups/<group>

//SHOW SCHEDULES
//GET /api/0123456789abdcef0123456789abcdef/schedules

//CHANGE NAME OF A BULB
//PUT /api/0123456789abdcef0123456789abcdef/lights/<bulb>			{"name":"Dining"}

//CHANGE STATE OF A BULB
//PUT /api/0123456789abdcef0123456789abcdef/lights/<bulb>/state		{"bri":230,"xy":[0.63531,0.34127],"on":true}

//CHANGE STATE OF A BULB (BY GROUP)
//PUT /api/0123456789abdcef0123456789abcdef/groups/<group>/action	{"on":false}

//ADD A NEW SCHEDULE
//POST /api/0123456789abdcef0123456789abcdef/schedules
//{
//  "name":"Timer on 807548               ",
//  "time":"2012-11-30T18:57:02",
//  "description":" ",
//  "command": {
//    "method":"PUT",
//    "address":"/api/0123456789abdcef0123456789abcdef/lights/1/state",
//    "body": {
//      "bri":144,
//      "ct":469,
//      "transitiontime":1800,
//      "on":true
//    }
//  }
//}

//DELETE A SCHEDULE
//DELETE /api/0123456789abdcef0123456789abcdef/schedules/1

//SCAN FOR NEW BULBS
//This initiates two distinct requests: first an empty POST request to start the search and subsequent GET requests to get search progress.
//POST /api/0123456789abdcef0123456789abcdef/lights
//GET /api/0123456789abdcef0123456789abcdef/lights/new

//ENABLE ALERT MODE FOR A BULB (makes a light repeatedly turn on and off)
//PUT /api/0123456789abdcef0123456789abcdef/lights/0/state		{"alert":"lselect"} (or select for single flash)

//REMOVE A USERNAME
//DELETE /api/:username/config/whitelist/<username>

// find bridge: http://www.meethue.com/api/nupnp
//[{"id":"001788fffe09745c","internalipaddress":"192.168.2.129","macaddress":"00:17:88:09:74:5c"}]

// Send the string "[Link,Touchlink]" followed by a linefeed to TCP port 30000 of the bridge
// On success the reply of the bridge should contain the string "success"
// with no unassociated bulbs near gives "[Link,Touchlink,failed]"
// Pressing the link button sends out "[Link,Allow,0]" and after a while "[Link,Disallow,0]" on port 30000.

#if 0
"POST /api HTTP/1.1"
"Accept: */*"
"Referer: http://192.168.2.152/debug/clip.html"
"Accept-Language: en-US,en-GB;q=0.7,en;q=0.3"
"Content-Type: text/plain;charset=UTF-8"
"Accept-Encoding: gzip, deflate"
"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/51.0.2704.79 Safari/537.36 Edge/14.14393"
"Host: 192.168.2.152"
"Content-Length: 40"
"Connection: Keep-Alive"
"Cache-Control: no-cache"
#endif

HTTP::Context hue_context("HUE"
	, "connection", "keep-alive"
);

class HUEColour {
	enum MODE {UNKNOWN, HSV, CIE, CT} mode;
	float	bri;
	float	c0, c1;

	HUEColour(MODE _mode, float _bri, float _c0, float _c1) : mode(_mode), bri(clamp(_bri, 0, 1)), c0(_c0), c1(_c1)	{}
public:
	HUEColour() : mode(UNKNOWN), bri(0), c0(0), c1(0)	{}
	void				JSON(string_accum &a) const {
		a << "\"bri\": " << int(bri * 254);
		switch (mode) {
			case HSV:	a << ", \"hue\": " << int(c0 * 65536) << ", \"sat\": " << int(c1 * 254); break;
			case CIE:	a << ", \"xy\": [" << c0 << ", " << c1 << "]"; break;
			case CT:	a << ", \"ct\": " << int(c0); break;
			default:	break;
		}
	}
	static HUEColour	HueSat(float h, float s, float v) {
		return HUEColour(HSV, v, clamp(h, 0, 1), clamp(s, 0, 1));
	}
	static HUEColour	CIE1931(float x, float y, float Y) {
		return HUEColour(CIE, Y, clamp(x, 0, 1), clamp(y, 0, 1));
	}
	static HUEColour	Temperature(float K, float v) {
		return HUEColour(CT, v, clamp(1000000/ K, 154, 500), 0);
	}
	static HUEColour	Brightness(float v) {
		return HUEColour(UNKNOWN, v, 0, 0);
	}
};

struct HUETransiton {
	float	t;
	HUETransiton(float _t) : t(_t) {}
	void	JSON(string_accum &a) const {
		a << "\"on\": " << !iorf(t).get_sign() << ", \"transitiontime\": " << abs(int(t * 10));
	}
};

struct HUELightArray {
	uint32 lights;
	HUELightArray(uint32 _lights) : lights(_lights) {}
	void	JSON(string_accum &a) const {
		a << "\"lights\": [";
		for (uint32 i = 0, t = lights; t; ++i, t >>= 1) {
			if (t & 1) {
				a << "\"" << i + 1 << "\"";
				if (t > 1)
					a << ", ";
			}
		}
		a << ']';
	}
};

string_accum &operator<<(string_accum &a, const HUEColour &h)		{ h.JSON(a); return a; }
string_accum &operator<<(string_accum &a, const HUETransiton &h)	{ h.JSON(a); return a; }
string_accum &operator<<(string_accum &a, const HUELightArray &h)	{ h.JSON(a); return a; }

struct HUESchedule {
	int					light;
	fixed_string<32>	name;
	fixed_string<32>	desc;
	ISO_8601			time;
	HUEColour			col;
	float				trans;	// -ve for off

	HUESchedule(int _light, const char *_name, const char *_desc, DateTime _time, const HUEColour &_col, float _trans)
		: light(_light), name(_name), desc(_desc), time(_time), col(_col), trans(_trans) {}

	void				JSON(string_accum &a, const char *user) const {
		a <<
		"{\n"
		"  \"name\": \""		<< name << "\",\n"
		"  \"time\": \""		<< time << "\",\n"
		"  \"description\": \"" << desc << "\",\n"
		"  \"command\": {\n"
		"    \"method\": \"PUT\",\n"
		"    \"address\": \"api/" << user << "/lights/" << light << "/state\",\n"
		"    \"body\": {\n" << col << ", \"transitiontime\": " << abs(int(trans * 10)) << ", \"on\"" << (trans >= 0) << "}\n"
		"  }\n"
		"}\n";
	}
};

class HUE : public HTTP {
	string		user;
	FileHandler	*json;

public:
	SOCKET			ASyncSend(SOCKET sock, const char *verb, const char *state, const char *data = 0) const {
		buffer_accum<256>	ba;
		ba << "api/" << user;
		if (state)
			ba << '/' << state;
		return HTTP::Request(sock, verb, ba.term(), (const char*)0, data);
	}
	ISO_ptr<void>	Read(tag2 id, SOCKET sock) const {
		return json->Read(id, unconst(HTTPinput(sock)));
	}
	ISO_ptr<void>	Send(tag2 id, const char *verb, const char *state, string_ref data = 0) const {
		return Read(id, ASyncSend(Connect(), verb, state, data));
	}

	HUE(const char *host, string &&_user) : HTTP(hue_context, buffer_accum<256>("http://") << host << "/api"), user(_user) {
		json	= FileHandler::Get("json");
	}
	bool	Authorise(const char *name) {
		ISO_ptr<void>	ret = json->Read("get", unconst(HTTPinput(HTTP::Request("POST", "api",
			"Content-Type: application/jsonrequest\n",
			(buffer_accum<256>("{\"devicetype\": \"") << name << "\"}").data()
		))));
		if (ISO::Browser(ret)[0][0].GetName() == "success") {
			user = ISO::Browser(ret)[0][0]["username"].GetString();
			return true;
		}
		return false;
	}

	const string&	User() const { return user; }

	string			Touchlink()				const {
		SOCKET	sock = socket_address::TCP().connect(host, 30000);
		const char *s = "[Link,Touchlink]\r\n";
		if (send(sock, s, string_len32(s), 0) == string_len32(s)) {
			char	temp[1024];
			auto	n = recv(sock, temp, 1024, 0);
			if (n > 0) {
				n = recv(sock, temp, 1024, 0);
				return string(str(temp, n));
			}
		}
		return string();
	}

// status
	ISO_ptr<void>	GetState()				const { return Send("state",	"GET", NULL); }
	ISO_ptr<void>	GetConfig()				const { return Send("config",	"GET", "config"); }
	ISO_ptr<void>	GetLightNames()			const { return Send("lights",	"GET", "lights"); }
	ISO_ptr<void>	GetLightState(int i)	const { return Send("light",	"GET", "lights/" + to_string(i)); }
	ISO_ptr<void>	GetGroupState(int i)	const { return Send("groups",	"GET", "groups/" + to_string(i)); }
	ISO_ptr<void>	GetSchedules()			const { return Send("schedules","GET", "schedules"); }

// individual lights
	ISO_ptr<void>	SetLightName(int i, const char *name) const {
		return Send("result", "lights/" + to_string(i), "PUT",
			buffer_accum<256>("{\"name\": \"") << name << "\"}"
		);
	}

	SOCKET	_SetLightState(SOCKET sock, int i, string_ref data) const {
		return ASyncSend(sock, "PUT", "lights/" + to_string(i) + "/state", data);
	}
	SOCKET	SetLightState(SOCKET sock, int i, bool on)								const { return _SetLightState(sock, i, buffer_accum<256>() << "{\"on\": " << on << '}'); }
	SOCKET	SetLightState(SOCKET sock, int i, float trans)							const { return _SetLightState(sock, i, buffer_accum<256>() << '{' << HUETransiton(trans) << '}'); }
	SOCKET	SetLightState(SOCKET sock, int i, const HUEColour &h, bool on = true)	const { return _SetLightState(sock, i, buffer_accum<256>() << '{' << h << ", \"on\": " << on << '}'); }
	SOCKET	SetLightState(SOCKET sock, int i, const HUEColour &h, float trans)		const { return _SetLightState(sock, i, buffer_accum<256>() << '{' << h << ", " << HUETransiton(trans) << '}'); }
	SOCKET	SetLightAlert(SOCKET sock, int i, int alert)							const { static const char *alerts[] = {"off", "select", "lselect"}; return _SetLightState(sock, i, buffer_accum<256>() << "{\"alert\": " << alerts[alert] << '}'); }

	ISO_ptr<void>	SetLightState(int i, bool on)									const { return Read("result", SetLightState(Connect(), i, on)); }
	ISO_ptr<void>	SetLightState(int i, float trans)								const { return Read("result", SetLightState(Connect(), i, trans)); }
	ISO_ptr<void>	SetLightState(int i, const HUEColour &h, bool on = true)		const { return Read("result", SetLightState(Connect(), i, h, on)); }
	ISO_ptr<void>	SetLightState(int i, const HUEColour &h, float trans)			const { return Read("result", SetLightState(Connect(), i, h, trans)); }
	ISO_ptr<void>	SetLightAlert(int i, int alert)									const { return Read("result", SetLightAlert(Connect(), i, alert)); }

// light groups
	ISO_ptr<void>	CreateGroup(const char *name, uint32 lights)					const { return Send("result", "POST", "groups", buffer_accum<256>() << "{\"name\": \"" << name << "\", " << HUELightArray(lights) << '}'); }
	ISO_ptr<void>	SetGroupLights(int i, uint32 lights)							const { return Send("result", "PUT", "groups/" + to_string(i), buffer_accum<256>() << "{\"lights\": " << HUELightArray(lights) << '}'); };
	ISO_ptr<void>	DeleteGroup(int i)												const { return Send("delete", "DELETE", "groups/" + to_string(i)); }
	ISO_ptr<void>	SetGroupState(int i, bool on)									const { return Send("result", "PUT", "groups/" + to_string(i) + "/action", buffer_accum<256>("{\"on\": ") << on << '}'); }
	ISO_ptr<void>	SetGroupState(int i, float trans)								const { return Send("result", "PUT", "groups/" + to_string(i) + "/action", buffer_accum<256>() << '{' << HUETransiton(trans) << '}'); }
	ISO_ptr<void>	SetGroupState(int i, const HUEColour &h, bool on = true)		const { return Send("result", "PUT", "groups/" + to_string(i) + "/action", buffer_accum<256>() << '{' << h << ", \"on\": " << on << '}'); }
	ISO_ptr<void>	SetGroupState(int i, const HUEColour &h, float trans)			const { return Send("result", "PUT", "groups/" + to_string(i) + "/action", buffer_accum<256>() << '{' << h << ", " << HUETransiton(trans) << '}'); }

// schedule
	ISO_ptr<void>	AddSchedule(const HUESchedule &sched) const {
		buffer_accum<256>	ba;
		sched.JSON(ba, user);
		return Send("result", "POST", "schedules", ba);
	}
	ISO_ptr<void>	DeleteSchedule(int i) const {
		return Send("delete", "DELETE", "schedules/" + to_string(i));
	}

// misc
	ISO_ptr<void>	ScanForNewBulbs()				const { return Send("result", "POST", "lights"); }
	ISO_ptr<void>	GetNewBulb()					const { return Send("newlight", "GET", "lights/new"); }
	ISO_ptr<void>	DeleteUser(const char *user)	const { return Send("delete", "DELETE", str("config/whitelist/") + user); }
};

//-----------------------------------------------------------------------------
//	HueAnimation
//-----------------------------------------------------------------------------
#if defined PLAT_PC && !defined PLAT_WINRT
struct HueAnimation : TimerV {
	HUE		hue;
	int		i;
	template<typename T> HueAnimation(T *me, HUE &_hue) : Timer(me), hue(_hue), i(0)	{}
};

struct HueAnimationPulse : HueAnimation {
	HUEColour	c0, c1;
	timer		time;
	float		prev_time;

	rng<xorshiftplus<64> >	rand;

	void operator()(Timer*) {
	#if 0
		HUEColour	&c = i & 1 ? c1 : c0;

		hue.SetLightState(1, c, 0.1f);
		hue.SetLightState(2, c, 0.1f);
		hue.SetLightState(3, c, 0.1f);
		++i;
	#endif
		Socket	sock = hue.Connect();
		for (int i = 1; i < 4; i++)
			hue.Read(0, hue.SetLightState(sock, i, HUEColour::HueSat(rand, 1, 1), 0.1f));
	}
	HueAnimationPulse(HUE &_hue, const HUEColour &_c0, const HUEColour &_c1) : HueAnimation(this, _hue), c0(_c0), c1(_c1) {
		Timer::Start(.5f);
	}
};
#endif
//-----------------------------------------------------------------------------
//	HueHub
//-----------------------------------------------------------------------------

string HueUser(const char *host) {
#ifdef PLAT_WINRT
	return "bugger";
#else
	auto	r1	= Settings()["Hue"].values();
	auto	r4	= r1[host];
	auto	r5	= r4.get_text();
	string	r6	= r5;
	return r5;
#endif
}

class HueHub : public ISO::VirtualDefaults {
	HUE				hue;
	ISO_ptr<void>	p;
public:
	HueHub(const char *host) : hue(host, HueUser(host)) {
		p	= hue.GetState();
#ifndef PLAT_WINRT
		if (ISO::Browser(p)[0][0].GetName() == "error") {
			while (!hue.Authorise("isoeditor#user")) {
				if (MessageBox(Control(), "Press Link Button", "IsoEditor", MB_OKCANCEL) != IDOK)
					return;
			}
			Settings()["Hue"].values()[host] = hue.User();
			p	= hue.GetState();
		}
#endif
//		new HueAnimationPulse(hue, HUEColour::HueSat(0, 1, 1), HUEColour::HueSat(1 / 3.f, 1, 1));
	}
	int			Count()						{ return ISO::Browser(p).Count(); }
	tag			GetName(int i)				{ return ISO::Browser(p).GetName(i); }
	int			GetIndex(tag id, int from)	{ return ISO::Browser(p).GetIndex(id, from);	}
	ISO::Browser	Index(int i)				{ return ISO::Browser(p)[i]; }
	bool		Update(const char *spec, bool from);
};

template<> struct ISO::def<HueHub> : public ISO::VirtualT<HueHub> {};

bool HueHub::Update(const char *spec, bool from) {
	if (str(spec).begins("[0][0]")) {
		//lights
		int	i;
		from_string(spec + 7, i);
		ISO::Browser	b	= ISO::Browser(p).Parse(spec);
		tag2		id	= b.GetName();

		if (id == "name") {
			hue.SetLightName(i + 1, b.GetString());
		} else if (id == "on") {
			hue.SetLightState(i + 1, !!b.GetInt());
		} else if (id == "bri") {
			hue.SetLightState(i + 1, HUEColour::Brightness(b.GetFloat() / 254));
		} else {
			fixed_string<256>	specstate(spec);
			specstate.rfind("[")[0] = 0;

			ISO::Browser	state	= ISO::Browser(p).Parse(specstate);
			float		bri		= state["bri"].GetFloat() / 254;
			HUEColour	col;

			if (id == "hue" || id == "sat") {
				col = HUEColour::HueSat(state["hue"].GetFloat() / 65536, state["sat"].GetFloat() / 254, bri);
			} else if (id == "ct") {
				col = HUEColour::Temperature(1000000 / b.GetFloat(154), bri);
			} else if (id == "xy") {
				col = HUEColour::CIE1931(b[0].GetFloat(), b[1].GetFloat(), bri);
			} else if (!id) {
				col = HUEColour::CIE1931(state["xy"][0].GetFloat(), state["xy"][1].GetFloat(), bri);
			}
			hue.SetLightState(i + 1, col);

//			HUEColour	cols[3] = {col,col,col};
//			main->AddEntry(hue.SetLightStates(true, cols));
		}
	}
	return false;
}

struct PhilipsHueDevices : DeviceT<PhilipsHueDevices> {
	struct HueDevice : DeviceCreateT<HueDevice> {
		string		name;
		string		host;
		ISO_ptr<void>	operator()(const Control &main) {
			return Busy(), ISO_ptr<HueHub>(name, host);
		}
		void	operator()(AppEvent *ev) {
			if (ev->state == AppEvent::END)
				delete this;
		}
		HueDevice(const char *_name, const char *_host) : name(_name), host(_host)	{}
	};

	void			operator()(const DeviceAdd &add) {
		DeviceAdd	sub = add("Philips Hue", LoadPNG("IDB_DEVICE_HUE"));
		return;
		RunThread([this, sub] {
			dynamic_array<string>	locs;
			for (Socket sock = UPnP_scan("libhue.idl"); sock.select(10); ) {
				IP4::socket_addr	addr;
				char		buff[1024];
				size_t		len		= addr.recv(sock, buff, sizeof(buff));
				buff[len] = 0;

				if (char *st = (char*)istr(buff, len).find("\nst:")) {
					st = skip_whitespace(st + 4);
					if (str(st, string_find(st, '\r')) != "upnp:rootdevice")
						continue;
				}

				if (char *loc = (char*)istr(buff, len).find("location:")) {
					loc = skip_whitespace(loc += 9);
					*str(loc).find('\r') = 0;
					if (find_check(locs, loc))
						continue;

					locs.push_back(loc);

					HTTP		http("HUE", loc);
					XMLDOM		dom(HTTPinput(http.Request("GET", http.PathParams())));

					const char *name = dom/"root"/"device"/"friendlyName";
					sub(name, new HueDevice(name, http.host));
				}
			}
		});
	}
} philipshue_devices;

