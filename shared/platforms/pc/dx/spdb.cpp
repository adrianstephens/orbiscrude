#include "spdb.h"
#include "stream.h"
#include "filename.h"

using namespace iso;

struct LineLoc {
	uint32	file;
	uint32	baseLineNum;
	LineLoc() : file(0), baseLineNum(0) {}
	LineLoc(uint32 file, uint32 line) : file(file), baseLineNum(line) {}
};

struct LineMapper {
	struct File {
		uint32					file;
		sparse_array<LineLoc,uint32,uint32>	lines;
	};
	hash_map<string, File>	file_by_name;
	hash_map<uint32, File*>	file_by_id;
	void	add_file(uint32 file, const char *name) {
		file_by_name.put(name).file = file;
	}
	File*	get_file(const string &name) {
		auto	file = file_by_name[name];
		return file.exists() ? &file.get() : nullptr;
	}
	File*	get_file(uint32 id) {
		return file_by_id[id].or_default();
	}
	bool	start() {
		if (file_by_name.empty())
			return false;
		for (auto &i : file_by_name)
			file_by_id[i.file] = &i;
		return true;
	}
};

string iso::GetPDBFirstFilename(istream_ref file) {
	MSF::reader	msf(file, 0);
//	msf.addref(1u<<31);

	PDBinfo	info;
	info.load(&msf, snPDB);

	DBI		dbi;
	dbi.load(info, &msf, snDbi);
	return (const char*)dbi.filename_buf;
}

