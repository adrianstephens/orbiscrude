#include "iso/iso_files.h"
#include "base/algorithm.h"
#include "matroska.h"

namespace matroska {
//-----------------------------------------------------------------------------
//	Block
//-----------------------------------------------------------------------------

int64 Block::GetTimeCode(Cluster *cluster) const {  // absolute, but not scaled
	return cluster ? timecode + cluster->GetTimeCode() : timecode;
}
int64 Block::GetTime(Cluster *cluster) const {  // absolute, and scaled (ns)
	return GetTimeCode(cluster) * cluster->TimecodeScale();
}

int Block::Parse(const Cluster* cluster) {
	istream_ref	file = cluster->File();
	file.seek(range.start);
	int		len;
	track = read_ebml_num(file, len);
	if (track <= 0)
		return E_FILE_FORMAT_INVALID;

	timecode	= file.get<uint16be>();
	flags		= file.getc();
	int lacing	= int(flags & 0x06) >> 1;
	frames.resize(lacing == LacingNone ? 1 : file.getc() + 1);

	switch (lacing) {
		case LacingNone: {
			Frame& f = frames[0];
			f.pos = file.tell();
			f.len = static_cast<int32>(range.end() - f.pos);
			break;
		}
		case LacingXiph: {
			int64 size = 0;
			for (auto i = frames.begin(), e = frames.end() - 1; i != e; ++i) {
				int32 frame_size = 0;
				for (;;) {
					uint8 val = file.getc();
					frame_size += val;
					if (val < 255)
						break;
				}

				i->len	= frame_size;
				size	+= frame_size;
			}

			streamptr	pos = file.tell();
			frames.back().len = range.end() - pos - size;
			for (auto i = frames.begin(), e = frames.end() - 1; i != e; ++i) {
				i->pos = pos;
				pos += i->len;
			}
			break;
		}
		case LacingFixed: {
			const int64 total_size = range.end() - file.tell();
			if (total_size % frames.size32() != 0)
				return E_FILE_FORMAT_INVALID;

			const int64 frame_size = total_size / frames.size32();
			if (frame_size > (long)maximum || frame_size <= 0)
				return E_FILE_FORMAT_INVALID;

			streamptr	pos = file.tell();
			for (auto i = frames.begin(), e = frames.end() - 1; i != e; ++i) {
				i->pos = pos;
				i->len = static_cast<int32>(frame_size);
				pos += frame_size;
			}
			break;
		}
		case LacingEbml: {
			int64	size		= 0;
			int64	frame_size	= read_ebml_num(file, len);

			for (auto i = frames.begin(), e = frames.end() - 1; i != e; ++i) {
				i->len	= frame_size;
				size	+= frame_size;

				int64	delta_size	= read_ebml_num(file, len);
				delta_size	-= (1LL << (7 * len - 1)) - 1;
				frame_size	+= delta_size;
			}

			//last frame
			streamptr	pos = file.tell();
			frames.back().len = range.end() - pos - size;
			for (auto i = frames.begin(), e = frames.end(); i != e; ++i) {
				i->pos = pos;
				pos += i->len;
			}
			break;
		}
	}
	return 0;
}

//-----------------------------------------------------------------------------
//	ContentEncoding
//-----------------------------------------------------------------------------

int ContentEncoding::ParseCompressionEntry(EBMLreader &reader, ContentCompression* compression) {
	bool valid = false;

	while (!reader.atend()) {
		EBMLreader	reader2(reader.child());
		switch (reader2.id) {
			case ContentCompression::ID_Algo:
				compression->algo = reader2.read_uint();
				valid = true;
				break;
			case  ContentCompression::ID_Settings:
				compression->settings = reader2.read_binary();
				break;
		}
	}
	return !valid ? E_FILE_FORMAT_INVALID : 0;
}

int ContentEncoding::ParseContentEncAESSettingsEntry(EBMLreader &reader, ContentEncAESSettings* aes) {
	while (!reader.atend()) {
		EBMLreader	reader2(reader.child());
		switch (reader2.id) {
			case ContentEncAESSettings::ID_CipherMode: {
				aes->cipher_mode = reader2.read_uint();
				if (aes->cipher_mode != 1)
					return E_FILE_FORMAT_INVALID;
				break;
			}
		}
	}
	return 0;
}

int ContentEncoding::ParseContentEncodingEntry(EBMLreader &reader) {
	int compression_count = 0;
	int encryption_count = 0;

	while (!reader.atend()) {
		EBMLreader	reader2(reader.child());
		switch (reader2.id) {
			case ContentCompression::ID:
				++compression_count;
				break;
			case ContentEncryption::ID:
				++encryption_count;
				break;
		}
	}

	if (compression_count == 0 && encryption_count == 0)
		return -1;

	compression_entries.reserve(compression_count);
	encryption_entries.reserve(encryption_count);

	reader.seek(reader.start);
	while (!reader.atend()) {
		EBMLreader	reader2(reader.child());
		switch (reader2.id) {
			case ID_Order:
				encoding_order = reader2.read_uint();
				break;
			case ID_Scope:
				encoding_scope = reader2.read_uint();
				if (encoding_scope < 1)
					return -1;
				break;
			case ID_Type:
				encoding_type = reader2.read_uint();
				break;

			case ContentCompression::ID: {
				ContentCompression* const compression = new(compression_entries) ContentCompression;
				int	status = ParseCompressionEntry(reader2, compression);
				if (status)
					return status;
				break;
			}
			case ContentEncryption::ID: {
				ContentEncryption* const encryption = new(encryption_entries) ContentEncryption;
				int	status = ParseEncryptionEntry(reader2, encryption);
				if (status)
					return status;
				break;
			}
		}
	}
	return 0;
}

int ContentEncoding::ParseEncryptionEntry(EBMLreader &reader, ContentEncryption* encryption) {
	while (!reader.atend()) {
		EBMLreader	reader2(reader.child());
		switch (reader2.id) {
			case ContentEncryption::ID_Algo:
				encryption->algo = reader2.read_uint();
				if (encryption->algo != 5)
					return E_FILE_FORMAT_INVALID;
				break;

			case ContentEncryption::ID_KeyID:
				encryption->key_id = reader2.read_binary();
				break;

			case ContentEncryption::ID_Signature:
				encryption->signature = reader2.read_binary();
				break;

			case ContentEncryption::ID_SigKeyID:
				encryption->sig_key_id = reader2.read_binary();
				break;

			case ContentEncryption::ID_SigAlgo:
				encryption->sig_algo = reader2.read_uint();
				break;

			case ContentEncryption::ID_SigHashAlgo:
				encryption->sig_hash_algo = reader2.read_uint();
				break;

			case ContentEncAESSettings::ID:
				const int32 status = ParseContentEncAESSettingsEntry(reader2, &encryption->aes_settings);
				if (status)
					return status;
				break;
		}
	}
	return 0;
}

//-----------------------------------------------------------------------------
//	Track
//-----------------------------------------------------------------------------

int Track::Create(Segment* segment, const TrackInfo& info, EBMLreader &reader, Track*& result) {
	if (result)
		return -1;

	Track* const track = new Track(segment, reader);
	info.Copy(*track);
	result = track;
	return 0;  // success
}

int Track::GetFirst(const BlockEntry*& entry) const {
	Cluster* cluster = segment->GetFirst();

	for (int i = 100; cluster && i--; cluster = segment->GetNext(cluster)) {
		int32 status = cluster->GetFirst(entry);
		if (status < 0)  // error
			return status;

		if (entry == 0) {  // empty cluster
			cluster = segment->GetNext(cluster);
			continue;
		}

		for (;;) {
			const Block* const block = entry->GetBlock();
			const int64 tn = block->track;

			if (tn == number && VetEntry(entry))
				return 0;

			const BlockEntry* next;
			status = cluster->GetNext(entry, next);

			if (status < 0)  // error
				return status;

			if (next == 0)
				break;

			entry = next;
		}
	}

	// NOTE: if we get here, it means that we didn't find a block with a matching track number.  An error might be too conservative
	entry = 0;
	return !cluster && !segment->DoneParsing() ? E_BUFFER_NOT_FULL : 1;
}

int Track::GetNext(const BlockEntry* curr, const BlockEntry*& next) const {
	const Block* const block = curr->GetBlock();
	if (!block || block->track != number)
		return -1;

	Cluster* cluster	= curr->cluster;
	int32	status		= cluster->GetNext(curr, next);
	if (status < 0)  // error
		return status;

	for (int i = 100; i--;) {
		while (next) {
			if (next->GetBlock()->track == number)
				return 0;

			curr	= next;
			status	= cluster->GetNext(curr, next);
			if (status < 0)  // error
				return status;
		}

		cluster = segment->GetNext(cluster);
		if (!cluster) {
			// TODO: there is a potential O(n^2) problem here: we tell the caller to (pre)load another cluster, which he does, but then he
			// calls GetNext again, which repeats the same search.  This is a pathological case, since the only way it can happen is if
			// there exists a long sequence of clusters none of which contain a block from this track.  One way around this problem is for the
			// caller to be smarter when he loads another cluster: don't call us back until you have a cluster that contains a block from this
			// track. (Of course, that's not cheap either, since our caller would have to scan each cluster as it's loaded, so that
			// would just push back the problem.)

			next = NULL;
			return !segment->DoneParsing() ? E_BUFFER_NOT_FULL : 1;
		}

		status = cluster->GetFirst(next);
		if (status < 0)  // error
			return status;
	}

	// NOTE: if we get here, it means that we didn't find a block with a matching track number after lots of searching, so we give up trying.
	next = 0;
	return 1;
}

// This function is used during a seek to determine whether the frame is a valid seek target.
// This default function simply returns true, which means all frames are valid seek targets.
// It gets overridden by the VideoTrack class, because only video keyframes can be used as seek target.
bool Track::VetEntry(const BlockEntry* entry) const {
	const Block* const block = entry->GetBlock();
	return block && block->track == number && (type != kVideo || entry->GetBlock()->IsKey());
}

int Track::Seek(int64 time_ns, const BlockEntry*& result) const {
	const int32 status = GetFirst(result);
	if (status < 0)  // buffer underflow, etc
		return status;

	if (!result)
		return 0;

	Cluster* cluster = result->cluster;
	if (time_ns <= result->GetBlock()->GetTime(cluster))
		return 0;

	Cluster** const i = segment->clusters + cluster->index;
	Cluster** const j = segment->clusters + segment->loaded_clusters;

	Cluster** lo = lower_bound(i, j, time_ns, [](Cluster *c, int64 time_ns) {
		return c->GetTime() < time_ns;
	});

	if (type != kVideo)
		time_ns = -1;	// return first entry

	while (lo > i) {
		cluster = *--lo;
		result = cluster->GetEntry(this, time_ns);
		if (result)
			return 0;
	}

	result = 0;
	return 0;
}

int Track::ParseContentEncodingsEntry(const file_range &range) {
	istream_ref		file	= segment->file;
	const int64 stop	= range.end();

	// Count ContentEncoding elements.
	int count = 0;
	file.seek(range.start);
	while (file.tell() < stop) {
		EBMLreader	reader(file);
		if (reader.id == ContentEncoding::ID)
			++count;
	}

	if (count == 0)
		return -1;

	content_encoding_entries.reserve(count);

	file.seek(range.start);
	while (file.tell() < stop) {
		EBMLreader	reader(file);
		if (reader.id == ContentEncoding::ID) {
			auto& content_encoding = content_encoding_entries.push_back();
			int	status = content_encoding.ParseContentEncodingEntry(reader);
			if (status) {
				content_encoding_entries.pop_back();
				return status;
			}
		}
	}
	return 0;
}

int VideoTrack::Parse(Segment* segment, const TrackInfo& info) {
	istream_ref		file = segment->file;
	file.seek(info.settings.start);
	while (file.tell() < info.settings.end()) {
		EBMLreader	reader2(file);
		switch (reader2.id) {
			case ID_PixelWidth:
				width = reader2.read_uint();
				break;

			case ID_PixelHeight:
				height = reader2.read_uint();
				break;

			case ID_DisplayWidth:
				display_width = reader2.read_uint();
				break;

			case ID_DisplayHeight:
				display_height = reader2.read_uint();
				break;

			case ID_DisplayUnit:
				display_unit = reader2.read_uint();
				break;

			case ID_StereoMode:
				stereo_mode = reader2.read_uint();
				break;

			case ID_FrameRate:
				rate = reader2.read_float();
				break;
		}
	}

	info.Copy(*this);
	return 0;  // success
}

int AudioTrack::Parse(Segment* segment, const TrackInfo& info) {
	istream_ref		file = segment->file;
	file.seek(info.settings.start);
	while (file.tell() < info.settings.end()) {
		EBMLreader	reader2(file);
		switch (reader2.id) {
			case ID_SamplingFreq:
				rate = reader2.read_float();
				if (rate <= 0)
					return E_FILE_FORMAT_INVALID;
				break;

			case ID_Channels:
				channels = reader2.read_uint();
				if (channels <= 0)
					return E_FILE_FORMAT_INVALID;
				break;

			case ID_Bitdepth:
				bit_depth = reader2.read_uint();
				if (bit_depth <= 0)
					return E_FILE_FORMAT_INVALID;
				break;
		}
	}

	info.Copy(*this);
	return 0;  // success
}

Tracks::~Tracks() {
	for_each(*this, [](Track *t) { delete t; });
}

int Tracks::Parse() {
	uint64		stop	= range.end();
	istream_ref		file	= segment->file;
	file.seek(range.start);

	int count = 0;
	while (file.tell() < stop) {
		EBMLreader	reader(file);
		if (reader.id == ID_TrackEntry)
			++count;
	}

	if (count == 0)
		return 0;  // success

	reserve(count);

	file.seek(range.start);
	while (file.tell() < stop) {
		EBMLreader	reader(file);
		if (reader.id == ID_TrackEntry) {
			const int32 status = ParseTrackEntry(reader, push_back());
			if (status) {
				pop_back();
				return status;
			}
		}
	}

	return 0;  // success
}

int Tracks::ParseTrackEntry(EBMLreader &reader, Track*& result) const {
	result = 0;

	TrackInfo info;
	info.type			= 0;
	info.number			= 0;
	info.uid			= 0;
	info.defaultDuration = 0;

	file_range		v(0, 0);
	file_range		a(0, 0);
	file_range		e(0, 0);  // content_encodings_settings;
	int64 lacing = 1;  // default is true

	while (!reader.atend()) {
		EBMLreader	reader2(reader.child());
		switch (reader2.id) {
			case TrackInfo::ID_Video:
				v= reader2;
				break;

			case TrackInfo::ID_Audio:
				a = reader2;
				break;

			case TrackInfo::ID_ContentEncodings:
				e = reader2;
				break;

			case TrackInfo::ID_UID:
				if (reader2.size > 8)
					return E_FILE_FORMAT_INVALID;
				info.uid = reader2.read_uint();
				break;

			case TrackInfo::ID_Number: {
				const int64 num = reader2.read_uint();
				if (num > 127)
					return E_FILE_FORMAT_INVALID;
				info.number = static_cast<int32>(num);
				break;
			}
			case TrackInfo::ID_Type: {
				const int64 type = reader2.read_uint();
				if (type > 254)
					return E_FILE_FORMAT_INVALID;
				info.type = static_cast<int32>(type);
				break;
			}
			case TrackInfo::ID_Name:
				info.name = reader2.read_ascii();
				break;

			case TrackInfo::ID_Language:
				info.language = reader2.read_ascii();
				break;

			case TrackInfo::ID_DefaultDuration:
				info.defaultDuration = reader2.read_uint();
				break;

			case TrackInfo::ID_Codec_ID:
				info.codec_id = reader2.read_ascii();
				break;

			case TrackInfo::ID_FlagLacing:
				lacing = reader2.read_uint();
				if (lacing > 1)
					return E_FILE_FORMAT_INVALID;
				break;

			case TrackInfo::ID_Codec_Private:
				reader2.readbuff(info.codec_private.create(reader2.size), reader2.size);
				break;

			case TrackInfo::ID_Codec_Name:
				info.codec_name	= reader2.read_ascii();
				break;

			case TrackInfo::ID_Codec_Delay:
				info.codec_delay = reader2.read_uint();
				break;

			case TrackInfo::ID_SeekPreRoll:
				info.seekPreRoll = reader2.read_uint();
				break;
		}
	}

	if (info.number == 0 || GetTrackByNumber(info.number) || info.type == 0)
		return E_FILE_FORMAT_INVALID;

	info.lacing = lacing != 0;

	switch (info.type) {
		case Track::kVideo: {
			if (v.start == 0 || a.start != 0)
				return E_FILE_FORMAT_INVALID;

			info.settings = v;
			VideoTrack* track	= new VideoTrack(segment, reader);
			const int32 status	= track->Parse(segment, info);
			if (status)
				return status;

			result = track;
			if (e.start != 0)
				result->ParseContentEncodingsEntry(e);
			return 0;  // success
		}
		case Track::kAudio: {
			if (a.start == 0 || v.start != 0)
				return E_FILE_FORMAT_INVALID;

			info.settings = a;
			AudioTrack* track = new AudioTrack(segment, reader);
			const int32 status = track->Parse(segment, info);
			if (status)
				return status;

			result = track;
			if (e.start != 0)
				result->ParseContentEncodingsEntry(e);
			return 0;  // success
		}
		default: {
			// neither video nor audio - probably metadata or subtitles
			if (a.start != 0 || v.start != 0 || (info.type == Track::kMetadata && e.start != 0))
				return E_FILE_FORMAT_INVALID;

			info.settings.start = maximum;
			info.settings.size	= 0;

			Track* track = NULL;
			const int32 status = Track::Create(segment, info, reader, track);
			if (status)
				return status;
			result = track;
			return 0;  // success
		}
	}

}

const Track* Tracks::GetTrackByNumber(int32 tn) const {
	if (tn >= 0) {
		for (auto i = begin(), e = end(); i != e; ++i) {
			if (*i && tn == (*i)->number)
				return *i;
		}
	}
	return NULL;  // not found
}

//-----------------------------------------------------------------------------
//	Chapters
//-----------------------------------------------------------------------------

int Chapters::Parse() {
	istream_ref	file = segment->file;
	file.seek(range.start);
	while (file.tell() < range.end()) {
		EBMLreader	reader2(file);
		if (reader2.id == ID_EditionEntry) {
			int status = editions.push_back().Parse(reader2);
			if (status < 0)
				return status;
		}
	}
	return 0;
}

int Chapters::Edition::Parse(EBMLreader &reader) {
	while (!reader.atend()) {
		EBMLreader	reader2(reader.child());
		if (reader2.id == ID_Atom) {
			int status = atoms.push_back().Parse(reader2);
			if (status < 0)
				return status;
		}
	}
	return 0;
}

int Chapters::Atom::Parse(EBMLreader &reader) {
	while (!reader.atend()) {
		EBMLreader	reader2(reader.child());
		switch (reader2.id) {
			case ID_Display: {
				int	status = displays.push_back().Parse(reader2);
				if (status < 0)  // error
					return status;
			}
			case ID_StringUID:
				string_uid = reader2.read_ascii();
				break;

			case ID_UID:
				uid	= reader2.read_int();
				break;

			case ID_TimeStart:
				start_timecode = reader2.read_uint();
				break;

			case ID_TimeEnd:
				stop_timecode = reader2.read_uint();
				break;
		}
	}
	return 0;
}

int64 Chapters::Atom::GetTime(const Chapters* chapters, int64 timecode) {
	if (chapters == NULL)
		return -1;

	Segment* const segment = chapters->segment;
	if (segment == NULL)  // weird
		return -1;

	const SegmentInfo* const info = segment->info;
	if (info == NULL)
		return -1;

	const int64 timecode_scale = info->timecode_scale;
	if (timecode_scale < 1 || timecode < 0)
		return -1;

	return timecode_scale * timecode;
}

int Chapters::Display::Parse(EBMLreader &reader) {
	while (!reader.atend()) {
		EBMLreader	reader2(reader.child());
		switch (reader2.id) {
			case ID_String:
				chapstring = reader2.read_ascii();
				break;
			case ID_Language:
				language = reader2.read_ascii();
				break;
			case ID_Country:
				country = reader2.read_ascii();
				break;
		}
	}
	return 0;
}

//-----------------------------------------------------------------------------
//	Tags
//-----------------------------------------------------------------------------

int Tags::Parse() {
	istream_ref	file = segment->file;
	file.seek(range.start);
	while (file.tell() < range.end()) {
		EBMLreader	reader2(file);
		if (reader2.id == ID_Tag) {
			int status = tags.push_back().Parse(reader2);
			if (status < 0)
				return status;
		}
	}
	return 0;
}

int Tags::Tag::Parse(EBMLreader &reader) {
	while (!reader.atend()) {
		EBMLreader	reader2(reader.child());
		if (reader2.id == ID_SimpleTag) {
			int status = simple_tags.push_back().Parse(reader2);
			if (status < 0)
				return status;
		}
	}
	return 0;
}

int Tags::SimpleTag::Parse(EBMLreader &reader) {
	while (!reader.atend()) {
		EBMLreader	reader2(reader.child());
		switch (reader2.id) {
			case ID_Name:
				tag_name = reader2.read_ascii();
				break;

			case ID_String:
				tag_string = reader2.read_ascii();
				break;
		}
	}
	return 0;
}

//-----------------------------------------------------------------------------
//	Cues
//-----------------------------------------------------------------------------

void Cues::Init() {
	if (empty()) {
		istream_ref	file = segment->file;
		file.seek(range.start);
		while (file.tell() < range.end()) {
			EBMLreader	reader2(file);
			if (reader2.id == CuePoint::ID) {
				int		n = size32();
				emplace_back(n - loaded_count, pos);
			}
		}
	}
}

bool Cues::LoadCuePoint() {
	if (DoneParsing() || loaded_count == size32())
		return false;  // nothing else to do

	Init();

	istream_ref	file = segment->file;
	file.seek(range.start);
	while (file.tell() < range.end()) {
		EBMLreader	reader2(file);
		if (reader2.id == CuePoint::ID) {
			CuePoint& cp = (*this)[loaded_count];
			if ((cp.timecode >= 0 || -cp.timecode == reader2.start) && cp.Load(reader2)) {
				++loaded_count;
				return true;  // yes, we loaded a cue point
			}
			break;
		}
	}

	return false;  // no, we did not load a cue point
}

const CuePoint::TrackPosition *Cues::Find(int64 time_ns, const Track* track, const CuePoint *&cp) const {
	if (time_ns < 0 || !track || loaded_count == 0)
		return 0;
#if 1
	cp = lower_bound(begin(), begin() + loaded_count, time_ns, [this](const CuePoint &cp, int64 time_ns) {
		return cp.GetTime(segment) < time_ns;
	});
	if (cp == begin())
		return 0;
	return (--cp)->Find(track);
#else
	CuePoint* i = cue_points;
	CuePoint* j = i + loaded_count;

	if (i->GetTime(segment) >= time_ns)
		return i->Find(track);

	while (i < j) {
		CuePoint* const k = i + (j - i) / 2;
		if (k->GetTime(segment) <= time_ns)
			i = k + 1;
		else
			j = k;
	}

	--i;
	if (i->GetTime(segment) > time_ns)
		return 0;

	// TODO: here and elsewhere, it's probably not correct to search for the cue point with this time, and then search for a matching
	// track.  In principle, the matching track could be on some earlier cue point, and with our current algorithm, we'd miss it.  To make
	// this bullet-proof, we'd need to create a secondary structure, with a list of cue points that apply to a track, and then search
	// that track-based structure for a matching cue point.
	cp = i;
	return i->Find(track);
#endif
}

const CuePoint* Cues::GetFirst() const {
	return loaded_count > 0 && begin()->timecode >= 0 ? begin() : 0;
}

const CuePoint* Cues::GetLast() const {
	return loaded_count > 0 && (*this)[loaded_count - 1].timecode >= 0 ? begin() + loaded_count - 1 : 0;
}

const CuePoint* Cues::GetNext(const CuePoint* cp) const {
	return cp && cp->timecode >= 0 && cp->index + 1 < loaded_count && cp[1].timecode >= 0 ? cp + 1 : 0;
}

const BlockEntry* Cues::GetBlock(const CuePoint *cp, const CuePoint::TrackPosition *tp) const {
	return cp && tp ? segment->GetBlock(*cp, *tp) : 0;
}

int64 CuePoint::GetTime(const Segment *segment) const {
	return segment->info->timecode_scale * timecode;
}

bool CuePoint::Load(EBMLreader &reader) {
	if (timecode >= 0)  // already loaded
		return true;

	while (!reader.atend()) {
		EBMLreader	reader2(reader.child());
		switch (reader2.id) {
			case ID_Time:
				timecode = reader2.read_uint();
				break;

			case TrackPosition::ID:
				if (!track_positions.push_back().Parse(reader2))
					return false;
				break;
		}
	}

	return timecode >= 0;
}

bool CuePoint::TrackPosition::Parse(EBMLreader &reader) {
	while (!reader.atend()) {
		EBMLreader	reader2(reader.child());
		switch (reader2.id) {
			case ID_Track:
				track = reader2.read_uint();
				break;
			case ID_ClusterPosition:
				pos = reader2.read_uint();
				break;
			case ID_BlockNumber:
				block = reader2.read_uint();
				break;
		}
	}
	return true;
}

const CuePoint::TrackPosition* CuePoint::Find(const Track* track) const {
	const int64 n = track->number;
	for (const TrackPosition *i = track_positions.begin(), *e = track_positions.end(); i != e; ++i) {
		if (i->track == n)
			return i;
	}
	return NULL;  // no matching track number found
}

//-----------------------------------------------------------------------------
//	Cluster
//-----------------------------------------------------------------------------
Cluster::~Cluster() {
	for_each(entries, [](BlockEntry *e) { delete e; });
}

Cluster* Cluster::Create(Segment* segment, int32 idx, const file_range &range) {
	return segment ? new Cluster(segment, idx, range) : 0;
}

int64 Cluster::GetTimeCode() {
	const int32 status = Load();
	if (status < 0)  // error
		return status;
	return timecode;
}

int64 Cluster::GetTime() {
	const int64 tc = GetTimeCode();
	return tc < 0 ? tc : tc * segment->info->timecode_scale;
}

int64 Cluster::GetFirstTime() {
	const BlockEntry*	entry;
	const int32			status = GetFirst(entry);
	return status < 0	? status	// error
		: !entry		? GetTime()	// empty cluster
		: entry->GetBlock()->GetTime(this);
}

int64 Cluster::GetLastTime() {
	const BlockEntry*	entry;
	const int32			status = GetLast(entry);
	return status < 0	? status	// error
		: !entry		? GetTime() // empty cluster
		: entry->GetBlock()->GetTime(this);
}

int Cluster::GetFirst(const BlockEntry*& first) {
	first = NULL;
	if (entries.empty()) {
		const int32 status = Parse();

		if (status < 0)  // error
			return status;

		if (entries.empty())
			return 0;
	}

	first = entries[0];
	return 0;  // success
}

int Cluster::GetLast(const BlockEntry*& last) {
	int32 status;
	while ((status = Parse()) == 0);

	if (status < 0)		// error
		return status;

	last = entries.empty() ? 0 : entries.back();
	return 0;
}

int Cluster::GetNext(const BlockEntry* curr, const BlockEntry*& next) {
	size_t idx = curr->index + 1;
	next = NULL;
	if (idx >= entries.size()) {
		const int32 status = Parse();

		if (status < 0)  // error
			return status;

		if (status > 0)
			return 0;
	}

	next = entries[idx];
	return 0;
}

int Cluster::Load() {
	if (segment == NULL)
		return E_PARSE_FAILED;

	if (timecode >= 0)  // at least partially loaded
		return 0;

	// pos points to start of payload
	int64	new_pos		= -1;
	bool	got_block	= false;
	istream_ref	file		= segment->file;
	uint64	stop		= range.end();
	file.seek(pos);

	while (file.tell() < stop) {
		EBMLreader	reader(file);

		// This is the distinguished set of ID's we use to determine that we have exhausted the sub-element's inside the cluster whose ID we parsed earlier.
		if (reader.id == ID || reader.id == Cues::ID)
			break;

		if (reader.unended() || pos > stop)
			return E_FILE_FORMAT_INVALID;

		switch (reader.id) {
			case ID_Timecode:
				timecode	= reader.read_uint();
				new_pos		= reader.end();
				if (got_block)
					break;
				break;

			case BlockGroup::ID:
			case SimpleBlock::ID:
				got_block = true;
				break;
		}
	}

	if (timecode < 0 || !got_block)  // no timecode found
		return E_FILE_FORMAT_INVALID;

	pos		= new_pos;  // designates position just beyond timecode payload
	return 0;
}

int Cluster::Parse() {
	uint64 total, avail;
	segment->GetAvail(total, avail);

	int32 status = Load();
	if (status < 0)
		return status;

	istream_ref		file	= segment->file;
	uint64		stop	= range.end();
	if (pos >= stop)
		return 1;  // nothing else to do

	file.seek(pos);

	while (file.tell() < stop) {
		EBMLreader	reader(file);

		// This is the distinguished set of ID's we use to determine that we have exhausted the sub-element's inside the cluster whose ID we parsed earlier.
		if (reader.id == ID || reader.id == Cues::ID) {
			if (range.unended())
				range.set_end(reader.start);
			break;
		}

		if (reader.unended())
			return E_FILE_FORMAT_INVALID;

		const int64 block_stop = reader.end();

		if (!range.unended()) {
			if (block_stop > range.end()) {
				if (reader.id == BlockGroup::ID || reader.id == SimpleBlock::ID)
					return E_FILE_FORMAT_INVALID;
				pos = range.end();
				break;
			}
		} else if (block_stop > total) {
			range.set_end(total);
			pos		= total;
			break;

		} else if (block_stop > avail) {
			return E_BUFFER_NOT_FULL;
		}

		pos = block_stop;

		if (reader.id == BlockGroup::ID)
			return ParseBlockGroup(reader);

		if (reader.id == SimpleBlock::ID)
			return ParseSimpleBlock(reader);
	}

	if (!entries.empty()) {
		const BlockEntry* const last	= entries.back();
		const Block* const block		= last->GetBlock();
		const int64 block_start			= block->range.start;
		const int64 block_stop			= block->range.end();

		if (block_start > total)
			return E_PARSE_FAILED;  // defend against trucated stream

		if (block_stop > range.end())
			return E_FILE_FORMAT_INVALID;

		if (block_stop > total)
			return E_PARSE_FAILED;  // defend against trucated stream
	}

	return 1;  // no more entries
}

int Cluster::ParseSimpleBlock(EBMLreader &reader) {
	uint64 total, avail;
	segment->GetAvail(total, avail);

	const int64 track	= reader.read_packed_num();
	if (track == 0)
		return E_FILE_FORMAT_INVALID;

	uint16	timecode	= reader.get<uint16>();
	uint8	flags		= reader.getc();

	if (Block::GetLacing(flags) && reader.end() > avail)
		return E_BUFFER_NOT_FULL;

	SimpleBlock	*block = new SimpleBlock(this, entries.size32(), reader);
	entries.push_back(block);
	return block->block.Parse(this);
}

int Cluster::ParseBlockGroup(EBMLreader &reader) {
	uint64 total, avail;
	segment->GetAvail(total, avail);

	int64 discard_padding = 0;

	// For WebM files, there is a bias towards previous reference times (in order to support alt-ref frames, which refer back to the previous keyframe).
	// Normally a 0 value is not possible, but here we tenatively allow 0 as the value of a reference frame, with the interpretation that this is a "previous" reference time.
	int64		prev		= 1;	// nonce
	int64		next		= 0;	// nonce
	int64		duration	= -1;	// really, this is unsigned
	file_range block_range(0, 0);

	while (!reader.atend()) {
		EBMLreader	reader2(reader.child());
		if (reader2.unended())
			return E_FILE_FORMAT_INVALID;

		switch (reader2.id) {
			case 0:
				return E_FILE_FORMAT_INVALID;

			case BlockGroup::ID_Block: {
				const int64 track = reader2.read_packed_num();
				if (track == 0)
					return E_FILE_FORMAT_INVALID;
				uint8	flags	= reader.getc();

				if (Block::GetLacing(flags) && reader.end() > avail)
					return E_BUFFER_NOT_FULL;

				if (block_range.start == 0)  // Block ID
					block_range = reader2;
				break;
			}

			case BlockGroup::ID_Duration:
				duration = reader2.read_uint();
				break;

			case BlockGroup::ID_Reference: {
				int64	time = reader2.read_int();
				(time <= 0 ? prev : next) = time;  // see note above
				break;
			}
			case BlockGroup::ID_DiscardPadding:
				discard_padding = reader2.read_uint();
				break;
		}
	}

	if (block_range.start == 0)
		return E_FILE_FORMAT_INVALID;

	BlockGroup	*block = new BlockGroup(this, entries.size32(), block_range, prev, next, duration);
	entries.push_back(block);

	int status = block->block.Parse(this);
	if (status)
		return status;

	if (prev > 0 && next <= 0)
		block->block.flags |= 0x80;		// make key

	if (discard_padding)
		block->block.flags |= 0x100;	// discard padding

	return 0;
}

const BlockEntry* Cluster::GetEntry(const Track* track, int64 time_ns) {
	const BlockEntry* result = 0;

	for (int32 index = 0; ; ++index) {
		if (index >= entries.size32()) {
			const int32 status = Parse();
			if (status > 0)  // completely parsed, and no more entries
				return result;

			if (status < 0)  // should never happen
				return 0;
		}

		const BlockEntry* const entry = entries[index];
		const Block* const		block = entry->GetBlock();

		if (block->track == track->number) {
			if (track->VetEntry(entry)) {
				if (time_ns < 0)  // just want first candidate block
					return entry;

				const int64 ns = block->GetTime(this);
				if (ns > time_ns)
					return result;

				result = entry;  // have a candidate

			} else if (time_ns >= 0) {
				const int64 ns = block->GetTime(this);
				if (ns > time_ns)
					return result;
			}
		}
	}
}

const BlockEntry* Cluster::GetEntry(const CuePoint& cp, const CuePoint::TrackPosition& tp) {
	const int64 tc = cp.timecode;

	if (tp.block > 0) {
		const int32 index = int32(tp.block) - 1;
		while (index >= entries.size32()) {
			const int32 status = Parse();
			if (status < 0)  // TODO: can this happen?
				return NULL;

			if (status > 0)  // nothing remains to be parsed
				return NULL;
		}

		const BlockEntry* const entry = entries[index];
		const Block* const		block = entry->GetBlock();

		if (block->track == tp.track && block->GetTimeCode(this) == tc)
			return entry;
	}

	for (int32 index = 0; ; ++index) {
		if (index >= entries.size32()) {
			const int32 status = Parse();
			if (status < 0)  // TODO: can this happen?
				return NULL;

			if (status > 0)  // nothing remains to be parsed
				return NULL;
		}

		const BlockEntry* const entry = entries[index];
		const Block* const		block = entry->GetBlock();
		if (block->track != tp.track)
			continue;

		const int64 tc_ = block->GetTimeCode(this);
		if (tc_ < tc)
			continue;

		if (tc_ > tc)
			return NULL;

		if (const Track *const track = segment->tracks->GetTrackByNumber(tp.track)) {
			if (track->type == Track::kAudio || (track->type == Track::kVideo && block->IsKey()))
				return entry;
		}
		return NULL;
	}
}

Cluster** Cluster::FindByTime(Cluster** const begin, Cluster** const end, int64 time) {
	return lower_bound(begin, end, time, [](Cluster *c, int64 time) {
		return c->GetTime() < time;
	});
}
Cluster** Cluster::FindByPos(Cluster** const begin, Cluster** const end, int64 pos) {
	return lower_bound(begin, end, pos, [](Cluster *c, int64 pos) {
		return c->range.start < pos;
	});
}

bool Cluster::HasBlockEntries(const Segment* segment, const file_range &range) {
	uint64 cluster_stop = range.end();
	if (!range.unended() && cluster_stop > segment->range.end())
		return E_FILE_FORMAT_INVALID;

	istream_ref	file = segment->file;
	file.seek(range.start);

	while (file.tell() < cluster_stop) {
		EBMLreader	reader(file);
		if (reader.id == Cluster::ID || reader.id == Cues::ID)
			break;

		if (reader.id == BlockGroup::ID || reader.id == SimpleBlock::ID)
			return true;  // have at least one entry
	}
	return false;  // no entries detected
}

//-----------------------------------------------------------------------------
//	Segment
//-----------------------------------------------------------------------------

int SegmentInfo::Parse() {
	timecode_scale	= 1000000;
	duration		= -1;

	istream_ref	file	= segment->file;
	file.seek(range.start);
	const int64 stop = range.end();

	while (file.tell() < stop) {
		EBMLreader	reader(file);
		switch (reader.id) {
			case ID_TimecodeScale:
				timecode_scale = reader.read_uint();
				break;

			case ID_Duration:
				duration = reader.read_float();
				if (duration < 0)
					return E_FILE_FORMAT_INVALID;
				break;

			case ID_MuxingApp:
				muxingApp = reader.read_ascii();
				break;

			case ID_WritingApp:
				writingApp = reader.read_ascii();
				break;

			case ID_Title:
				title = reader.read_ascii();
				break;
		}
	}

	return 0;
}

int SeekHead::Parse() {
	istream_ref	file		= segment->file;
	const int64 stop	= range.end();

#if 0
	file.seek(range.start);
	// first count the seek head entries
	int entry_count			= 0;
	int void_element_count	= 0;

	while (file.tell() < stop) {
		EBMLreader	reader(file);
		switch (reader.id) {
			case ID_Seek:
				++entry_count;
				break;
			case ID_Void:
				++void_element_count;
				break;
		}
	}

	entries.reserve(entry_count);
	void_elements.reserve(void_element_count);
#endif

	file.seek(range.start);
	while (file.tell() < stop) {
		EBMLreader	reader(file);
		switch (reader.id) {
			case ID_Seek: {
				Entry	*e = new(entries) Entry;
				if (!e->Parse(reader))
					entries.pop_back();
				break;
			}
			case ID_Void: {
				void_elements.emplace_back(reader);
				break;
			}
			break;
		}
	}
	return 0;
}

bool SeekHead::Entry::Parse(istream_ref file) {
	// Note that the SeekId payload really is serialized as a "Matroska integer", not as a plain binary value.
	// In fact, Matroska requires that ID values in the stream exactly match the binary representation as listed
	// in the Matroska specification.
	// This parser is more liberal, and permits IDs to have any width.  (This could make the representation in the stream
	// different from what's in the spec, but it doesn't matter here, since we always normalize "Matroska integer" values.)
	{
		EBMLreader reader2(file);
		if (reader2.id != ID_SeekID)
			return false;
		id = reader2.read_packed_num();
	}
	{
		EBMLreader reader2(file);
		if (reader2.id != ID_SeekPosition)
			return false;
		pos = reader2.read_uint();
	}
	return true;
}

Segment::~Segment() {
	for_each(clusters, [](Cluster *c) { delete c; });
	delete tracks;
	delete info;
	delete cues;
	delete chapters;
	delete tags;
	delete seekHead;
}

int64 Segment::CreateInstance(istream_ref file, int64 pos, Segment*& segment) {
	segment = NULL;
	file.seek(pos);
	for (;;) {
		EBMLreader	reader(file);
		if (reader.id == Segment::ID) {
			segment = new Segment(file, reader);
			return 0;  // success
		}
	}
}

int64 Segment::ParseHeaders() {
	while (file.tell() < range.end()) {
		EBMLreader	reader2(file);

		if (reader2.id == Cluster::ID)
			break;

		switch (reader2.id) {
			case 0:
				return E_FILE_FORMAT_INVALID;

			case SegmentInfo::ID:
				if (info)
					return E_FILE_FORMAT_INVALID;
				info = new SegmentInfo(this, reader2);
				if (int status = info->Parse())
					return status;
				break;

			case Tracks::ID:
				if (tracks)
					return E_FILE_FORMAT_INVALID;
				tracks = new Tracks(this, reader2);
				if (int status = tracks->Parse())
					return status;
				break;

			case Cues::ID:
				if (cues == NULL)
					cues = new Cues(this, reader2);
				break;

			case SeekHead::ID:
				if (seekHead == NULL) {
					seekHead = new SeekHead(this, reader2);
					const int32 status = seekHead->Parse();
					if (status)
						return status;
				}
				break;

			case Chapters::ID:
				if (chapters == NULL) {
					chapters = new Chapters(this, reader2);
					const int32 status = chapters->Parse();
					if (status)
						return status;
				}
				break;

			case Tags::ID:
				if (tags == NULL) {
					tags = new Tags(this, reader2);
					const int32 status = tags->Parse();
					if (status)
						return status;
				}
				break;
		}
	}

	if (!info)  // TODO: liberalize this behavior
		return E_FILE_FORMAT_INVALID;

	if (!tracks)
		return E_FILE_FORMAT_INVALID;

	return 0;  // success
}

int Segment::LoadCluster() {
	file.seek(pos);
	for (;;) {
		const int result = DoLoadCluster();
		if (result <= 1)
			return result;
	}
}

int Segment::DoLoadCluster() {
	if (unknownSize) {
		const int32 status = unknownSize->Parse();
		if (status < 0)		// error or underflow
			return status;

		if (status == 0)	// parsed a block
			return 2;		// continue parsing

		unknownSize = 0;
		return 2;			// continue parsing
	}

	uint64 total, avail;
	GetAvail(total, avail);

	file_range cluster_range(0, 0);
	const int64 segment_stop = min(total, range.end());

	while (cluster_range.size == 0) {
		if (file.tell() >= segment_stop)
			return 1;  // no more clusters

		EBMLreader	reader2(file);
		if (file.tell() >= avail)
			return E_BUFFER_NOT_FULL;

		switch (reader2.id) {
			case Cues::ID: {
				if (reader2.unended())
					return E_FILE_FORMAT_INVALID;		// Cues element of unknown size: Not supported.

				if (cues == NULL)
					cues = new Cues(this, reader2);
				break;
			}

			case Cluster::ID:
				// Besides the Segment, Libwebm allows only cluster elements of unknown size
				// Fail the parse upon encountering a non-cluster element reporting unknown size.
				if (reader2.unended())
					return E_FILE_FORMAT_INVALID;
				cluster_range = reader2;
				break;
		}
	}

	int	status = Cluster::HasBlockEntries(this, cluster_range);
	if (status < 0)  // error, or underflow
		return status;

	// status == 0 means "no block entries found"
	// status > 0 means "found at least one block entry"

	// TODO:
	// The issue here is that the segment increments its own pos ptr past the most recent cluster parsed, and then starts from there to parse the next cluster.  If we
	// don't know the size of the current cluster, then we must either parse its payload (as we do below), looking for the cluster (or cues) ID to terminate the parse.
	// This isn't really what we want: rather, we really need a way to create the curr cluster object immediately.
	// The pity is that cluster::parse can determine its own boundary, and we largely duplicate that same logic here.
	//
	// Maybe we need to get rid of our look-ahead preloading in source::parse???
	//
	// As we're parsing the blocks in the curr cluster (in cluster::parse), we should have some way to signal to the segment that we have determined the boundary,
	// so it can adjust its own segment::pos member.
	//
	// The problem is that we're asserting in asyncreadinit, because we adjust the pos down to the curr seek pos, and the resulting adjusted len is > 2GB.  I'm suspicious
	// that this is even correct, but even if it is, we can't be loading that much data in the cache anyway.

	const int32 idx = clusters.size32();

	if (clusters.size() > loaded_clusters) {
		Cluster* const cluster = clusters[idx];
		if (cluster == NULL || cluster->index >= 0)
			return E_FILE_FORMAT_INVALID;

		if (cluster->range.start == cluster_range.start) {  // preloaded already
			if (status == 0)  // no entries found
				return E_FILE_FORMAT_INVALID;

			if (cluster_range.size)
				pos = cluster_range.end();
			else
				pos = cluster->range.end();

			cluster->index = idx;  // move from preloaded to loaded
			++loaded_clusters;
			return 0;  // success
		}
	}

	if (status == 0) {  // no entries found
		if (cluster_range.size)
			pos = cluster_range.end();

		if (pos >= range.end()) {
			pos = range.end();
			return 1;  // no more clusters
		}
		return 2;  // try again
	}

	// status > 0 means we have an entry
	Cluster* const cluster = new Cluster(this, idx, cluster_range);

	if (cluster == NULL)
		return -1;

	if (!AppendCluster(cluster)) {
		delete cluster;
		return -1;
	}

	if (cluster_range.size) {
		pos = cluster_range.end();
		if (pos > range.end())
			return E_FILE_FORMAT_INVALID;

		return 0;
	}

	unknownSize = cluster;
	return 0;  // partial success, since we have a new cluster

	// status == 0 means "no block entries found"
	// pos designates start of payload
	// pos has NOT been adjusted yet (in case we need to come back here)
}

bool Segment::AppendCluster(Cluster* cluster) {
	if (cluster == NULL || cluster->index < 0)
		return false;
	clusters.insert(clusters + loaded_clusters++, cluster);
	return true;
}

bool Segment::PreloadCluster(Cluster* cluster, uint32 idx) {
	if (cluster == NULL || cluster->index >= 0 || idx < loaded_clusters)
		return false;
	clusters.insert(clusters + idx, cluster);
	return true;
}

// Outermost (level 0) segment object has been constructed, and pos designates start of payload.  We need to find the inner (level 1) elements.

int Segment::Load() {
	file.seek(pos = range.start);
	const int64 header_status = ParseHeaders();

	if (header_status < 0)  // error
		return static_cast<int32>(header_status);

	if (header_status > 0)  // underflow
		return E_BUFFER_NOT_FULL;

	if (!info || !tracks)
		return E_FILE_FORMAT_INVALID;

	for (;;) {
		const int status = LoadCluster();
		if (status < 0)  // error
			return status;
		if (status >= 1)  // no more clusters
			return 0;
	}
}

const BlockEntry* Segment::GetBlock(const CuePoint &cp, const CuePoint::TrackPosition &tp) {
	Cluster	*cluster = FindOrPreloadCluster(tp.pos);
	return cluster ? cluster->GetEntry(cp, tp) : 0;
}

Cluster* Segment::FindOrPreloadCluster(uint64 requested_pos, int from) {
	if (requested_pos <= 0)
		return 0;

	Cluster	**i = Cluster::FindByPos(clusters.begin() + from, clusters.end(), requested_pos);
	if (i != clusters.end() && (*i)->range.start == requested_pos)
		return *i;

	Cluster	*next = Cluster::Create(this, -1, file_range(range.start + requested_pos));
	if (next) {
		if (!PreloadCluster(next, i - clusters)) {
			delete next;
			next = 0;
		}
	}
	return next;
}

bool Segment::DoneParsing() const {
	if (range.unended()) {
		uint64 total, avail;
		GetAvail(total, avail);
		return pos >= total;
	}
	return pos >= range.end();
}

Cluster* Segment::GetFirst() {
	return loaded_clusters == 0 ? NULL : clusters.front();
}

Cluster* Segment::GetLast() {
	return loaded_clusters == 0 ? NULL : clusters.back();
}

Cluster* Segment::GetNext(const Cluster *cluster) {
	int32 idx = cluster->index;
	if (idx >= 0) {
		if (++idx >= loaded_clusters)
			return NULL;  // caller will LoadCluster as desired
		return clusters[idx];
	}

	file.seek(cluster->range.start);
	EBMLreader	reader(file);
	if (reader.id != Cluster::ID)
		return NULL;

	file_range cluster_range(0, 0);

	while (!reader.atend()) {
		EBMLreader	reader2(file);
		if (reader2.id == Cluster::ID) {
			const int32 status = Cluster::HasBlockEntries(this, reader2);
			if (status > 0) {
				cluster_range = reader2;
				break;
			}
		}
	}

	return FindOrPreloadCluster(cluster_range.start, loaded_clusters);
}

int Segment::ParseNext(Cluster* cluster, Cluster*& result) {
	result = 0;

	if (cluster->index >= 0) {  // loaded (not merely preloaded)
		const int32 next_idx = cluster->index + 1;
		if (next_idx < loaded_clusters) {
			result = clusters[next_idx];
			return 0;  // success
		}
		// curr cluster is last among loaded
		const int32 status = LoadCluster();

		if (status < 0)  // error or underflow
			return status;

		if (status > 0)  // no more clusters
			return 1;

		result = GetLast();
		return 0;  // success
	}

	uint64 total, avail;
	GetAvail(total, avail);

	const int64 segment_stop = range.end();

	// interrogate curr cluster
	pos = cluster->range.start;

	if (!cluster->range.unended()) {
		pos += cluster->range.size;
	} else {
		if (pos > avail)
			return E_BUFFER_NOT_FULL;
		file.seek(pos);
		EBMLreader	reader(file);
		if (reader.unended())  // TODO: should never happen
			return E_FILE_FORMAT_INVALID;  // TODO: resolve this

		pos = reader.end();
		// By consuming the payload, we are assuming that the curr cluster isn't interesting.  That is, we don't bother checking
		// whether the payload of the curr cluster is less than what happens to be available (obtained via IMkvReader::Length).
		// Presumably the caller has already dispensed with the current cluster, and really does want the next cluster.
	}

	// pos now points to just beyond the last fully-loaded cluster
	for (;;) {
		const int32 status = DoParseNext(result);
		if (status <= 1)
			return status;
	}
}

int Segment::DoParseNext(Cluster *&result) {
	uint64 total, avail;
	GetAvail(total, avail);

	const int64 segment_stop = min(total, range.end());

	// Parse next cluster. Creation of a new cluster object happens later
	file_range cluster_range(0, 0);
	for (;;) {
		if (pos >= segment_stop)
			return 1;  // EOF

		if (pos >= avail)
			return E_BUFFER_NOT_FULL;

		file.seek(pos);
		EBMLreader	reader(file);

		if (file.tell() >= avail)
			return E_BUFFER_NOT_FULL;

		if (reader.unended() || reader.end() > segment_stop)
			return E_FILE_FORMAT_INVALID;

		if (reader.id == Cues::ID) {
			if (!cues)
				cues = new Cues(this, reader);
			continue;
		}

		if (reader.id == Cluster::ID) {
			// We have a cluster.
			cluster_range	= reader;
			break;
		}
	}

	// We have parsed the next cluster.
	// We have not created a cluster object yet, so determine whether it has already been preloaded
	//(in which case, an object for this cluster has already been created), and if not, create a new cluster object.

	Cluster	**i = Cluster::FindByPos(clusters.begin() + loaded_clusters, clusters.end(), cluster_range.start);
	if (i != clusters.end() && (*i)->range.start == cluster_range.start) {
		result = *i;
		return 0;
	}

	int status = Cluster::HasBlockEntries(this, cluster_range);
	if (status < 0)  // error or underflow
		return status;

	if (status > 0) {  // means "found at least one block entry"
		Cluster* const next = Cluster::Create(this, -1, cluster_range);
		if (next == NULL)
			return -1;

		if (!PreloadCluster(next, i - clusters)) {
			delete next;
			return -1;
		}
		result = next;
		return 0;  // success
	}

	// status == 0 means "no block entries found"
	if (cluster_range.unended()) {  // unknown size
		const int64 payload_pos = pos;  // absolute pos of cluster payload

		while (file.tell() < min(total, segment_stop))  {  // determine cluster size
			EBMLreader	reader(file);
			if (reader.id == Cluster::ID || reader.id == Cues::ID)
				break;

			if (reader.unended())
				return E_FILE_FORMAT_INVALID;  // not allowed for sub-elements
		}

		cluster_range.set_end(file.tell());
		pos = payload_pos;  // reset and re-parse original cluster
	}

	pos = cluster_range.end();  // consume payload
	return 2;  // try to find a cluster that follows next
}

Cluster* Segment::FindCluster(int64 time_ns) {
	if (loaded_clusters == 0)
		return NULL;

	Cluster **i = Cluster::FindByTime(clusters.begin(), clusters.begin() + loaded_clusters, time_ns);
	return i == clusters.begin() ? *i : i[-1];
}

int Segment::GetAvail(uint64 &total, uint64 &avail) const {
	total = maximum;
	avail = maximum;
	return 0;
}

//-----------------------------------------------------------------------------
//	TrackReader
//-----------------------------------------------------------------------------

Block::Frame *TrackReader::GetNextFrame() {
	do {
		long status = 0;
		if (block_entry == NULL) {
			status				= cluster->GetFirst(block_entry);
			block_frame_index	= 0;

		} else if (block_frame_index == block_entry->block.frames.size() || block_entry->block.track != track_index) {
			status = cluster->GetNext(block_entry, block_entry);
			if (block_entry == NULL) {
				cluster = segment->GetNext(cluster);
				if (cluster == NULL)
					return NULL;
				continue;
			}
			block_frame_index = 0;
		}
		if (status)
			return NULL;

	} while (!block_entry || block_entry->block.track != track_index);

	return &block_entry->block.frames[block_frame_index++];
}

Block::Frame *MultiTrackReader::GetNextFrame(int *track_index) {
	do {
		long status = 0;
		if (block_entry == NULL) {
			status				= cluster->GetFirst(block_entry);
			block_frame_index	= 0;

		} else if (block_frame_index == block_entry->block.frames.size() || !(track_filter & (uint64(1) << block_entry->block.track))) {
			status = cluster->GetNext(block_entry, block_entry);
			if (block_entry == NULL) {
				cluster = segment->GetNext(cluster);
				if (cluster == NULL)
					return NULL;
				continue;
			}
			block_frame_index = 0;
		}
		if (status)
			return NULL;

	} while (!block_entry || !(track_filter & (uint64(1) << block_entry->block.track)));

	*track_index = block_entry->block.track;
	return &block_entry->block.frames[block_frame_index++];
}

} // namespace matroska

