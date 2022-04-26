#include "main.h"
//#include "viewer.h"
#include "com.h"
#include "iso/iso_binary.h"
#include "graphics.h"
#include "common/shader.h"
#include <d3dcompiler.h>

#ifndef __clang__
#include <d3dx9shader.h>
#endif

#include "dx/spdb.h"
#include "dx/dx11_effect.h"
#include "windows\treecolumns.h"
#include "windows\splitter.h"

using namespace app;

Control MakeTextViewer(const WindowPos &wpos, const char *title, const char *text, size_t len);
Control MakeHTMLViewer(const WindowPos &wpos, const char *title, const char *text, size_t len);
Control EditWiiShader(const WindowPos &wpos, void *p);

#ifndef ISO_PTR64
bool DisassembleX360Shader(const void *p, ostream_ref file, bool ps);
#endif

bool DisassemblePS3Shader(const void *p, ostream_ref file, bool ps);
bool DisassemblePS3Shader(const void *p, size_t size, ostream_ref file, bool ps);

#ifdef USE_DX11
bool DisassembleDX11Shader(const void *p, size_t size, ostream_ref file) {
	com_ptr<ID3DBlob>	blob;
	if (FAILED(D3DDisassemble(p, size, D3D_DISASM_ENABLE_COLOR_CODE, 0, &blob)))
		return false;
	file.writebuff(blob->GetBufferPointer(), blob->GetBufferSize());
	return true;
}
#endif

#ifndef __clang__
bool DisassemblePCShader(const void *p, ostream_ref file, bool ps) {
	com_ptr<ID3DXBuffer>	dis;
	if (FAILED(D3DXDisassembleShader((DWORD*)p, 1, NULL, &dis)))
		return false;
	file.writebuff(dis->GetBufferPointer(), dis->GetBufferSize() - 1);
	return true;
}
#endif

class ViewTree : public Window<ViewTree> {
protected:
	SplitterWindow		splitter;
	TreeColumnControl	treecolumn;
	TreeColumnDisplay	treecolumn_display;
	Control				editwindow;
	TabControl2			tabcontrol;

	ISO_ptr<void>		p;
	uint32				max_expand;
public:
	LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam);

//	void			SetEditWindow(Control c);
	ViewTree(MainWindow &_main, const WindowPos &wpos, const ISO_ptr<void> &_p) : p(_p), max_expand(1000) {
		Create(wpos, NULL, CHILD | CLIPCHILDREN | VISIBLE, CLIENTEDGE);
		ISOTree(treecolumn.GetTreeControl()).Setup(p, TVI_ROOT, max_expand);
	}
};

LRESULT ViewTree::Proc(UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE: {
			splitter.Create(GetChildWindowPos(), 0, CHILD | VISIBLE | CLIPSIBLINGS, NOEX);
			treecolumn.Create(splitter._GetPanePos(0), NULL, CHILD | VISIBLE | HSCROLL | treecolumn.GRIDLINES | treecolumn.HEADERAUTOSIZE, ACCEPTFILES);
			splitter.SetPane(0, treecolumn);

			HeaderControl	header	= treecolumn.GetHeaderControl();
			header.SetValue(GWL_STYLE, CHILD | VISIBLE | HDS_FULLDRAG);
			HeaderControl::Item("Symbol").	Format(HDF_LEFT).Width(100).Insert(header, 0);
			HeaderControl::Item("Type").	Format(HDF_LEFT).Width(100).Insert(header, 1);
			HeaderControl::Item("Value").	Format(HDF_LEFT).Width(100).Insert(header, 2);
			treecolumn.SetMinWidth(2, 100);
			treecolumn.GetTreeControl().style = CHILD | VISIBLE | CLIPSIBLINGS | TreeControl::NOHSCROLL | TreeControl::HASLINES | TreeControl::HASBUTTONS | TreeControl::LINESATROOT | TreeControl::SHOWSELALWAYS | TreeControl::FULLROWSELECT;

			break;
		}

		case WM_SIZE:
			splitter.Resize(Point(lParam));
			return 0;

		case WM_NOTIFY: {
			NMHDR			*nmh	= (NMHDR*)lParam;
			ISOTree			tree	= treecolumn.GetTreeControl();
			HeaderControl	header	= treecolumn.GetHeaderControl();
			switch (nmh->code) {
				case TCN_GETDISPINFO: {
					NMTCCDISPINFO *nmdi = (NMTCCDISPINFO*)nmh;
					treecolumn_display.Display(p, tree, nmdi);
					return nmdi->iSubItem;
				}
				case TVN_ITEMEXPANDING:
					treecolumn_display.Expanding(*this, p, tree, (NMTREEVIEW*)nmh, 1024);
					return 0;

				case NM_CUSTOMDRAW: {
					NMCUSTOMDRAW *nmcd = (NMCUSTOMDRAW*)nmh;
					if (nmcd->dwDrawStage == CDDS_POSTPAINT && nmh->hwndFrom == tree)
						treecolumn_display.PostDisplay(treecolumn);
					break;
				}
			}
			break;
		}

		case WM_NCDESTROY:
			delete this;
			break;
		default:
			return Super(message, wParam, lParam);
	}
	return 0;
}


