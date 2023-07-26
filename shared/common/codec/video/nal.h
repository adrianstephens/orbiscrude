#include "base/array.h"

//-----------------------------------------------------------------------------
// NAL
//-----------------------------------------------------------------------------

namespace iso {

struct chunked_mem {
	const_memory_block	data;

	struct iterator {
		const uint8 *p, *end;
		iterator(const uint8 *p) : p(p) {}
		const_memory_block operator*()				const { return {p + 4, *(const uint32be*)p}; }
		bool		operator!=(const iterator &b)	const { ISO_ASSERT(p <= b.p); return p != b.p; }
		iterator&	operator++() { p += *(const uint32be*)p + 4; return *this; }
	};
	chunked_mem(const_memory_block data) : data(data) {}
	auto	begin()	const { return iterator(data.begin()); }
	auto	end()	const { return iterator(data.end()); }
};

struct NAL {
	class Parser;

	struct header {
		union {
		#if ISO_BIGENDIAN
			struct {uint16	:1, type:6, layer_id:6, temporal_id:3;};
		#else
			struct {uint16	temporal_id_plus1:3, layer_id:6, type:6, :1;};
		#endif
			uint16be	u;
		};
		header(const void *p) : u(*(const uint16*)p) {}
	};

	class unit {
		dynamic_array<int> skipped_bytes; // up to position[x], there were 'x' skipped bytes
		Parser			*parser;
	public:
		malloc_block2	data;
		int64			pts			= 0;
		void*			user_data	= nullptr;

		unit(Parser *parser) : parser(parser) {}
		void	init(int size, int64 _pts, void* _user_data) {
			pts			= _pts;
			user_data	= _user_data;
			data.resize(0);
			data.reserve(size);
			skipped_bytes.clear();
		}

		void	release();

		void	resize(int new_size)			{ data.resize(new_size); }
		void	append(const uint8* p, int n)	{ data += const_memory_block(p, n); }
		void	set_data(const uint8* p, int n)	{ data = const_memory_block(p, n); }

		int		num_skipped_bytes_before(int offset) const {
			for (int k = skipped_bytes.size(); k--;)
				if (skipped_bytes[k] <= offset)
					return k + 1;
			return 0;
		}
		int		num_skipped_bytes() const		{ return skipped_bytes.size(); }
		void	insert_skipped_byte(int pos)	{ skipped_bytes.push_back(pos); }
		auto	get_header()	const			{ return header(data); }
	};

	class Parser {
		int			state			= 0;
		unit*		pending			= nullptr;
		static_array<unit*, 16>		free_list;
		dynamic_array_de<unit*>		queue;

		unit*		alloc(int size, int64 pts, void* user_data = NULL);
	public:
		bool		end_of_stream		= false;	// data in pending_input_data is end of stream
		bool		end_of_frame		= false;	// data in pending_input_data is end of frame

		~Parser();
		bool		push_stream(const_memory_block data, int64 pts, void* user_data = NULL);
		bool		push_nal(const_memory_block data, int64 pts, void* user_data = NULL);
		unit*		pop();
		bool		flush_data();
		void		remove_pending_input_data();
		void		dealloc(unit *nal);

		int			number_pending()		const { return queue.size() + !!pending; }
		int			queue_length()			const { return queue.size(); }
	};
};
} // namespace iso