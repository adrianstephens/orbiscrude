#include "connection.h"
#include "stream.h"
#include "thread.h"

using namespace iso;

isolink_platform_t GetPlatform(const char *target) {
	return isolink_resolve(target, Connection::PORT);
}

size_t ReceiveCheck(const char *target, isolink_handle_t handle, void *buffer, size_t size) {
	struct ReceiveCheckThread : Thread {
		const char		*target;
		isolink_handle_t handle;
		bool			stop;
		ReceiveCheckThread(const char *_target, isolink_handle_t _handle) : Thread(this), target(_target), handle(_handle), stop(false) {
			Start();
		}
		int	operator()() {
			for (int i = 0; i < 4; i++) {
				sleep(1);
				if (stop)
					break;
				isolink_close(SendCommand(target, ISO_ptr<int>("ASyncFlush",0)));
			}
			if (!stop)
				isolink_close(handle);
			while (!stop)
				sleep(1);
			delete this;
			return 0;
		}
	};
	ReceiveCheckThread	*thread = new ReceiveCheckThread(target, handle);
	size = isolink_receive(handle, buffer, size);
	thread->stop = true;
	return size;
}

isolink_handle_t SendBuffer(const char *target, const void *buffer, unsigned size) {
	if (target == isolink_platform_invalid)
		return isolink_invalid_handle;

	uint32be		sizebe	= size;
	isolink_handle_t handle = isolink_send(target, Connection::PORT, &sizebe, 4);
	if (handle != isolink_invalid_handle && !isolink_send(handle, buffer, size)) {
		isolink_close(handle);
		handle = isolink_invalid_handle;
	}
	return handle;
}

isolink_handle_t SendCommand(const char *target, ISO_ptr<void> p, bool big) {
	if (target == isolink_platform_invalid)
		return isolink_invalid_handle;

	ISO_ptr<anything> pa(NULL, 1);
	(*pa)[0] = p;

	dynamic_memory_writer	file;
	ISO::binary_data.save(), ISO::binary_data.Write(ISO::Browser(pa), file, 0, ISO::BIN_STRINGIDS | ISO::BIN_EXPANDEXTERNALS | ISO::BIN_WRITEREADTYPES | (big ? ISO::BIN_BIGENDIAN : 0));
	return SendBuffer(target, file.data(), file.length());
}

bool GetTargetMemory(const char *target, void *buffer, unsigned size, unsigned address, bool sync) {
	ISO_TRACEF("Getting 0x%08x bytes from %s:0x%08x to 0x%08x\n", size, target, address, uint32(intptr_t(buffer)));
	isolink_handle_t	h = SendCommand(target, ISO_ptr<pair<uint32,uint32> >("GetMemory", make_pair(address, sync ? (size | 0x80000000) : size)));
	size_t	read = isolink_receive(h, buffer, size);
	isolink_close(h);
	return read == size;
}

malloc_block GetTargetMemory(const char *target, unsigned size, unsigned address, bool sync) {
	malloc_block	buffer(size);
	if (GetTargetMemory(target, buffer, size, address, sync))
		return buffer;
	return none;
}

isolink_memory_interface::isolink_memory_interface(const char *target) {
	uint16be	result;
	handle	= SendCommand(target, ISO_ptr<int>("MemoryInterface",0));
	ReceiveCheck(target, handle, &result, 2);
}

isolink_memory_interface::~isolink_memory_interface() {
	pair<uint32be, uint32be>	req(0, 0);
	isolink_send(handle, &req, sizeof(req));
	isolink_close(handle);
}

bool isolink_memory_interface::get(void *buffer, iso::uint64 size, iso::uint64 address) {
	ISO_TRACEF("mem request: 0x%08x:0x%08x\n", address, (uint32)size);
	pair<uint32be, uint32be>	req((uint32)address, (uint32)size);
	isolink_send(handle, &req, sizeof(req));
	return isolink_receive(handle, buffer, (uint32)size) == (uint32)size;
}

void Connection::Sink::Printf(reason r, const char *format, ...) {
	char buffer[256];
	va_list	arglist;
	va_start(arglist, format);
	_vsnprintf(buffer, num_elements(buffer), format, arglist);
	va_end(arglist);
	Message(r, buffer);
}

bool Connection::SetTarget(const char *_target) {
	platform = isolink_resolve(_target, PORT);
	if (platform == isolink_platform_invalid) {
		if (const char *error = isolink_get_error())
			sink->Printf(FAIL, "Could not connect to %s:\n%s\n", _target, error);
		else
			sink->Printf(FAIL, "Could not connect to %s\n", _target);
		target = 0;
		return false;
	}
	target		= _target;
	bigendian	= str(target) != "pc";

	sink->Printf(INFO, "Connected to %s\n", target);
	return true;
}
