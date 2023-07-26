#ifdef PLAT_PC
#include <cmath>
#define USING_VSMATH
#endif

#define ISO_TEST

#include "main.h"
#include "iso/iso_script.h"
#include "iso/iso_binary.h"
#include "base/algorithm.h"
#include "base/vector.h"
#include "extra/xml.h"
#include "comms/HTTP.h"
#include "extra/date.h"
#include "filetypes/sound/sample.h"
#include "filetypes/sound/xma.h"
#include "comms/zlib_stream.h"
#include "comms/WebSocket.h"
#include "directory.h"
#include "scenegraph.h"
#include "systems/communication/isolink.h"
#include "systems/conversion/platformdata.h"
#include "systems/mesh/patch.h"
#include "systems/mesh/nurbs.h"
#include "systems/mesh/model_iso.h"
#include "vm.h"
#include "jobs.h"
#include "sound.h"
#include "linq.h"
#include "maths/polynomial.h"

namespace iso {
	ISO_INIT(NurbsModel)	{}
	ISO_DEINIT(NurbsModel)	{}
}

using namespace isocmd;

//-----------------------------------------------------------------------------
//	Globals
//-----------------------------------------------------------------------------

ISO_ptr<anything> Info(holder<ISO_ptr<void> > _p) {
	ISO_ptr<void>		p = _p;
	ISO_ptr<anything>	a(p.ID());
	if (p.IsType<sample>()) {
		sample			*s = p;
		a->Append(ISO_ptr<int>	("Channels",	s->Channels()));
		a->Append(ISO_ptr<int>	("Bits",		s->Bits()));
		a->Append(ISO_ptr<float>("Frequency",	s->Frequency()));
		a->Append(ISO_ptr<int>	("Length",		s->Length()));
		a->Append(ISO_ptr<float>("Duration",	s->Length() / s->Frequency()));
		a->Append(ISO_ptr<bool8>("Looping",		!!(s->Flags() & sample::LOOP)));
		a->Append(ISO_ptr<bool8>("Music",		!!(s->Flags() & sample::MUSIC)));
	} else if (p.IsType<bitmap>()) {
		bitmap	*s = p;
		a->Append(ISO_ptr<int>	("Width",		s->BaseWidth()));
		a->Append(ISO_ptr<int>	("Height",		s->BaseHeight()));
		a->Append(ISO_ptr<int>	("Depth",		s->Depth()));
		a->Append(ISO_ptr<int>	("Mips",		s->Mips()));
	} else if (p.IsType<anything>()) {
		ISO::Browser	b(p);
		for (int i = 0, n = b.Count(); i < n; i++)
			a->Append(Info((ISO_ptr<void>)b[i]));
	}
	return a;
}

#if 1
initialise init(
	ISO::getdef<float4p>(),
	ISO::getdef<AnimationEventKey>(),
	ISO::getdef<SubMesh>(),
	ISO::getdef<SampleBuffer>(),
	ISO::getdef<SoundList>(),
	ISO_get_operation(Info)
);
#endif

//-----------------------------------------------------------------------------
//	Helpers
//-----------------------------------------------------------------------------

void RemoveDupes(ISO_ptr<void> p, int flags = 0);
void ShareExternals(ISO_ptr<void> p, const filename &dir, const char *externals, bool dontconvert);
void ListExternals(const Cache &cache, const filename &fn, const filename &outfn);
void WriteCHeader(ISO_ptr<void> p, ostream_ref file);

streamptr copy_stream(istream_ref in, ostream_ref out, streamptr size=0, uint32 chunk=32768) {
	malloc_block	buffer(chunk);
	streamptr		total = 0;

	while (uint32 read = in.readbuff(buffer, chunk))
		total += out.writebuff(buffer, read);

	return total;
}

void PrintInfo(ISO::Browser b) {
	if (b.Is<sample>()) {
		sample	*s = b;
		printf(
			"Channels:    %i\n"
			"Bits:        %i\n"
			"Sample rate: %f\n"
			"Length:      %u (%fs)%s%s\n"
			, s->Channels()
			, s->Bits()
			, s->Frequency()
			, s->Length(), s->Length() / s->Frequency()
			, (s->Flags() & sample::LOOP) ? " looping" : ""
			, (s->Flags() & sample::MUSIC) ? " music" : ""
		);
	} else if (b.Is<bitmap>()) {
		bitmap	*s = b;
		printf(
			"Type:     %s\n"
			"Dimensions:%i x %i x %i\n"
			"Mips:     %i\n"
			, s->IsCube() ? "Cube" : s->IsVolume() ? "Volume" : s->Depth() > 1 ? "Array" : "2D"
			, s->BaseWidth(), s->BaseHeight(), s->Depth()
			, s->Mips()
		);
	} else if (b.Is<anything>()) {
		for (int i = 0, n = b.Count(); i < n; i++) {
			printf("\n%s:\n", (const char*)b.GetName(i).get_tag());
			PrintInfo(*b[i]);
		}
	}
}

