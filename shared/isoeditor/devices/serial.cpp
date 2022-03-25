#include "base/defs.h"
#include "base/strings.h"
#include "extra/text_stream.h"
#include "jobs.h"

using namespace iso;

struct Serial {
	HANDLE	h;
	bool	own;

	Serial(int com) {
//		h = CreateFileA(format_string("\\\\.\\COM%i", com),
		h = CreateFileA(format_string("COM%i", com),
			GENERIC_READ | GENERIC_WRITE,	//Read/Write
			0,								// No Sharing
			NULL,							// No Security
			OPEN_EXISTING,					// Open existing port only
			0,								// Non Overlapped I/O
			NULL							// Null for Comm Devices
		);
		own = true;
	}
	Serial(HANDLE _h) : h(_h), own(false) {}

	~Serial() {
		if (own)
			CloseHandle(h);	//Closing the Serial Port
	}

	bool	GetParams(int &speed, int &bytesize, int &stopbits, int &parity) {
		DCB		params;
		clear(params);
		params.DCBlength = sizeof(params);
		if (!GetCommState(h, &params))
			return false;

		speed = params.BaudRate;
		bytesize = params.ByteSize;
		stopbits = params.StopBits;
		parity = params.Parity;
		return true;
	}

	bool	SetParams(int speed, int bytesize, int stopbits, int parity) {
		DCB		params;
		clear(params);
		params.DCBlength = sizeof(params);

		if (!GetCommState(h, &params))
			return false;

		params.BaudRate = speed;
		params.ByteSize = bytesize;
		params.StopBits = stopbits;
		params.Parity = parity;

		return !!SetCommState(h, &params);
	}

	bool SetTimeouts(uint32 read_interval, uint32 read_total_constant, uint32 read_total_multiplier, uint32 write_total_constant, uint32 write_total_multiplier) {
		COMMTIMEOUTS timeouts = { 0 };
		timeouts.ReadIntervalTimeout = read_interval;
		timeouts.ReadTotalTimeoutConstant = read_total_constant;
		timeouts.ReadTotalTimeoutMultiplier = read_total_multiplier;
		timeouts.WriteTotalTimeoutConstant = write_total_constant;
		timeouts.WriteTotalTimeoutMultiplier = write_total_multiplier;

		return !!SetCommTimeouts(h, &timeouts);
	}
	bool SetReceiveMask(uint32 mask) {
		return !!SetCommMask(h, mask);
	}
	uint32 Wait() {
		DWORD	event_mask;
		return WaitCommEvent(h, &event_mask, NULL) ? event_mask : 0;
	}
	uint32	Read(void *buffer, int size) {
		DWORD	read;
		return ReadFile(h, buffer, size, &read, NULL) ? read : 0;
	}
	uint32	ReadLine(void *buffer, uint32 max_size) {
		char	*p = (char*)buffer, *e = p + max_size;
		while (p != e && Read(p, 1) && *p)
			++p;
		return p - (char*)buffer;
	}
	uint32	Write(const void *buffer, uint32 size) {
		DWORD	written;
		return WriteFile(h, buffer, size, &written, NULL) ? written : 0;
	}
	uint32	Write(const char *buffer) {
		return Write(buffer, uint32(strlen(buffer)));
	}
};

#include "filename.h"
#include "stream.h"

struct serial_tester {
	serial_tester(const filename &fn) {
		FileInput	in(fn);

		Serial	s(3);
		s.SetParams(CBR_115200, 8, ONESTOPBIT, NOPARITY);
		s.SetTimeouts(~0, 0, 0, 0, 0);
		//		s.SetTimeouts(50, 50, 10, 50, 10);
		s.SetReceiveMask(EV_RXCHAR); //Configure Windows to Monitor the serial device for Character Reception

		//s.Write("M999\n");

		int		line_number = 0;
		char	receive_buffer[256], *pr = receive_buffer;
		auto	text = make_text_reader(in);

		for (;;) {

			for (;;) {
				++line_number;
				string	line;
				text.read_line(line);
				const char	*begin = line.begin();
				const char	*end = line.find(';');
				if (!end)
					end = line.end();
				if (end - begin) {
					ISO_TRACEF("Send: ") << str(begin, end) << '\n';
					s.Write(begin, end - begin);
					s.Write("\n");
					break;
				}
			}

			for (bool ok = false; !ok;) {
				s.Wait(); //Wait for the character to be received
				uint32	m = end(receive_buffer) - pr;
				int		n = s.ReadLine(pr, m);

				ISO_TRACEF(str(pr, n));
				pr += n;

				while (char *nl = string_find(receive_buffer, '\n', pr)) {
					string	line = str(receive_buffer, nl);
					int		rem = pr - (nl + 1);
					pr = receive_buffer + rem;
					memcpy(receive_buffer, nl + 1, rem);
					ok = line == "ok";
				}
			}

		}
	}
};// _serial_tester("Q:\\rook2.g");

//-----------------------------------------------------------------------------
//	device
//-----------------------------------------------------------------------------

#include "device.h"
#include "viewers/viewer.h"
#include "windows/registry.h"
#include "windows/text_control.h"

