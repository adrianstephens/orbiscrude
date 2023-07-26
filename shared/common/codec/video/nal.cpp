#include "nal.h"

using namespace iso;

void NAL::unit::release() {
	parser->dealloc(this);
}

NAL::unit*	NAL::Parser::alloc(int size, int64 pts, void* user_data) {
	unit* nal = free_list.empty() ? new unit(this) : free_list.pop_back_value();
	nal->init(size, pts, user_data);
	return nal;
}

NAL::Parser::~Parser() {
	while (auto nal = pop())
		dealloc(nal);

	dealloc(pending);

	for (auto &i : free_list)
		delete i;
}

bool NAL::Parser::push_stream(const_memory_block data, int64 pts, void* user_data) {
	end_of_frame = false;

	if (!pending)
		pending = alloc(data.size() + 3, pts, user_data);
	else
		pending->data.reserve(pending->data.size() + data.size() + 3);

	unit*			nal = pending;
	uint8*			out = nal->data.end();
	const uint8*	p	= data, *end = data.end();
#if 1
	while (p < end) {
		uint8	b = *p++;
		switch (state) {
			case 0:
			case 1:
				if (b == 0)
					state++;
				else
					state = 0;
				break;
			case 2:
				if (b == 1)
					state = 3;
				else if (b != 0)
					state = 0;
				break;
			case 3:
				*out++ = b;
				state = 4;
				break;
			case 4:
				*out++ = b;
				state = 5;
				break;
			case 5:
				if (b==0)
					state = 6;
				else
					*out++ = b;
				break;
			case 6:
				if (b==0) {
					state = 7;
				} else {
					*out++ = 0;
					*out++ = b;
					state = 5;
				}
				break;
			case 7:
				if (b == 0) {
					*out++ = 0;
				} else if (b == 3) {
					*out++ = 0;
					*out++ = 0;
					state = 5;
					// remember which byte we removed
					nal->insert_skipped_byte((out - nal->data) + nal->num_skipped_bytes());
				} else if (b==1) {
					ISO_ASSERT(out - nal->data < nal->data.max_size);
					nal->resize(out - nal->data);
					// push this to queue
					queue.push_back(nal);
					// initialize new, empty NAL unit
					nal = pending	= alloc(end - p + 3, pts, user_data);
					out = nal->data;
					state = 3;
				} else {
					*out++ = 0;
					*out++ = 0;
					*out++ = b;
					state = 5;
				}
				break;
		}
	}
#else
	while (p < end) {
		uint8	b = *p++;
		switch (state++) {
			case 0:		case 1:
				if (b)
					state = 0;
				break;
			case 2:
				if (b != 1)
					state = b == 0 ? 2 : 0;
				break;
			case 3:		case 4:
				*out++ = b;
				break;
			case 5:
				if (b) {
					*out++ = b;
					state = 5;
				}
				break;
			case 6:
				if (b) {
					*out++ = 0;
					*out++ = b;
					state = 5;
				}
				break;
			case 7:
				switch (b) {
					case 0:
						*out++ = 0;
						state = 7;
						break;
					case 1:
						nal->resize(out - nal->data);
						// push this to queue && initialize new, empty NAL unit
						queue.push_back(nal);
						nal = pending	= alloc(end - p + 3, pts, user_data);
						out = nal->data;
						state = 3;
						break;
					case 3:
						*out++ = 0;
						*out++ = 0;
						// remember which byte we removed
						nal->insert_skipped_byte((out - nal->data) + nal->num_skipped_bytes());
						state = 5;
						break;
					default:
						*out++ = 0;
						*out++ = 0;
						*out++ = b;
						state = 5;
						break;
				}
				break;
		}
	}
#endif

	nal->resize(out - nal->data);
	return true;
}

bool NAL::Parser::push_nal(const_memory_block data, int64 pts, void* user_data) {
	end_of_frame		= false;
	unit		*nal	= alloc(data.size(), pts, user_data);
	uint8		*out	= nal->data;
	const uint8	*end	= data.end() - 2;
	const uint8* p		= data;

	while (p < end) {
		*out++ = *p++;
		if (p[1] != 3 && p[1] != 0) {
			*out++ = *p++;
			*out++ = *p++;
		} else if (p[-1] == 0 && p[0] == 0 && p[1] == 3) {
			*out++ = *p++;
			p++;
			nal->insert_skipped_byte(p - data);
		}
	}
	while (p < data.end())
		*out++ = *p++;

	nal->resize(out - nal->data);
	queue.push_back(nal);
	return true;
}

NAL::unit* NAL::Parser::pop() {
	return queue.empty() ? nullptr : queue.pop_front_value();
}

bool NAL::Parser::flush_data() {
	if (auto nal = pending) {
		uint8 null[2] = {0, 0};
		// only push the NAL if it contains at least the NAL header
		if (state >= 5) {
			// append bytes that are implied by the push state
			if (state == 6)
				nal->append(null,1);
			if (state == 7)
				nal->append(null,2);
			queue.push_back(nal);
			pending = nullptr;
		}
		state = 0;
	}
	return true;
}

void NAL::Parser::remove_pending_input_data() {
	if (pending) {
		dealloc(pending);
		pending = nullptr;
	}
	while (auto nal = pop())
		dealloc(nal);
	state	= 0;
}

void NAL::Parser::dealloc(unit *nal) {
	if (nal) {
		if (free_list.full())
			delete nal;
		else
			free_list.push_back(nal);
	}
}