_iso_debug_print_t prev_iso_debug_print;
void iso_stdout_print(void*, const char *s) {
#ifndef PLAT_MAC
	prev_iso_debug_print(s);
#endif
	fputs(s, stdout);
//	printf(s);
}

//-----------------------------------------------------------------------------
//	CommandLine
//-----------------------------------------------------------------------------

struct Options {
	bool	help, version, quiet, printargs, devstudio, copy, timer;
	int		num_procs;

	static bool get_opt(const char opt) {
		return opt != '-';
	}
	void	SetOption(const char *line) {
		switch (line[1]) {
			case 'c': copy		= get_opt(line[2]);	break;
			case 'v': version	= get_opt(line[2]);	break;
			case 't': timer		= get_opt(line[2]);	break;
			case 'a': printargs	= get_opt(line[2]);	break;
			case 'm':
				num_procs = is_whitespace(line[2]) ? 0 : from_string<int>(line + 2);
				if (num_procs <= 0)
					num_procs += from_string<int>(ISO::root("variables")["NUMBER_OF_PROCESSORS"].GetString(getenv("NUMBER_OF_PROCESSORS")));
				break;
			default:
				help	= true;
				break;
		}
	}
	Options() {
		clear(*this);
	}
};

struct CommandLine : static_array<string, 3>, Options {
	string				script;

	ISO::Browser		vars;
	bool				keepexternals;
	bool				dontconvert;
	bool				checkdate;
	filetime_t			tool_time;
	const char			*listexternals;
	iso::timer			time;

	CommandLine(const char *line);
	CommandLine(int argc, const char *argv[]);

	void	printf(const char *format, ...) {
		if (!quiet) {
			va_list valist;
			va_start(valist, format);

			buffer_accum<512>	fs;
			if (timer)
				fs << "t=" << (float)time << ": ";
			fs.vformat(format, valist);
			_iso_debug_print(fs.term());
		}
	}
	void			InitFlags();
	void			Error(const char *error, const char *filename2);
	bool			MaybeRemoveDupes(ISO_ptr<void> p);
	bool			CheckDate(const filename &fn1, const filename &fn2);

	ISO_ptr<void>	ReadSingle(tag id, const filename &fn);
	ISO_ptr<void>	ReadWild(tag id, const filename &fn, bool raw);
	bool			WriteSingle(ISO_ptr<void> &p, const filename &fn);
	bool			WriteWild(ISO_ptr<void> &p, const filename &fn);
	void			ProcessSingle(const filename &fnin, const filename &fnout, bool extract);
	void			ProcessWild(const filename &fnin, const filename &fnout, bool extract);
};

void CommandLine::InitFlags() {
	keepexternals	= vars["keepexternals"].GetInt() != 0;
	dontconvert		= vars["dontconvert"].GetInt() != 0;
	listexternals	= vars["listexternals"].GetString();
	quiet			= vars["quiet"].GetInt() != 0;

	int	_checkdate	= vars["checkdate"].GetInt();
	checkdate		= _checkdate != 0;
	if (_checkdate > 1)
		tool_time = filetime_write(get_exec_path());
}

CommandLine::CommandLine(const char *line) {
	vars				= ISO::root("variables");
	devstudio			= true;
	string	saveargs	= line;
	while (*line) {
		while (is_whitespace(*line))
			++line;
		if (*line == '"') {
			const char	*start = ++line;
			while (*line && *line != '"') line++;
			if (size() < capacity())
				push_back(str(start, line));
			line += int(*line != 0);

		} else if (*line == '{') {
			const char	*start = ++line;
			int		open	= 1;
			do {
				if (*line == 0)
					break;
				open += (*line == '{') - (*line == '}');
				++line;
			} while (open);
			script	= count_string(start, line - 1);
			if (size() == 1)
				push_back(script.begin());

		} else if (*line == '+') {
			const char	*start = ++line;
			while (*line && !is_whitespace(*line))
				line++;
			ISO::UserTypeArray	types;
			if (!ISO::ScriptReadDefines(filename(str(start, line)), types))
				throw_accum("Can't open " << start);

		} else if (*line == '/' || *line == '-') {
			SetOption(line);
			while (*line && !is_whitespace(*line))
				++line;

		} else if (*line) {
			const char	*start	= line;
			const char	*space	= strchr(start, ' ');
			const char	*equals = strchr(start, '=');
			if (equals && (!space || space > equals)) {
				const char *colon = strchr(start, ':');
				if (!colon || colon > equals) {
					const char	*value = equals + 1;
					if (*value == '{') {
						memory_reader	mi(const_memory_block(value + 1, strlen(value + 1)));
						vars.Append().Set(ISO::ScriptRead(str(start, equals), 0, mi, ISO::SCRIPT_NOCHECKEND));
						line = equals + 2 + uint32(mi.tell());
						if (*line++ != '}')
							throw("Missing '}'");
					} else {
						bool	quote = *value == '"';
						if (quote)
							space = strchr(++value, '"');
						line = space ? space : string_end(line);
						vars.Append().Set(ISO_ptr<string>(str(start, equals), str(value, line)));
						line += int(space && quote);
					}
					continue;
				}
			}
			line = space ? space : string_end(line);
			if (size() < capacity())
				push_back(str(start, line));
		}
	}
	InitFlags();
}