struct SerialPort : ISO::VirtualDefaults {
	HANDLE	h;
	DCB		settings;

	SerialPort(HANDLE _h) : h(_h) {
		clear(settings);
		settings.DCBlength = sizeof(DCB);
		GetCommState(h, &settings);
	}
	~SerialPort() {
		CloseHandle(h);
	}
	int			Count()			{ return 0; }
	ISO::Browser	Deref()			{ return ISO::MakeBrowser(settings); }
	bool		Update(const char *s, bool from) {
		return SetCommState(h, &settings);
	}
};

ISO_DEFUSERVIRT(SerialPort);
ISO_DEFUSERCOMPV(DCB,BaudRate,XonLim, XoffLim, ByteSize, Parity, StopBits, XonChar, XoffChar, ErrorChar, EofChar, EvtChar);

/*
fBinary: 1,
fParity: 1,
fOutxCtsFlow:1,
fOutxDsrFlow:1,
fDtrControl:2,
fDsrSensitivity:1,
fTXContinueOnXoff: 1,
fOutX: 1,
fInX: 1,
fErrorChar: 1,
fNull: 1,
fRtsControl:2,
fAbortOnError:1,
*/

class SerialTTY : public win::Subclass<SerialTTY, win::D2DEditControl> {
	Serial		port;
	Semaphore	semaphore;
	Mutex		mutex;
public:
	LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam) {
		switch (message) {
			case WM_CHAR: {
				auto		w		= with(mutex);
				char		s[2]	= {wParam == '\r' ? '\n' : (char)wParam, 0};
				ReplaceSelection(s, false);
				EnsureVisible();
				if (wParam == '\r') {
					string	line = GetLineText(GetCurrentLineStart() - 1);
					if (line && *skip_whitespace(line.begin())) {
						port.Write(line, line.size32());
						port.Write("\n");
						semaphore.unlock();
					}
				}
				return 0;
			}

			case WM_NCDESTROY:
				delete this;
				return 0;
		}
		return Super(message, wParam, lParam);
	}
	SerialTTY(const win::WindowPos &wpos, const char *title, SerialPort *_port) : port(_port->h), semaphore(1) {
		Create(wpos, title, CHILD | VISIBLE | VSCROLL);

		Rebind(this);
		SetBackground(colour::black);
		SetFormat(win::CharFormat().Font("Courier New").Size(11).Colour(colour::white));

		port.SetParams(CBR_115200, 8, ONESTOPBIT, NOPARITY);
		port.SetTimeouts(~0, 0, 0, 0, 0);
		port.SetReceiveMask(EV_RXCHAR);

		DWORD	error;
		COMSTAT comstat;
		ClearCommError(port.h, &error, &comstat);
		RunThread([this]() {
			char	receive_buffer[256];
			//while (semaphore.lock() && port.h) {
			while (port.h) {
				port.Wait();	//wait for a character to be received
				int		n;
				while ((n = port.ReadLine(receive_buffer, 255)) == 255) {
					receive_buffer[n] = 0;
	//				replace(receive_buffer, '\n', '\r');

					auto	w = with(mutex);
					SetSelection(win::CharRange::end());
					ReplaceSelection(receive_buffer, false);
					EnsureVisible();
				}
			}
		});
	}
};

class SerialPortEditor : public app::Editor {
	virtual bool Matches(const ISO::Type *type) {
		return type->SameAs<SerialPort>();
	}
	virtual win::Control Create(app::MainWindow &main, const win::WindowPos &wpos, const ISO_ptr<void> &p) {
		return *new SerialTTY(wpos, p.ID().get_tag(), p);
	}
public:
} serialport_tty;

struct SerialDevice : app::DeviceT<SerialDevice> {

	struct SerialPortDevice : app::DeviceCreateT<SerialPortDevice> {
		string	port;
		ISO_ptr<void>	operator()(const win::Control &main) {
			return ISO_ptr<SerialPort>(port, CreateFileA(port, GENERIC_READ | GENERIC_WRITE, 0,  NULL, OPEN_EXISTING, 0, NULL));
		}
		SerialPortDevice(const char *_port) : port(_port) {}
	};

	void			operator()(const app::DeviceAdd &add) {
		app::DeviceAdd	sub = add("Serial Ports", app::LoadPNG("IDB_DEVICE_SERIALPORT"));
	#if 1
		win::RegKey	r(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\SERIALCOMM");
		for (auto i : r.values()) {
			string	name = i;
			sub(name, new SerialPortDevice(name));
		}
	#else
		for (int size = 4096; size; size = GetLastError() == ERROR_INSUFFICIENT_BUFFER ? size * 2 : 0) {
			malloc_block	data(size)
			if (QueryDosDevice(NULL, data, size)) {
				for (const char *p = data; *p; p += strlen(p) + 1) {
					if (str(p).begins("COM") && is_digit(p[3])) {
						app::AddMenuItem(sub, id, p,
							new SerialPortDevice(p)
						);
					}
				}
				break;
			}
		}
	#endif
	}
} serial_device;