//-----------------------------------------------------------------------------
//	MKVFileHandler
//-----------------------------------------------------------------------------

using namespace iso;

class MKVFileHandler : public FileHandler {
	const char*		GetExt() override { return "mkv"; }
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
	bool			Write(ISO_ptr<void> p, ostream_ref file) override;
} mkv;

ISO_ptr<void> ReadEBML(istream_ref file, int level) {
	EBMLreader	ebml(file);
	if (ebml.id == 0)
		return ISO_NULL;

	char		id[64];
	sprintf(id, "m%X", ebml.id);

	if (ebml.size <= 8)
		return ISO_ptr<uint32>(id, uint32(ebml.read_uint()));
	if (level == 0)
		return ISO_ptr<void>(id);

	ISO_ptr<anything>	p(id);
	while (!ebml.atend())
		p->Append(ReadEBML(file, level - 1));

	return p;
}

ISO_ptr<void> MKVFileHandler::Read(tag id, istream_ref file) {
	/*
		EBMLread	ebml(file);
		if (ebml.id != EBML_ID_HEADER)
			return ISO_NULL;

		char	*doctype	= NULL;
		int		version		= 0;

		while (!ebml.atend()) {
			EBMLread	ebml2(file);
			uint64		num;
			switch (ebml2.id) {
				case EBML_ID_EBMLREADVERSION: // is our read version uptodate?
					num = ebml2.read_uint();
					if (num > EBML_VERSION)
						return ISO_NULL;
					break;
				case EBML_ID_EBMLMAXSIZELENGTH:	// we only handle 8 byte lengths at max
					num = ebml2.read_uint();
					if (num > sizeof(uint64))
						return ISO_NULL;
					break;
				case EBML_ID_EBMLMAXIDLENGTH:	// we handle 4 byte IDs at max
					num = ebml2.read_uint();
					if (num > sizeof(uint32))
						return ISO_NULL;
					break;
				case EBML_ID_DOCTYPE: {
					char *text = ebml2.read_ascii();
					if (doctype)
						free(doctype);
					doctype = text;
					break;
				}
				case EBML_ID_DOCTYPEREADVERSION:
					version = ebml2.read_uint();
					break;
				default:
				case EBML_ID_VOID:
				case EBML_ID_EBMLVERSION:
				case EBML_ID_DOCTYPEVERSION:
					break;
			}
		}
	*/
	ISO_ptr<anything>	p(id);
	while (ISO_ptr<void> p2 = ReadEBML(file, 3))
		p->Append(p2);
	return p;
}

bool MKVFileHandler::Write(ISO_ptr<void> p, ostream_ref file) {
	{
		EBMLwriter	ebml(file, EBMLHeader::ID);
		ebml.write_uint(EBMLHeader::ID_EBML_Version,		1);
		ebml.write_uint(EBMLHeader::ID_EBML_ReadVersion,	1);
		ebml.write_uint(EBMLHeader::ID_EBML_MaxIDLength,	4);
		ebml.write_uint(EBMLHeader::ID_EBML_MaxSizeLength,	8);
		ebml.write(EBMLHeader::ID_DocType, "matroska");
		ebml.write_uint(EBMLHeader::ID_DocTypeVersion,		2);
		ebml.write_uint(EBMLHeader::ID_DocTypeReadVersion,	2);
	}
	{
		EBMLwriter	ebml(file, matroska::Segment::ID);
	}
	return true;
}