CommandLine::CommandLine(int argc, const char *argv[]) {
	vars			= ISO::root("variables");
	for (int i = 0; i < argc; i++) {
		const char	*line	= argv[i];
//		printf("arg[%i]=%s\n", i, line);
		if (*line == '{') {
			int	open = 1;
			++line;
			while (open && i < argc) {
				const char	*start = line;
				while (open && *line) {
					open += (*line == '{') - (*line == '}');
					++line;
				};
				script += " ";
				if (open == 0)
					--line;
				script += str(start, line);
				line	= argv[++i];
			}
			if (size() == 1)
				push_back(script.begin());
			--i;
		} else if (*line == '+') {
			line++;
			ISO::UserTypeArray	types;
			if (!ISO::ScriptReadDefines(filename(line), types))
				throw_accum("Can't open " << line);
		} else if (*line == '-') {
			SetOption(line);

		} else if (const char *equals = strchr(line, '=')) {
			//*equals++ = 0;
			if (equals[1] == '{') {
				memory_reader	mi(const_memory_block(equals + 2, strlen(equals + 2)));
				vars.Append().Set(ISO::ScriptRead(str(line, equals), 0, mi, ISO::SCRIPT_NOCHECKEND));
				line = equals + 1 + uint32(mi.tell());
				if (*line++ != '}')
					throw("Missing '}'");
			} else {
				vars.Append().Set(ISO_ptr<ISO::ptr_string<char,32>>(str(line, equals), equals + 1));
			}
		} else if (size() < capacity()) {
			push_back(line);
		}
	}
	InitFlags();
}

void CommandLine::Error(const char *error, const char *filename2) {
	const char	*p;
	if (devstudio && (p = strstr(error, " at line "))) {
		int			fn, line;
		char		temp;
		switch (sscanf(p + 9, "%i in %c%n", &line, &temp, &fn)) {
			case 1:
				throw_accum(filename2 << '(' << line << ") : error: " << str(error, p) << '\n');
			case 2:
				throw_accum(p + 8 + fn << '(' << line << ") : error: " << str(error, p) << '\n');
		}
	} else if (str(error).find(istr(filename2))) {
		throw(error);
	}
	throw_accum(filename2 << " : error: " << error);
}

bool CheckDate(const filename &fn1, const filename &fn2, const char *listexternals, bool recurse) {
	filetime_t	t1 = filetime_write(fn1);
	if (t1 == 0)
		return true;

	filetime_t	t2 = filetime_write(fn2);
	if (t1 > t2)
		return true;

	if (!listexternals)
		return false;

	FileInput	ext(listexternals);
	if (!ext.exists())
		return true;

	for (bool build = false;;) {
		filename	dep;
		char		*p = dep, c;
		while ((c = ext.getc()) != EOF && c != '\n')
			*p++ = c;
		*p = 0;

		if (dep.blank())
			return build;

		if (recurse && (dep.ext() == ".ib" || dep.ext() == ".ibz")) {
			filename	ix(filename(dep).set_ext("ix"));
			FileInput	fix(ix);
			if (fix.exists() && CheckDate(dep, ix, filename(dep).set_ext("dep"), recurse)) {
				printf("building %s\n", (char*)ix);
				FileHandler::Write(ISO::ScriptRead(dep.name(), ix, fix, 0), filename(dep));
				build = true;
				continue;
			}
		}

		if (t2 < filetime_write(dep)) {
			if (!recurse)
				return true;
			build = true;
		}
	}
}

bool CommandLine::CheckDate(const filename &fn1, const filename &fn2) {
	if (!checkdate || filetime_write(fn2) < tool_time)
		return true;

	return ::CheckDate(fn1, fn2, listexternals, false);
}

bool CommandLine::MaybeRemoveDupes(ISO_ptr<void> p) {
	if (vars["keepdupes"].GetInt())
		return false;
	printf("Removing dupes\n");
	RemoveDupes(p);
	return true;
}

ISO_ptr<void> CommandLine::ReadSingle(tag id, const filename &fn) {
	try {
		if (const char *ext = vars["withext"].GetString()) {
			if (FileHandler *fh = FileHandler::Get(ext))
				return fh->ReadWithFilename(id, fn);
		} else {
			ISO_ptr<void> p = FileHandler::CachedRead(fn);
			p.SetID(id);
			return p;
		}
	} catch (const char *error) {
		Error(error, fn);
	}
	return ISO_NULL;
}

bool CommandLine::WriteSingle(ISO_ptr<void> &p, const filename &fn) {
	try {
		create_dir(fn.dir());
		if (const char *ext = vars["asext"].GetString()) {
			if (FileHandler *fh = FileHandler::Get(ext))
				if (fh->WriteWithFilename(p, fn))
					return true;
		} else if (FileHandler::Write(p, fn)) {
			return true;
		}
		delete_file(fn);
		return false;

	} catch (const char *error) {
		delete_file(fn);
		Error(error, fn);
	}
	return false;
}