ISO_ptr<void> MakeISO_PDB(PDB &&pdb);

class EditorShader : public Editor {
	enum {
		VS = 0, PS = 1,
		PC = 0, PS3 = 2, PS3_raw = 4, X360 = 6, WII = 8, DX11 = 10,
	};
	int	Check(void *p, uint32 len) {
		if (len > 4) {
			bool			ps;
			uint32le		*le	= (uint32le*)p;
			uint32be		*be	= (uint32be*)p;

			if ((ps = le[0] == 0xffff0300) || le[0] == 0xfffe0300)
				return PC + int(ps);

			if (be[0] == 'DXBC')
				return DX11;

//			char			*pc	= (char*)p;
//			if ((ps = pc[0] == 'P') || pc[0] == 'V')
//				return PS3 + int(ps);

#ifndef ISO_PTR64
			if (len > 10 * 4 && be[10] == 0x102a1100)
				return X360 + PS;

			if (len > 218 * 4 && (be[218] & 0xffffff01) == 0x102a1101)
				return X360 + VS;
#endif
			//return PS3_raw + PS;
		}
		return -1;//EditWiiShader(main, rect, sh);
	}

	virtual bool Matches(const ISO::Browser &b) {
		return b.Is("PS3PixelShader") || b.Is("PS3VertexShader")
			|| b.Is("DX9ShaderStage")
#ifdef USE_DX11
			|| b.Is("DX11ShaderStage")
//			|| b.Is("DX11Shader")
#endif
			|| b.Is("physical_ptr")
//			|| (b.GetType() == ISO::ARRAY && Check(b, b.GetTypeDef()->GetSize()) >= 0)
//			|| (b.GetType() == ISO::OPENARRAY && Check(b[0], b.Count()) >= 0)
//			|| (b.GetType() == ISO::REFERENCE && Matches(*b))
		;
	}
	virtual bool Matches(const ISO::Type *type) {
		return type->Is("physical_ptr");
	}
	virtual Control		Create(MainWindow &main, const WindowPos &wpos, const ISO_VirtualTarget &b) {
		const char		*title = "Shader";

#ifdef USE_DX11
		if (b.Is("DX11ShaderStage")) {
			const dx::DXBC	*dxbc	= *b;

			MSF::EC		error;
			ref_ptr<MSF::reader>	msf = new MSF::reader(lvalue(memory_reader(dxbc->GetBlob(dx::DXBC::ShaderPDB))), &error);
			if (!error) {
				PDBinfo		info;
				if (info.load(msf, snPDB)) {
					PDB	pdb;
					if (pdb.load(info, msf))
						return *new ViewTree(main, wpos, MakeISO_PDB(move(pdb)));
				}
			}

		} else if (b.Is("DX11Shader")) {
			DX11Shader		*shader = b;

#if 0
			ISO_ptr<anything>	p(0);
			for (int i = 0; i < num_elements(shader->sub); i++) {
				if (sized_data sub = shader->sub[i]) {
					const dx::DXBC	*dxbc = sub;
					//ParsedSPDB		spdb(dxbc->GetBlob(dx::DXBC::ShaderPDB));

					MSF_EC		error;
					ref_ptr<reader>	msf = new reader(memory_reader(dxbc->GetBlob(dx::DXBC::ShaderPDB)), &error);
					if (!error) {
						PDBinfo		info;
						if (info.load(msf, snPDB)) {
							PDB	pdb(msf);
							if (pdb.load(info))
								p->Append(MakeISO_PDB(move(pdb)));
						}
					}

				}
			}
			return *new ViewTree(main, wpos, p);
#else
			static const char *shader_types[] = {
				"PIXEL",
				"VERTEX",
				"GEOMETRY",
				"HULL",
				"DOMAIN",
				"COMPUTE",
			};
			malloc_block	text;

			for (int i = 0; i < num_elements(shader->sub); i++) {
				if (sized_data sub = shader->sub[i]) {
					com_ptr<ID3DBlob>	blob;
					if (SUCCEEDED(D3DDisassemble(sub, sub.length(),
						D3D_DISASM_ENABLE_COLOR_CODE,
						buffer_accum<256>()
							<< "\n<font color = \"#00f000\">//" << repeat('-', 40)
							<< "\n//\t" << shader_types[i] << " SHADER"
							<< "\n//" << repeat('-', 40) << "\n</font>\n",
						&blob
					))) {
						text += memory_block(blob->GetBufferPointer(), blob->GetBufferSize());
					}
				}
			}

			return MakeHTMLViewer(wpos, title, text, text.size32());
#endif
		}
#endif

		ISO::Browser2	b2		= b.GetData();
		void			*p		= b2;

		if (b.Is("physical_ptr")) {
			void	*e		= ISO::iso_bin_allocator().alloc(0, 1);
			uint32	total	= vram_offset(e);
			char	*buffer = (char*)e - total + *(uint32*)p + b.bin;
			return MakeTextViewer(wpos, title, buffer, strlen(buffer));
		}

		uint32	len		= b2.GetSize();
		int		type;
		if (b2.Is("PS3PixelShader")) {
			type	= PS3_raw | PS;
		} else if (b2.Is("PS3VertexShader")) {
			type	= PS3_raw | VS;
		} else {
			if (b2.SkipUser().GetType() == ISO::OPENARRAY) {
				p	= b2[0];
				len	= b2.Count() * b2[0].GetSize();
			}
			type	= Check(p, len);
		}

		dynamic_memory_writer	m;
		bool			ps		= (type & 1) == PS;
		bool			html	= false;
		switch (type & ~1) {
#ifdef USE_DX11
			case DX11:		html = DisassembleDX11Shader(p, len, m);break;
#endif
#ifndef __clang__
			case PC:		html = DisassemblePCShader(p, m, ps);	break;
#endif
			case PS3:		DisassemblePS3Shader(p, m, ps);			break;
			default:
			case PS3_raw:	DisassemblePS3Shader(p, len, m, ps);	break;
#ifndef ISO_PTR64
			case X360:		html = DisassembleX360Shader(p, m, ps);	break;
#endif
			case WII:		return EditWiiShader(wpos, p);
		}
		return html ?	MakeHTMLViewer(wpos, title, m.data(), m.size32())
				:		MakeTextViewer(wpos, title, m.data(), m.size32());
	}
public:
	EditorShader() {
		static ISO::TypeUserSave	def_physical_ptr("physical_ptr", ISO::getdef<uint32>());
	}

} editorshader;


