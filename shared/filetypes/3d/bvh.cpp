#include "iso/iso_files.h"
#include "iso/iso_convert.h"
#include "scenegraph.h"
#include "extra/text_stream.h"

using namespace iso;

class BVHFileHandler : FileHandler {
	const char*		GetExt() override { return "bvh";			}
	const char*		GetDescription() override { return "Biovision hierarchical data";	}
	ISO_ptr<void>	Read(tag id, istream_ref file) override;
//	bool			Write(ISO_ptr<void> p, ostream_ref file) override;
} bvh;

ISO_ptr<anything> ReadHierarchy(tag id, text_reader<istream_ref> &text, fixed_string<1024> &line, int &channels) {
	text.read_line(line);
	if (string_scan(line).get_token() != "{")
		return ISO_NULL;

	ISO_ptr<anything>	p(id);

	for (;;) {
		text.read_line(line);
		string_scan	s(line);
		count_string	t = s.get_token();

		if (t == "}")
			return p;

		if (t == "End") {
			//if (s.get_token() == "Site")
			text.read_line(line);
			if (string_scan(line).get_token() == "{") {
				text.read_line(line);
				string_scan	s(line);
				if (s.get_token() == "OFFSET") {
					float	x = s.get(), y = s.get(), z = s.get();
					p->Append(ISO_ptr<array<float,3> >("end", make_array(x, y, z)));
				}
				text.read_line(line);
			}
			continue;
		}

		if (t == "OFFSET") {
			float	x = s.get(), y = s.get(), z = s.get();
			p->Append(ISO_ptr<array<float,3> >("offset", make_array(x, y, z)));

		} else if (t == "CHANNELS") {
			int			n = s.get();
			channels	+= n;

		} else if (t == "JOINT") {
			ISO_ptr<anything>	p2 = ReadHierarchy(s.getp(), text, line, channels);
			p->Append(p2);
		}
	}
}

ISO_ptr<Bone> ReadBone(ISO_ptr<BasePose> &bp, ISO_ptr<Animation> &an, tag id, text_reader<istream_ref> &text, fixed_string<1024> &line, int &channels) {
	text.read_line(line);
	if (string_scan(line).get_token() != "{")
		return ISO_NULL;

	ISO_ptr<Bone>		p(id);
	ISO_ptr<Animation>	a(id);
	p->basepose	= identity;
	bp->Append(p);
	an->Append(a);

	for (;;) {
		text.read_line(line);
		string_scan	s(line);
		count_string	t = s.get_token();

		if (t == "}")
			return p;

		if (t == "End") {
			//if (s.get_token() == "Site")
			text.read_line(line);
			if (string_scan(line).get_token() == "{") {
				text.read_line(line);
				string_scan	s(line);
				if (s.get_token() == "OFFSET") {
					p->basepose.w.x = s.get();
					p->basepose.w.y = s.get();
					p->basepose.w.z = s.get();
				}
				text.read_line(line);
			}
			continue;
		}

		if (t == "OFFSET") {
			p->basepose.w.x = s.get();
			p->basepose.w.y = s.get();
			p->basepose.w.z = s.get();

		} else if (t == "CHANNELS") {
			int			n = s.get();
			channels	+= n;
			bool		pos = false, rot = false;
			while (n--) {
				count_string	t = s.get_token();
				if (t == "Xposition" || t == "Yposition" || t == "Zposition")
					pos = true;
				else if (t == "Zrotation" || t == "Yrotation" || t == "Xrotation")
					rot = true;
			}
			if (pos)
				a->Append(ISO_ptr<ISO_openarray<float3p> >("pos"));
			if (rot)
				a->Append(ISO_ptr<ISO_openarray<float4p> >("rot"));

		} else if (t == "JOINT") {
			ISO_ptr<Bone>	p2 = ReadBone(bp, an, s.get_token(), text, line, channels);
			p2->parent	= p;
		}
	}
}

ISO_ptr<void> BVHFileHandler::Read(tag id, istream_ref file) {
	auto	text = make_text_reader(file);
	fixed_string<1024>	line;

	text.read_line(line);
	if (line != "HIERARCHY")
		return ISO_NULL;

	ISO_ptr<anything>	p(id);
	ISO_ptr<BasePose>	bp(0);
	ISO_ptr<Animation>	an(0);
	p->Append(bp);
	p->Append(an);
	int					channels = 0;
	for (;;) {
		text.read_line(line);
		string_scan	s(line);
		if (s.get_token() != "ROOT")
			break;
		ReadBone(bp, an, s.get_token(), text, line, channels);
//		p->Append(ReadHierarchy(s.getp(), file, line, channels));
	}

	if (line == "MOTION") {
		int		num_frames = 0;
		float	frame_time = 0;
		string	line;

		for (;;) {
			text.read_line(line);

			if (line.begins("Frames:"))
				num_frames = string_scan(line + 7).get();
			else if (line.begins("Frame Time:"))
				frame_time = string_scan(line + 12).get();
			else
				break;
		}
		for (int f = 0; f < num_frames; f++) {
			string_scan	s(line);
#if 1
			ISO::Browser	b(an);
			for (auto i : b) {
				for (auto j = i.begin(), je = i.end(); j != je; ++j) {
					tag2	name	= j.GetName();
					if (name == "pos") {
						float	x = s.get(), y = s.get(), z = s.get();
						*((float3p*)j->Append()) = float3{x, y, z};
					} else if (name == "rot") {
						float	x = s.get(), y = s.get(), z = s.get();
						quaternion	q = rotate_in_x(degrees(x)) * rotate_in_y(degrees(y)) * rotate_in_z(degrees(z));
						*((float4p*)j->Append()) = q.v;
					}
				}
			}
#else

			ISO_ptr<ISO_openarray<float> >	frame(0);
			float	*f = frame->Create(channels);
			for (int j = 0; j < channels; j++)
				*f++ = s.get();
			p->Append(frame);
#endif
			text.read_line(line);
		}
	}
	return p;
}