ISO_ptr<void> CommandLine::ReadWild(tag id, const filename &fn, bool raw) {
	if (fn.is_wild()) {
		ISO_ptr<anything> p;

		if (fn.dir().is_wild()) {
			filename	fn2	= filename(fn).rem_dir().rem_dir();
			p = ReadWild(id, filename(fn2).add_dir(fn.name_ext()), raw);

			for (directory_iterator name(filename(fn2).add_dir("*.*")); name; ++name) {
				if (name.is_dir() && name[0] != '.') {
					const char *n = name;
					if (ISO_ptr<void> p2 = ReadWild(filename(n).name(), filename(fn2).add_dir(n).add_dir("*").add_dir(fn.name_ext()), raw)) {
						if (!p)
							p.Create(id);
						p->Append(p2);
					}
				}
			}

		} else {
			for (directory_iterator name(fn); name; ++name) {
				if (!name.is_dir()) {
					const char *n = name;
					if (ISO_ptr<void> p2 = ReadWild(raw ? n : (const char*)filename(n).name(), filename(fn).rem_dir().add_dir(n), raw)) {
						if (!p)
							p.Create(id);
						p->Append(p2);
					}
				}
			}
		}
		return p;
	}

	if (raw) {
		ISO_ptr<ISO_openarray<uint8> >	p(id);
		FileInput	file(fn);
		uint32		size	= file.size32();
		file.readbuff(p->Create(size), size);
		return p;
	}

	return ReadSingle(id, fn);
}

bool CommandLine::WriteWild(ISO_ptr<void> &p, const filename &fn) {
	if (fn.is_wild()) {
		tag	id = p.ID();
		const ISO::Type *type = p.GetType()->SkipUser();
		// was commented out
		if (type->GetType() == ISO::OPENARRAY && ((ISO::TypeOpenArray*)type)->subtype->GetType() == ISO::REFERENCE) {
			ISO_openarray<ISO_ptr<void> > *array = p;
			for (int i = 0, n = array->Count(); i < n; i++) {
				if (!WriteWild((*array)[i], filename(fn.dir()).add_dir(id).add_dir(fn.name_ext())))
					return false;
			}
			return true;

		} else {
			filename	fn2 = filename(fn.dir()).add_dir(id);
			printf("%s\n", (const char*)id);

			if (fn.ext() != "" && fn.ext() != ".*")
				return WriteWild(p, fn2.add_ext(fn.ext()));

			FileOutput	file(fn2);
			if (!file.exists()) {
				printf("Can't create %s, skipping\n", (const char*)fn2);
				return true;
			}
			if (!file.lock()) {
				printf("%s is locked, skipping\n", (const char*)fn2);
				return true;
			}

			if (p.GetType()->GetType() == ISO::REFERENCE)
				p = *(ISO_ptr<void>*)p;

			if (const char *ext = vars["asext"].GetString()) {
				if (FileHandler *fh = FileHandler::Get(ext)) try {
					if (fh->Write(p, file))
						return true;
				} catch (const char *error) {
					file.close();
					delete_file(fn2);
					Error(error, fn);
				}
				file.close();
				delete_file(fn2);
				return false;
			}

			const ISO::Type	*type = p.GetType();
			if (type->SameAs<ISO_openarray<xint32> >() || type->SameAs<ISO_openarray<uint8> >()) {
				ISO_openarray<char>	*array	= (ISO_openarray<char>*)p;
				file.writebuff(*array, ((ISO::TypeOpenArray*)type)->subtype->GetSize() * array->Count());

			} else if (FileHandler *fh = FileHandler::Get(filename(id).ext())) {
				return fh->Write(p, file);

			} else {
				file.writebuff(p, type->GetSize());
			}
			return true;
		}
	}
	return WriteSingle(p, fn);
}

//-----------------------------------------------------------------------------
//	Process
//-----------------------------------------------------------------------------

ConcurrentJobs	convert_jobs;

struct ConvertJob {
	CommandLine	*com;
	filename	fnin, fnout;
	bool		extract;

	void	operator()() {
		try {
			com->ProcessSingle(fnin, fnout, extract);
		} catch (const char *error) {
			ISO_TRACE(error);
			fputs(error, stdout);
		} catch_all() {
			ISO_TRACE("Unknown error");
			fputs("Unknown error", stdout);
		}
		delete this;
	}
	ConvertJob(CommandLine *_com, const filename &_fnin, const filename &_fnout, bool _extract)
		: com(_com), fnin(_fnin), fnout(_fnout), extract(_extract)
	{}
};