ParsedSPDB::ParsedSPDB(istream_ref file, const char *path) : flags(0) {
	PDBinfo	info;
	ref_ptr<MSF::reader>	msf = new MSF::reader(file, 0);
	info.load(msf, snPDB);
	PDB::load(info, msf);

	struct FunctionLines : LineLoc {
		dynamic_array<CV::Location> locations;
	};

	dynamic_array<FunctionLines>	function_lines;
	uint32							code_offset	= 0;

	for (auto &mod : Modules()) {

		for (auto &sym : mod.Symbols()) {
			switch (sym.rectyp) {
				case CV::S_GPROC32: {
					auto proc = sym.as<CV::PROCSYM32>();
					const char *name = proc->name;
					code_offset	= proc->addr.off;
					break;
				}
				case CV::S_COMPILE3: {
					auto details = sym.as<CV::COMPILESYM3>();
					compiler = details->verSz;
					break;
				}
				case CV::S_ENVBLOCK: {
					auto env = sym.as<CV::ENVBLOCKSYM>();
					for (auto &&i : make_split_range<2>(env->rgsz)) {
						if (!strcmp(i[0], "hlslEntry"))
							entry	= i[1];
						else if (!strcmp(i[0], "hlslTarget"))
							profile = i[1];
						else if (!strcmp(i[0], "hlslFlags"))
							from_string(i[1], flags);
						else if (!strcmp(i[0], "hlslDefines"))
							defines	= i[1];
					}
					break;
				}
				case CV::S_INLINESITE: {
					FunctionLines		lines;
					for (auto &i : sym.as<CV::INLINESITESYM>()->lines(code_offset))
						lines.locations.push_back(i);
					function_lines.push_back(lines);
				}
			}
		}

		const CV::SubSectionT<CV::DEBUG_S_CROSSSCOPEEXPORTS>	*xscope_exports = 0;
		const CV::SubSectionT<CV::DEBUG_S_CROSSSCOPEIMPORTS>	*xscope_imports = 0;

		LineMapper	line_mapper;

		for (auto &sliceeam : mod.SubStreams()) {
			switch (sliceeam.type) {
				case CV::DEBUG_S_FILECHKSMS: { // hash
					auto	file_checksums = sliceeam.as<CV::DEBUG_S_FILECHKSMS>();
					for (auto &i : file_checksums->entries()) {
						uint32		file	= uint32((const uint8*)&i - sliceeam.data()) + 1;
						const char *name	= FileName(i.name);

						if (auto sn = info.Stream(cstr("/src/files/") + to_lower(name))) {
							files.add(file, name, malloc_block::unterminated(MSF::stream_reader(msf, sn)));

						} else if (exists(name)) {
							files.add(file, name, malloc_block(FileInput(name), filelength(name)));

						} else if (path && exists(filename(path).add_dir(name))) {
							filename	name2 = filename(path).add_dir(name);
							files.add(file, name, malloc_block(FileInput(name2), filelength(name2)));
						} else {
							line_mapper.add_file(file, name);
						}
					}

					if (line_mapper.start()) {
						// remap lines back to #line
						for (auto& i : files) {
							LineMapper::File	*lines2 = 0;

							for (const char *p = i.source; p = string_find(p, "#line "); ++p) {
								string_scan	ss(p);
								uint32	line	= 0;

								ss.check("#line ");
								ss >> line;
								if (ss.skip_whitespace().check('"')) {
									auto	fn	= ss.get_token(~char_set('"'));
									ss.scan_skip('\n');
									lines2	= line_mapper.get_file(fn);
								}
								if (lines2) {
									int	iline = i.get_line_num(p);
									while (ss.remaining()) {
										auto data = ss.get_raw(~char_set('\n'));
										if (data.begins("#line "))
											break;
										lines2->lines[line++] = LineLoc(i.id, iline++);
										ss.move(1);
									}
								}
							}
						}

					}
					break;
				}

				case CV::DEBUG_S_CROSSSCOPEEXPORTS:
					xscope_exports = sliceeam.as<CV::DEBUG_S_CROSSSCOPEEXPORTS>();
					break;

				case CV::DEBUG_S_CROSSSCOPEIMPORTS:
					xscope_imports = sliceeam.as<CV::DEBUG_S_CROSSSCOPEIMPORTS>();
					break;

				case CV::DEBUG_S_INLINEELINES: {
					auto	sub = sliceeam.as<CV::DEBUG_S_INLINEELINES>();
					auto	f	= function_lines.begin();
					if (sub->extended()) {
						for (auto &i : sub->entries_ex()) {
							if (i.inlinee & 0x1000) {
								f->file			= i.fileId + 1;
								f->baseLineNum	= i.sourceLineNum;

								if (auto x = line_mapper.get_file(f->file)) {
									auto	&&m = x->lines[f->baseLineNum];
									if (m.exists())
										*(LineLoc*)f = x->lines[f->baseLineNum].get();
								}

								for (auto &j : f->locations) {
									auto *patch		= lower_boundc(locations, j.offset);
									auto *end_patch = upper_boundc(locations, j.end() - 1);
									locations.erase(patch, end_patch);
									locations.insert(patch, Disassembler::Location(j.offset, f->file, f->baseLineNum + j.line, j.col_start, j.col_end));
								}
							}
							++f;
						}
					} else {
						for (auto &i : sub->entries()) {
							if (i.inlinee & 0x1000) {
								f->file			= i.fileId + 1;
								f->baseLineNum	= i.sourceLineNum;

								if (auto x = line_mapper.get_file(f->file))
									*(LineLoc*)f = x->lines[f->baseLineNum].get();

								for (auto &j : f->locations) {
									if (auto *patch = lower_boundc(locations, j.offset)) {
										if (patch->offset == j.offset) {
											patch->file = f->file;
											patch->line = f->baseLineNum + j.line;
										}
									}
								}
							}
							++f;
						}
					}
					break;
				}

				case CV::DEBUG_S_LINES: {
					auto	lines = sliceeam.as<CV::DEBUG_S_LINES>();
					if (lines->extended()) {
						for (auto &i : lines->entries_ex()) {
							int		file = i.offFile + 1;
							if (auto x = line_mapper.get_file(file)) {
								for (auto &&j : i.entries()) {
									LineLoc	loc = x->lines[j.get<0>().linenumStart];
									locations.emplace_back(j.get<0>().offset, loc.file, loc.baseLineNum, j.get<1>().offColumnStart, j.get<1>().offColumnEnd);
								}
							} else {
								for (auto &&j : i.entries())
									locations.emplace_back(j.get<0>().offset, file, j.get<0>().linenumStart, j.get<1>().offColumnStart, j.get<1>().offColumnEnd);
							}
						}
					} else {
						for (auto &i : lines->entries()) {
							int		file = i.offFile + 1;
							if (auto x = line_mapper.get_file(file)) {
								for (auto &j : i.lines()) {
									LineLoc	loc = x->lines[j.linenumStart];
									locations.emplace_back(j.offset, loc.file, loc.baseLineNum);
								}
							} else {
								for (auto &j : i.lines())
									locations.emplace_back(j.offset, file, j.linenumStart);
							}
						}
					}
					break;
				}
			}
		}
	}

	for (auto &i : locations)
		i.offset -= 8;
}