typedef ISO_ptr<ISO_openarray<xint32> >	Shader[SS_COUNT];

class Fx11FileHandler : public FileHandler {

	int	Check(istream_ref file) {
		file.seek(0);
		switch (file.get<uint32le>()) {
			case dx11_effect::Header::fx_4_0:
			case dx11_effect::Header::fx_4_1:
			case dx11_effect::Header::fx_5_0:
				return CHECK_PROBABLE;
			default:
				return  CHECK_DEFINITE_NO;
		}
	}

	virtual ISO_ptr<void>	Read(tag id, istream_ref file)	{
		using namespace dx11_effect;
		malloc_block	data = malloc_block::unterminated(file);

		Parser			*header	= (Parser*)data;
		const void		*base	= header->Base();
		Groups			ig		= header->GetGroups();
		ISO_ptr<fx>			pfx(id);
		ISO_ptr<technique>	*pt = pfx->Create(header->num_techniques);

		for (Groups::iterator i = ig.begin(), e = ig.end();  i != e; ++i) {
			Techniques t = i.GetTechniques();
			for (Techniques::iterator i = t.begin(), e = t.end();  i != e; ++i) {
				technique	*t	= pt++->Create(i.GetName());
				Passes		p	= i.GetPasses();
				for (Passes::iterator i = p.begin(), e = p.end();  i != e; ++i) {
					ISO_ptr<Shader>	pass(i.GetName());
					t->Append(pass);
					Assignments a = i.GetAssignments();
					for (Assignments::iterator i = a.begin(), e = a.end();  i != e; ++i) {
						int	j = -1;
						switch (i->state) {
							case Assignment::PixelShader:	j = SS_PIXEL; break;
							case Assignment::VertexShader:	j = SS_VERTEX; break;
							case Assignment::GeometryShader:j = SS_GEOMETRY; break;
							case Assignment::HullShader:	j = SS_HULL; break;
							case Assignment::DomainShader:	j = SS_LOCAL; break;
							default: continue;
						}
						if (i->type == Assignment::CAT_InlineShader) {
							Assignment::InlineShader	*data	= (Assignment::InlineShader*)i->data.get(base);
							const DataBlock				*data2	= data->shader.get(base);
							if (int size = data2->size) {
								memcpy(*(*pass)[j].Create(0, size / 4), data2->data, size);
							}
						}
					}
				}
			}
		}

		return pfx;

	}

} fx11;