void CommandLine::ProcessSingle(const filename &fnin, const filename &fnout, bool extract) {
	Platform::Set(NULL);
	ISO_ptr<void>	p;
	if (script) {
		vars.SetMember("i", ReadSingle("i", fnin));
		p = ISO::ScriptRead(none/*"input"*/, fnin, memory_reader(script.data()),
			(vars["keepexternals"].GetInt()	? ISO::SCRIPT_KEEPEXTERNALS	: 0)
		|	(vars["dontconvert"].GetInt()	? ISO::SCRIPT_DONTCONVERT		: 0)
		|	ISO::SCRIPT_KEEPID
		);
	} else {
		p = ReadSingle(fnin.name(), fnin);
	}

	MaybeRemoveDupes(p);

	Platform::type	t	= Platform::Set(vars["exportfor"].GetString());
	int				be	= vars["bigendian"].GetInt(-1);
	ISO::binary_data.SetBigEndian(be == -1 ? !!(t & Platform::_PT_BE) : be != 0);

	printf("Writing result\n");
	if (extract) {
		WriteWild(p, filename(fnout).add_dir("*"));

	} else if (!WriteSingle(p, fnout)) {
		if (vars["keepgoing"].GetInt()) {
			printf("Cannot open output, or unknown extension");
			return;
		}
		throw_accum("Cannot open output, or unknown extension \"" << fnout << '"');
	}

	if (vars["info"].GetInt())
		PrintInfo(ISO::Browser(p));
}

void CommandLine::ProcessWild(const filename &fnin, const filename &fnout, bool extract) {
	if (fnin.dir().is_wild()) {
		filename	fnin1	= filename(fnin).rem_dir();
		filename	fnin2	= filename(fnin1).rem_dir();
		filename	fnout2	= filename(fnout).rem_dir();

		if (fnin1.name() == "*")
			ProcessWild(filename(fnin2).add_dir(fnin.name_ext()), fnout, extract);

		for (directory_iterator name(fnin1); name; ++name) {
			if (name.is_dir() && name[0] != '.') {
				ProcessWild(
					filename(fnin2).add_dir((const char*)name).add_dir("*").add_dir(fnin.name_ext()),
					filename(fnout2).add_dir((const char*)name).add_dir(fnout.name_ext()),
					extract
				);
			}
		}
	} else if (extract || is_wild(fnout.name())) {
		for (directory_iterator name(fnin); name; ++name) {
			if (!name.is_dir()) {
				filename	src	= filename(fnin).rem_dir().add_dir((const char*)name);
				filename	dst	= filename(fnout).rem_dir().add_dir((const char*)name).set_ext(fnout.ext());

				printf("%s => %s: ", (const char*)src, (const char*)dst);
				if (!extract && !CheckDate(src, dst)) {
					printf("skipping\n");
					continue;
				}

				printf("processing\n");
				if (num_procs > 1) {
					if (!convert_jobs.Initialised())
						convert_jobs.Init(num_procs, THREAD_STACK_DEFAULT);

					convert_jobs.add(new ConvertJob(this, src, dst, extract));

				} else {
					ProcessSingle(src, dst, extract);
				}
			}
		}

	} else {
		filename	dst	= fnin.dir().add_dir(fnout.name_ext());
		if (!CheckDate(fnin, dst)) {
			printf("skipping\n");
		} else if (ISO_ptr<void> p = ReadWild(fnin.name(), fnin, false)) {
			MaybeRemoveDupes(p);
			printf("Writing result\n");
			filename	fn2 = fnin.dir().add_dir(fnout.name_ext());
			if (!FileHandler::Write(p, fn2)) {
				if (vars["keepgoing"].GetInt()) {
					printf("Cannot open output, or unknown extension");
				} else {
					throw_accum("Cannot open output, or unknown extension \"" << fn2 << '"');
				}
			}
		}
	}
}

//-----------------------------------------------------------------------------
//	Main
//-----------------------------------------------------------------------------

extern char _stack_start[];