uint16 GetSlot(CV::DATASYMHLSL *hlsl) {
	switch (hlsl->regType) {
		case CV_HLSLREG_SAMPLER:				return hlsl->sampslot;
		case CV_HLSLREG_RESOURCE:				return hlsl->texslot;
		case CV_HLSLREG_CONSTANT_BUFFER:		return hlsl->dataslot;
		case CV_HLSLREG_UNORDERED_ACCESS_VIEW:	return hlsl->uavslot;
		default:								return (uint16)-1;
	}
};

CV::DATASYMHLSL	*ParsedSPDB::FindUniform(uint16 type, uint16 slot) {
	for (auto &sym : Symbols()) {
		if (sym.rectyp == CV::S_GDATA_HLSL) {
			auto	*hlsl = sym.as<CV::DATASYMHLSL>();
			if (hlsl->regType == type && GetSlot(hlsl) == slot)
				return hlsl;
		}
	}
	return nullptr;
}

Uniforms::Uniforms(const PDB &pdb) {
	for (auto &sym : pdb.Symbols()) {
		if (sym.rectyp == CV::S_GDATA_HLSL) {
			auto	*hlsl = sym.as<CV::DATASYMHLSL>();
			switch (hlsl->regType) {
				case CV_HLSLREG_SAMPLER:
					smp.push_back(hlsl);
					break;
				case CV_HLSLREG_RESOURCE:
					srv.push_back(hlsl);
					break;
				case CV_HLSLREG_CONSTANT_BUFFER:
					cbs.set(hlsl->dataslot).push_back(hlsl);
					break;
				case CV_HLSLREG_UNORDERED_ACCESS_VIEW:
					uav.push_back(hlsl);
					break;
			}
		}
	}
	sort(smp, [](CV::DATASYMHLSL *a, CV::DATASYMHLSL *b) { return a->sampslot < b->sampslot; });
	sort(srv, [](CV::DATASYMHLSL *a, CV::DATASYMHLSL *b) { return a->texslot < b->texslot; });
	sort(uav, [](CV::DATASYMHLSL *a, CV::DATASYMHLSL *b) { return a->uavslot < b->uavslot; });
	for (auto &cb : cbs)
		sort(cb, [](CV::DATASYMHLSL *a, CV::DATASYMHLSL *b) { return a->dataoff < b->dataoff; });
}

ParsedSDBGC::ParsedSDBGC(const SDBG *sdbg) {
	if (!sdbg) {
		flags = 0;
		return;
	}

	flags			= sdbg->shaderFlags;

	char *ascii		= sdbg->ascii.get(sdbg);
	compiler		= ascii + sdbg->compilerSigOffset;
	profile			= ascii + sdbg->profileOffset;
	entry			= ascii + sdbg->entryFuncOffset;

//	dynamic_array<SDBG::FileHeader>	file_headers = sdbg->files.get(sdbg);
	int	id = 1;
	for (auto &i : sdbg->files.get(sdbg))
		files.add(
			id++,
			string(i.filename.get(ascii), i.filenameLen),
			memory_block(i.source.get(ascii), i.sourceLen)
		);

	instructions	= sdbg->instructions.get(sdbg);
	variables		= sdbg->variables.get(sdbg);
	inputs			= sdbg->inputs.get(sdbg);
	symbols			= sdbg->symbols.get(sdbg);
	scopes			= sdbg->scopes.get(sdbg);
	types			= sdbg->types.get(sdbg);
	int32s			= make_range(sdbg->int32s.get(sdbg), (int32*)sdbg->ascii.get(sdbg));
}

void ParsedSDBGC::GetFileLineFromIndex(uint32 instruction, int32 &file, int32 &line) const {
	if (instruction < instructions.size()) {
		int32 symID = instructions[instruction].symbol;
		if (symID > 0 && symID < (int32)symbols.size()) {
			const SDBG::Symbol &sym = symbols[symID];
			file	= sym.fileID;
			line	= sym.lineNum - 1;
		}
	}
}

void ParsedSDBGC::GetFileLineFromOffset(uint32 offset, int32 &file, int32 &line) const {

}

