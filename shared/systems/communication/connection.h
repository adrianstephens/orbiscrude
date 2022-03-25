#ifndef CONNECTION_H
#define CONNECTION_H

#if defined(CROSS_PLATFORM) || defined(ISOLINK_TARGET_H)
#include "isolink.h"
#include "iso/iso_binary.h"
#include "extra/memory_cache.h"

struct isolink_memory_interface : iso::memory_interface {
	isolink_handle_t	handle;
	isolink_memory_interface(const char *target);
	~isolink_memory_interface();
	bool	get(void *buffer, iso::uint64 size, iso::uint64 address);
};

isolink_platform_t	GetPlatform(const char *target);
size_t				ReceiveCheck(const char *target, isolink_handle_t handle, void *buffer, size_t size);
isolink_handle_t	SendBuffer(const char *target, const void *buffer, unsigned size);
isolink_handle_t	SendCommand(const char *target, iso::ISO_ptr<void> p, bool big = false);
bool				GetTargetMemory(const char *target, void *buffer, unsigned size, unsigned address, bool sync = false);
iso::malloc_block	GetTargetMemory(const char *target, unsigned size, unsigned address, bool sync = false);

class Connection {
public:
	const char			*target;
	const char			*platform;
	bool				bigendian;
	enum		{ PORT = 31978 };
	enum reason { INFO, WARNING, FAIL };

	struct Sink {
		virtual void	Message(reason r, const char *buffer) = 0;
		void			Printf(reason r, const char *format, ...);
	} *sink;

	isolink_handle_t	Check(isolink_handle_t h) {
		if (h == isolink_invalid_handle)
			sink->Message(FAIL, isolink_get_error());
		return h;
	}
public:
	Connection(Sink *v) : target(0), platform(0), sink(v)	{}
	Connection(Sink *v, const char *target) : sink(v)			{ SetTarget(target); }
	const char*			TargetName()					const	{ return target; }
	const char*			TargetPlatform()				const	{ return platform; }
	bool				SetTarget(const char *_target);

	isolink_handle_t	SendBuffer(const void *buffer, unsigned size) {
		return Check(::SendBuffer(platform, buffer, size));
	}
	isolink_handle_t	SendCommand(iso::ISO_ptr<void> p) {
		return Check(::SendCommand(platform, p, bigendian));
	}
	size_t				ReceiveCheck(isolink_handle_t handle, void *buffer, size_t size) {
		return ::ReceiveCheck(target, handle, buffer, size);
	}
};
#endif

#endif //CONNECTION_H