ISO::FileVariable	vars[] = {
	{"mapping",			"{<path>...}",	"Check other paths"},
	{"exportfor",		"<target>",		"Export for pc, x360, ps3, ps4, wii, ios, mac"},
	{"listexternals",	"<filename>",	"print included files to <filename> (.csv, .xml)"},
	{"checkdate",		"0|1|2",		"only build out of data targets (2 checks executable too)"},
	{"keepexternals",	"0|1",			"do not expand externals on read"},
	{"expandexternals",	"0|1",			"expand externals on write"},
	{"relativepaths",	"0|1",			"use relative paths when writing externals"},
	{"dontconvert",		"0|1",			"do not convert data"},
	{"withext",			"<ext>",		"override file format from input file extension"},
	{"asext",			"<ext>",		"override file format from output file extension"},
	{"raw",				"0|1",			"read file as raw binary"},
	{"keepdupes",		"0|1",			"do not strip dupes"},
	{"bigendian",		"0|1",			"convert to little or big endian"},
	{"stringids",		"0|1",			"keep ids as strings (not crc's)"},
	{"prefix",			"<prefix>",		"use custom id prefix for mergeable data"},
	{"nomip",			"0|1",			"disable mipmap generation"},
	{"nocompress",		"0|1",			"disable (bitmap) compression"},
	{"format",			"<format>",		"use specific bitmap format"},
	{"optimise",		"0|1",			"optimise shaders"},
	{"quality",			"0-100",		"quality level for jpg, eg"},
	{"compression",		"0-100",		"compression level for draco, eg"},
	{"info",			"0|1",			"print information about read files"},
	{"foreach",			"<filespec>",	"wildcard expansion"},
	{"cache",			"<directory>",	"cache converted textures here"},
	{"quiet",			"0|1",			"suppress most log output"},
	{"keepgoing",		"0|1",			"don't stop on first error"},
	{"shareexternals",	"<filename>",	"share externals"},
	{"extract",			"0|1",			"process files inside containers"},
	{"header",			"0|1",			"write a C header file"},
};
// mappings, sizes, layers, isodefs,
// password, find_only, keepenums, verbose, stylesheet,
// metal, usepvrtexlib, tex_scale, uvmode, incdirs, fxargs, debuginfo, samp_rate
// 
// OBJ:		data_size, data_end, nodemangle, section, targetbits, single
// Flash:	flashsemantics, warn_directtext,
// Model optimisation:	weight_threshold, verts_threshold
// ISO9660:				SYSTEM, VOLUME, VOLUMESET, PUBLISHER, PREPARER, APPLICATION, COPYRIGHT,
// WiiDisc:				game_name, company, title, firmware, version, 
// NUMBER_OF_PROCESSORS
// save.animation

filename& RemoveWildDirs(filename &fn) {
	if (fn.dir().is_wild()) {
		filename	dir1 = fn.dir();
		while (dir1.is_wild())
			dir1.rem_dir();
		fn.set_dir(dir1);
	}
	return fn;
}

int main(int argc, char* argv[], char* envp[]) {
	prev_iso_debug_print = _iso_set_debug_print({iso_stdout_print,0});
	ISO::allocate_base::flags.set(ISO::allocate_base::TOOL_DELETE);

	dynamic_array<string>	envp2;
	for (char **p = envp; *p; p++)
		envp2.push_back(*p);
	envp2.push_back((char*)0);

	ISO::root().Add(ISO_ptr<anything>("variables"));
	ISO::root().Add(ISO::GetEnvironment("environment", (char**)(string*)envp2));

	try {
	#ifdef PLAT_MAC
		CommandLine	com(argc, (const char**)argv);
		if (com.printargs) {
			for (int i = 0; i < argc; i++)
				printf("%s%c", argv[i], i == argc - 1 ? '\n' : ' ');
		}
	#else
		CommandLine	com(GetCommandLine());
		if (com.printargs) {
			fputs(GetCommandLine(), stderr);
			fputs("\n", stderr);
		}
	#endif
		if (com.version) {
			printf("IsoCmd Compiled " __DATE__ " at " __TIME__ "\n");
			exit(0);
		}

		if (com.help || com.size() < 2) {
			ISO_OUTPUTF("IsoCmd [var=<value>] [+include] <input> {<output>}\nCompiled " __DATE__ " at " __TIME__ "\n");
			ISO_OUTPUTF("\n"
				"<input> can be one of:\n"
				"filename (with wildcards)\n"
				"URL\n"
				"{script}\n"
			);
			ISO_OUTPUTF("\n"
				"<output> (optional) can be one of:\n"
				"filename (with wildcards)\n"
				"<IP address>:port\n"
				"target:port\n"
				"{script}\n"
			);
			ISO_OUTPUTF("\n"
				"<value> can be one of:\n"
				"text (without spaces)\n"
				"\"text\" (may contain spaces)\n"
				"{script}\n"
			);
			ISO_OUTPUTF("\n"
				"Some common variables:\n");
			size_t	max_len = 0;
			for (auto &i : ISO::FileVariable::all())
				max_len = max(max_len, strlen(i.name) + strlen(i.syntax));
			for (auto &i : ISO::FileVariable::all())
				ISO_OUTPUTF() << "" << leftjustify(uint16(max_len + 4)) << i.name << '=' << i.syntax << none << i.description << '\n';

			ISO_OUTPUTF("\n"
				"Supported file formats:\n");
			max_len = 0;
			for (auto &i : ISO::FileHandler::all())
				max_len = max(max_len, string_len(i.GetExt()));

			temp_array<holder<ISO::FileHandler&>>	indices = filter(ISO::FileHandler::all(), [](ISO::FileHandler &i) { return i.GetExt(); });
			sort(indices, [](ISO::FileHandler &a, ISO::FileHandler &b) { return str(a.GetExt()) < b.GetExt(); });
			for (ISO::FileHandler &i : indices) {
				if (auto ext = i.GetExt())
					ISO_OUTPUTF() << "" << leftjustify(uint16(max_len + 1)) << ext << ": " << none << i.GetDescription() << '\n';
			}

			exit(com.help ? 0 : -1);
		}

		Cache	cache;
		auto	save	= FileHandler::PushCache(&cache);

		Platform::Set(NULL);

		const char	*shareexternals	= com.vars["shareexternals"].GetString();
		const char	*exportfor		= com.vars["exportfor"].GetString();

		if (shareexternals && !com.keepexternals) {
			ISO_ptr<anything> externals = ISO::root("externals");
			if (!externals)
				ISO::root().Add(externals.Create("externals"));
			externals->Append(FileHandler::Read(filename(shareexternals).name(), filename(shareexternals)));
			com.vars.SetMember("keepexternals", 1);
			com.vars.SetMember("dontconvert", 1);
		}

		if (!com.script && com.size() >= 3) {
			filename	fn1(com[1]);
			filename	fn2(com[2]);

			// check dates

			if (!fn1.is_wild() && !com.CheckDate(fn1, fn1.matched(fn1, fn2))) {
				printf("Destination is newer than all dependencies - skipping\n");
				return 0;
			}

			bool		sameext	= fn1.ext() == fn2.ext() || fn2.ext() == ".*";

			// simple copies
			if (com.copy || (!exportfor && sameext && fn1.ext() != istr(".ix"))) {
				RemoveWildDirs(fn1);
				int		ret = 0;
				for (recursive_directory_iterator i(fn1); i; ++i) {
					filename	src = i;
					filename	dst;
					src.matched(dst, fn1, fn2);

					com.printf("copy %s to %s: ", (const char*)src, (const char*)dst);

					if (!com.CheckDate(src, dst)) {
						com.printf("skipping\n");
						continue;
					}

					FileInput	in(src);
					if (!in.exists()) {
						com.printf("Cannot open input\n");
						ret	= 1;
						continue;
					}

					if (dst.is_url()) {
						com.printf("transfering\n");
						uint32	len		= in.size32();
						ISO_ptr<ISO_openarray<uint8> > data(0);
						in.readbuff(data->Create(len, false), len);

						ISO_ptr<anything>	cmd("SendFile", 2);
						(*cmd)[0] = ISO_ptr<string>(0, dst.name_ext());
						(*cmd)[1] = data;

						ISO_ptr<anything>	p(0);
						p->Append(cmd);

						dynamic_memory_writer output;
						FileHandler::Get(".ib")->Write(p, output);
						// init
						if (isolink_init()) {
							const char *target = dst;
							char *colon = strchr(dst, ':');
							*colon++ = 0;

							// send
							uint32be output_len = output.size32();
							isolink_handle_t handle = isolink_send(target, from_string(colon), &output_len, sizeof(output_len));
							if (handle != isolink_invalid_handle && isolink_send(handle, output.data(), output.length()))
								isolink_close(handle);
							else
								throw_accum("Cannot upload, " << isolink_get_error());
							// term
							isolink_term();
						}
					} else {
						com.printf("copying\n");
						stream_copy(FileOutput(dst).me(), in);
					}
				}
				return ret;
			}

			// just compress ib to ibz
			if (!exportfor && fn1.ext() == istr(".ib") && fn2.ext() == istr(".ibz")) {
				RemoveWildDirs(fn1);
				int		ret = 0;
				for (recursive_directory_iterator i(fn1); i; ++i) {
					filename	src = i;
					filename	dst;
					src.matched(dst, fn1, fn2);

					com.printf("compress %s to %s: ", (const char*)src, (const char*)dst);

					if (!com.CheckDate(src, dst)) {
						com.printf("skipping\n");
						continue;
					}

					FileInput	in(src);
					if (!in.exists()) {
						com.printf("Cannot open input\n");
						ret	= 1;
						continue;
					}
					FileOutput	out(dst);
					com.printf("compressing\n");
					out.write(uint32le(in.size32()));
					stream_copy(deflate_writer(out), in);
				}
				return ret;
			}

			// just decompress ibz to ib
			if (!exportfor && fn1.ext() == istr(".ibz") && fn2.ext() == istr(".ib")) {
				RemoveWildDirs(fn1);
				int		ret = 0;
				for (recursive_directory_iterator i(fn1); i; ++i) {
					filename	src = i;
					filename	dst;
					src.matched(dst, fn1, fn2);

					com.printf("decompress %s to %s: ", (const char*)src, (const char*)dst);

					if (!com.CheckDate(src, dst)) {
						com.printf("skipping\n");
						continue;
					}

					FileInput	in(src);
					if (!in.exists()) {
						com.printf("Cannot open input\n");
						ret	= 1;
						continue;
					}
					uint32		len		= in.get<uint32le>();
					FileOutput	out(dst);
					com.printf("decompressing\n");
					stream_copy(FileOutput(dst).me(), deflate_reader(in), len);
				}
				return ret;
			}
		}

		// process input

		bool	extract = !!com.vars["extract"].GetInt();
		if (!com.script && com.size() >= 3 && filename(com[1]).is_wild() && filename(com[2]).is_wild()) {
			// *.x *.y
			// *.x y.y
			// */x.x */*.y
			// */x.x */y.y
			// */*.x */y.y
			com.printf("Multiple file conversion\n");

			filename	fn1(com[1]), fn2(com[2]);
			if (fn2.dir().blank()) {
				filename	dir(fn1.dir());
				while (dir.is_wild())
					dir.rem_dir();
				fn2.set_dir(dir);
			}
			com.ProcessWild(fn1, fn2, extract);

		} else if (!com.script && com.size() >= 3 && extract) {
			com.ProcessWild(com[1], com[2], extract);

		} else if (com.script && com.size() >= 3 && is_wild(filename(com[2]).name()) && com.vars["foreach"]) {
			com.printf("Multiple file output from script\n");
			com.ProcessWild(com.vars["foreach"].GetString(), com[2], extract);

		} else {
			ISO_ptr<void>	p;

			if (com.script) {
				com.printf("Parsing script\n");
				if (!(p = ISO::ScriptRead(/*"input"*/none, 0, memory_reader(const_memory_block(com.script.begin(), com.script.length())).me(),
					(com.keepexternals	? ISO::SCRIPT_KEEPEXTERNALS	: 0)
				|	(com.dontconvert	? ISO::SCRIPT_DONTCONVERT		: 0)
				|	ISO::SCRIPT_KEEPID
				)))
					throw_accum("Cannot parse script");
			} else {
				filename		fn(com[1]);
#ifndef PLAT_MAC
				if (fn.is_url()) {
					com.printf("Reading from URL\n");
					const char *ext = com.vars["withext"].GetString();
					if (FileHandler *fh = FileHandler::Get(ext ? ext : (const char*)fn.ext())) {
						p = fh->Read(fn.name(), HTTPopenURL(HTTP::Context("isocmd"), fn).me());
					}
				} else
#endif
				com.printf("Reading file\n");
				if (!(p = com.ReadWild(fn.is_wild() ? NULL : (const char*)fn.name(), fn, !!com.vars["raw"].GetInt())))
					throw_accum("Cannot open input, or unknown extension \"" << fn << '"');
			}

#ifndef PLAT_MAC
			if (shareexternals && !com.keepexternals) {
				ShareExternals(p, com[1], shareexternals, com.dontconvert);
				com.vars["keepexternals"].Set(0);
				com.vars["dontconvert"].Set(com.dontconvert);
			}
#endif
			com.MaybeRemoveDupes(p);

			if (com.size() >= 3) {
				Platform::type	t	= Platform::Set(exportfor);
				ISO::binary_data.SetBigEndian(!!com.vars["bigendian"].GetInt(t & Platform::_PT_BE));

				ISO::root("variables").SetMember("output", ISO_ptr<string>(0, com[2]));
				filename	fn2(com[2]);
				if (!com.script) {
					filename	dir1 = filename(com[1]).dir();
					filename	dir2 = fn2.dir();
					if (dir2.blank()) {
						while (dir1.is_wild())
							dir1.rem_dir();
						fn2.set_dir(dir1);
					} else if (dir2.is_wild() && !dir1.is_wild()) {
						while (dir2.is_wild())
							dir2.rem_dir();
						fn2.set_dir(dir2);
					}
				}
				if (fn2.is_url()) {
					com.printf("Transfering result to target\n");
					// build
					dynamic_memory_writer output;
					FileHandler::Get(".ib")->Write(p, output);
					// init
					if (isolink_init()) {
						// address
						const char *target = fn2;
						char *colon = strchr(fn2, ':');
						*colon++ = 0;
						if (strcmp(target, "target") == 0)
							target = exportfor;
						// send
						uint32be output_len = output.size32();
						isolink_handle_t handle = isolink_send(target, from_string(colon), &output_len, sizeof(output_len));
						if (handle != isolink_invalid_handle && isolink_send(handle, output.data(), output.length()))
							isolink_close(handle);
						else
							throw_accum("Cannot upload, " << isolink_get_error());
						// term
						isolink_term();
					} else {
						throw_accum("Failed to initialise isolink");
					}
				} else {
					com.printf("Writing result\n");
					if (!com.WriteWild(p, fn2))
						throw_accum("Cannot open output, or unknown extension \"" << fn2 << '"');
				}

			} else if (!com.vars["info"].GetInt()) {
				com.printf("Dumping result to screen\n");
				ISO::ScriptWriter(FileOutput(fdopen(dup(fileno(stdout)), "w")).me()).DumpData(ISO::Browser(p));
			}

			if (com.vars["info"].GetInt())
				PrintInfo(ISO::Browser(p));

			if (const char *header = com.vars["header"].GetString()) {
				com.printf("Writing header file\n");
				WriteCHeader(p, FileOutput(header).me());
			}

			if (const char *listexternals = com.vars["listexternals"].GetString()) {
				com.printf("Listing externals/dependencies\n");
				ListExternals(cache, com[2], listexternals);
			}
		}
		com.printf("\nDone.\n");

	// errors
	} catch (const char *error) {
		ISO_OUTPUT(error);
		fputs(error, stderr);
		return -1;
	} catch_all() {
		ISO_TRACE("Unknown error\n");
		fputs("Unknown error\n", stderr);
		return -1;
	}
	return 0;
}
