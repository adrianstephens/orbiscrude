#include "gpu.h"

#include "graphics.h"
#include "vector_string.h"
#include "windows/dib.h"
#include "shader.h"
#include "systems/mesh/shapes.h"
#include "viewmesh.rc.h"
#include "extra/si.h"
#include "utilities.h"

#define IDR_MENU_MESH			"IDR_MENU_MESH"
#define IDR_TOOLBAR_MESH		"IDR_TOOLBAR_MESH"
#define IDR_ACCELERATOR_MESH	"IDR_ACCELERATOR_MESH"

using namespace iso;
using namespace win;
using namespace app;

#if 1
Cursor	app::CURSOR_LINKBATCH = TextCursor(Cursor::LoadSystem(IDC_HAND), win::Font::DefaultGui(), "batch"),
		app::CURSOR_LINKDEBUG = TextCursor(Cursor::LoadSystem(IDC_HAND), win::Font::DefaultGui(), "debug"),
		app::CURSOR_ADDSPLIT = CompositeCursor(Cursor::LoadSystem(IDC_SIZENS), Cursor::Load("IDR_OVERLAY_ADD", 0));
#else
Cursor	app::CURSOR_LINKBATCH = Cursor::LoadSystem(IDC_HAND),
		app::CURSOR_LINKDEBUG = Cursor::LoadSystem(IDC_HAND),
		app::CURSOR_ADDSPLIT = Cursor::LoadSystem(IDC_SIZENS);
#endif

Control app::ErrorControl(const WindowPos &wpos, const char *error) {
	StaticControl	err(wpos, error, Control::CHILD | Control::VISIBLE | StaticControl::CENTER);
	err.Class().style |= CS_HREDRAW;
	err.SetFont(win::Font("Segoe UI", 32));
	return err;
}

#ifndef ISO_EDITOR

int		EditorGPU::num_frames	= 1;
bool	EditorGPU::until_halt	= false;
bool	EditorGPU::resume_after	= false;

bool EditorGPU::Command(MainWindow &main, ID id) {
	switch (id) {
		case 0: {
			Menu	menu	= main.GetDropDown(ID_ORBISCRUDE_GRAB);
			menu.Radio(ID_ORBISCRUDE_GRAB_PS4, ID_ORBISCRUDE_GRAB_TARGETS, ID_ORBISCRUDE_GRAB_TARGETS_MAX);
			menu.CheckByID(ID_ORBISCRUDE_GRAB_PAUSEAFTERCAP, !resume_after);
			menu.CheckByID(ID_ORBISCRUDE_GRAB_UNTILHALT, until_halt);
			menu.CheckByID(ID_ORBISCRUDE_GRAB_FRAMES1 + num_frames - 1);
			main.AddLoadFilters((char*)
				"Razor\0*.razor\0"
				"SB file\0*.sb\0"
			);
			return false;
		}

		case ID_ORBISCRUDE_GRAB_PAUSEAFTERCAP: {
			Menu	menu	= main.GetDropDown(ID_ORBISCRUDE_GRAB);
			resume_after = !resume_after;
			menu.CheckByID(id, !resume_after);
			return true;
		}

		case ID_ORBISCRUDE_GRAB_UNTILHALT: {
			Menu	menu	= main.GetDropDown(ID_ORBISCRUDE_GRAB);
			menu.CheckByID(id, until_halt = !until_halt);
			return true;
		}

		case ID_ORBISCRUDE_GRAB_FRAMES1:
		case ID_ORBISCRUDE_GRAB_FRAMES2:
		case ID_ORBISCRUDE_GRAB_FRAMES3:
		case ID_ORBISCRUDE_GRAB_FRAMES4:
		case ID_ORBISCRUDE_GRAB_FRAMES5:
		case ID_ORBISCRUDE_GRAB_FRAMES6:
		case ID_ORBISCRUDE_GRAB_FRAMES7:
		case ID_ORBISCRUDE_GRAB_FRAMES8: {
			Menu	menu	= main.GetDropDown(ID_ORBISCRUDE_GRAB);
			menu.CheckByID(ID_ORBISCRUDE_GRAB_FRAMES1 + num_frames - 1, false);
			menu.CheckByID(id);
			num_frames = id - ID_ORBISCRUDE_GRAB_FRAMES1 + 1;
			return true;
		}
	}
	return false;
}
#endif


int app::MakeHeaders(win::ListViewControl lv, int nc, const SoftType &type, string_accum &prefix) {
	for (int i = 0, n = type.Count(); i < n; i++) {
		char	*startp = prefix.getp();
		win::ListViewControl::Column(type.Name(prefix, i)).Width(75).Insert(lv, nc++);
		prefix.move(startp - prefix.getp());
	}
	return nc;
}

//-----------------------------------------------------------------------------
//	BatchList
//-----------------------------------------------------------------------------

string_accum &app::WriteBatchList(string_accum &sa, BatchList &bl) {
	for (auto i = bl.begin(), e = bl.end(); i != e;) {
		int		n = 16;
		sa.getp(n);
		sa.move(-n);
		if (n < 16)
			return sa << "...";

		uint32	start = *i, end = start;
		do {
			end = *i++;
		} while (i != e && *i == end + 1);

		sa << start;
		if (end > start)
			sa << "-" << end;

		if (i != e)
			sa << ',';
	}
	return sa;
}

int app::SelectBatch(HWND hWnd, const Point &mouse, BatchList &b, bool always_list) {
	if (b.size() == 0)
		return -1;

	uint32	batch	= b[0];

	if (always_list || b.size() > 1) {
		Menu	menu	= Menu::Popup();
		int		dy		= win::GetMenuSize().y;

		dy = GetSystemMetrics(SM_CYMENUSIZE);

		int		maxy	= Control::Desktop().GetClientRect().Height();
		int		y		= dy;

		menu.Append("Go to_batch:", 0, MF_DISABLED);
		for (uint32 *i = b.begin(), *e = b.end(); i != e;) {
			int	type = 0;
			if (y > maxy) {
				type	= MFT_MENUBARBREAK;
				y		= 0;
			}
			y += dy;

			uint32	start = *i, end = start;
			do {
				end = *i++;
			} while (i != e && *i == end + 1);

			if (end > start) {
				Menu	submenu	= Menu::Create();
				Menu::Item().Text(format_string("%i-%i", start, end)).Type(type).SubMenu(submenu).AppendTo(menu);
				int		y1		= dy * 2;
				for (int b = start; b <= end; b++) {
					Menu::Item().Text(to_string(b)).ID(b + 1).Type(y1 > maxy ? MFT_MENUBARBREAK : 0).AppendTo(submenu);
					if (y1 > maxy)
						y1		= 0;
					y1 += dy;
				}
			} else {
				Menu::Item().Text(to_string(start)).ID(start + 1).Type(type).AppendTo(menu);
			}

		}
		batch = menu.Track(hWnd, mouse, TPM_NONOTIFY | TPM_RETURNCMD);
		if (batch == 0)
			return -1;
		--batch;
	}
	return batch;
}

//-----------------------------------------------------------------------------
//	MeshVertexWindow
//-----------------------------------------------------------------------------

LRESULT MeshVertexWindow::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
			GetPane(0).SetFocus();
			break;

		case WM_SETFOCUS:
			GetPane(0).SetFocus();
			return 0;

		case WM_NOTIFY: {
			NMHDR	*nmh = (NMHDR*)lParam;
			switch (nmh->code) {
				case LVN_ITEMCHANGED: {
					NMLISTVIEW	*nmlv = (NMLISTVIEW*)nmh;
					if (nmlv->uNewState & LVIS_SELECTED) {
						if (auto mw = MeshWindow::Cast(GetPane(1)))
							mw->SetSelection(nmlv->iItem, false);
					}
					return 0;
				}
				case NM_CUSTOMDRAW: {
					NMCUSTOMDRAW *nmcd = (NMCUSTOMDRAW*)nmh;
					switch (nmcd->dwDrawStage) {
						case CDDS_PREPAINT:
							return CDRF_NOTIFYITEMDRAW;

						case CDDS_ITEMPREPAINT: {
							static const COLORREF cols[] = { RGB(255,255,255), RGB(224,224,224) };
							if (auto mw = MeshWindow::Cast(GetPane(1))) {
								NMLVCUSTOMDRAW 	*nmlvcd = (NMLVCUSTOMDRAW*)nmh;
								int				i		= nmcd->dwItemSpec;
								nmlvcd->clrTextBk		= cols[mw->PrimFromVertex(i) & 1];
								return mw->ChunkOffset(i + 1) == 0 ? CDRF_NEWFONT | CDRF_NOTIFYPOSTPAINT : CDRF_NEWFONT;
							}
							return CDRF_DODEFAULT;
						}

						case CDDS_ITEMPOSTPAINT: {
							ListViewControl	lv(nmcd->hdr.hwndFrom);
							DeviceContext	dc(nmcd->hdc);
							Rect			rect	= lv.GetItemRect(nmcd->dwItemSpec);//(nmcd->rc);
							dc.Fill(rect.Subbox(0, -2, 0, 0), win::Colour(0,0,0));
							return CDRF_DODEFAULT;
						}
					}
					break;
				}

				case MeshNotification::SET:
					return GetPane(0).SendMessage(message, wParam, lParam);
			}
			break;
		}
	}
	return SplitterWindow::Proc(message, wParam, lParam);
}

MeshVertexWindow::MeshVertexWindow(const WindowPos &wpos, text title, ID id) : SplitterWindow(SWF_VERT | SWF_PROP) {
	Create(wpos, title, CHILD | CLIPSIBLINGS | VISIBLE, NOEX, id);
	Rebind(this);
}

//-----------------------------------------------------------------------------
//	BufferWindow
//-----------------------------------------------------------------------------

struct BufferWindow : public Window<BufferWindow> {
	ListViewControl2	vw;
	EditControl2		edit_format;
	EditControl2		edit_control;

	TypedBuffer			buffer;
	uint32				edit_row, edit_col;
	bool				fixed_stride;

	static const int ID_EDITFORMAT = 0xedf;

	void	Fill() {
		ListViewControl2::Column("offset").Width(50).Insert(vw, 1);
		MakeHeaders(vw, 2, buffer.format, lvalue(buffer_accum<512>()));
		vw.SetCount(buffer.size32());
		SetColumnWidths(vw.GetHeader());
	}

	LRESULT Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
		switch (message) {
			case WM_CREATE: {
				vw.Create(GetChildWindowPos(), "verts", CHILD | CLIPSIBLINGS | VISIBLE | vw.OWNERDATA);
				vw.id	= id;
				return 0;
			}

			case WM_SIZE:
				vw.Resize(Point(lParam));
				break;

			case WM_CHAR:
				SetFocus();
				return 0;

			case WM_COMMAND:
				if (HIWORD(wParam) == EN_KILLFOCUS) {
#if 0//def REPLAY_H
					if (LOWORD(wParam) == ID_EDIT) {
						if (edit_control.GetText().len()) {
							if (SetComponent(string_scan(str<64>(edit_control.GetText())), edit_row, edit_col))
								vw.Invalidate(vw.GetSubItemRect(edit_row, edit_col + 2));
							edit_control.Destroy();
						}
						break;
					}
#endif
					if (LOWORD(wParam) == ID_EDITFORMAT) {
						fixed_string<256>	text = edit_format.GetText();
						if (const C_type *type = ReadCType(memory_reader(text.begin()), builtin_ctypes(), 0)) {
							Busy				bee;
							WithDisabledRedraw	h(*this);
							buffer.format = type;

							if (!fixed_stride)
								buffer.stride = type->size32();

							vw.DeleteAll();
							for (int n = vw.NumColumns(); n-- > 1;)
								vw.DeleteColumn(n);
							Fill();
						}
						return 1;
					}

				}
				break;
				//return Parent()(message, wParam, lParam);

			case WM_NOTIFY: {
				NMHDR	*nmh = (NMHDR*)lParam;
				switch (nmh->code) {
					case LVN_GETDISPINFO: {
						ListViewControl::Item	&i	= (ListViewControl::Item&)((NMLVDISPINFO*)nmh)->item;
						int						row	= i.iItem, col = i.iSubItem;
						if (i.mask & LVIF_TEXT) {
							fixed_accum	ba(i.TextBuffer());
							if (col == 0) {
								ba << row;

							} else if (col == 1) {
								ba << "0x" << hex(uint64(buffer.stride) * row);

							} else {
								void	*data = buffer[row];
								buffer.format.Get(ba, data, col - 2);
								//int		shift;
								//const C_type	*subtype = GetNth(data, buffer.format, col - 2, shift);
								//DumpData(ba, data, subtype, shift);
							}
						}
						return 1;
					}

					#if 0
					case NM_RCLICK: {
						NMITEMACTIVATE	*nma	= (NMITEMACTIVATE*)nmh;
						if (!buff.read_only) {
							Menu		menu	= Menu::Popup();
							Menu::Item("Debug this entry").ID(ID_DEBUG_PIXEL).AppendTo(menu);
							if (int id = menu.Track(*this, ToScreen(nma->ptAction), TPM_NONOTIFY | TPM_RIGHTBUTTON | TPM_RETURNCMD)) {
								uint64		target	= con->GetUse(con->Find(&buff)).u;
								auto		param	= make_pair(target, GetIndexOffset(nma->iItem, nma->iSubItem - 2));
								Parent()(WM_COMMAND, id, (LPARAM)&param);
							}
						}
						break;
					}

					case NM_DBLCLK: {
						NMITEMACTIVATE *iacc	= (NMITEMACTIVATE*)nmh;
						if (iacc->iSubItem >= 2) {
							EditLabel(vw, edit_control, iacc->iItem, iacc->iSubItem, ID_EDIT);
							edit_control.SetOwner(*this);
							edit_row	= iacc->iItem;
							edit_col	= iacc->iSubItem - 2;
							return 0;
						}
						break;
					}
					#endif
				}
				return Parent()(message, wParam, lParam);
			}

			case WM_NCDESTROY:
				delete this;
			case WM_DESTROY:
				return 0;
		}
		return Super(message, wParam, lParam);
	}

	void Init() {
		ListViewControl2::Column("#").Width(50).Insert(vw, 0);
		HeaderControl	hc	= vw.GetHeader();
		hc.style			= hc.style | CLIPSIBLINGS;
		hc.GetItem(0).Width(100).Format(LVCFMT_FIXED_WIDTH).Set(hc, 0);

		win::Rect headerRect = hc.GetItemRect(0);
		edit_format.Create(*this, "format", CHILD | VISIBLE | CLIPSIBLINGS | EditControl::AUTOHSCROLL | EditControl::WANTRETURN, NOEX, headerRect, ID_EDITFORMAT);
		edit_format.SetFont(hc.GetFont());
		edit_format.MoveBefore(vw);

		fixed_stride	= buffer.stride != 0;

		if (!fixed_stride)
			buffer.stride	= !buffer.format ? (uint32)min(lowest_set(buffer.raw_size()), 16) : buffer.format.Size();

		if (!buffer.format)
			buffer.format	= buffer.stride & 3 ? builtin_ctypes().get_array_type<uint8>(buffer.stride)
							: buffer.stride == 4 ? builtin_ctypes().get_static_type<float>()
							: builtin_ctypes().get_array_type<float>(buffer.stride / 4);

		//edit_format.SetText(DumpType(buffer_accum<256>(), buffer.format, 0, 0));
		edit_format.SetText(buffer.format.Type(lvalue(buffer_accum<256>()), -1, 0));
		Fill();
	}

	BufferWindow(const WindowPos &wpos, const char *title, ID id, const TypedBuffer &buffer) : buffer(buffer) {
		Create(wpos, title, CHILD | CLIPCHILDREN | CLIPSIBLINGS | VISIBLE, NOEX, id);
		Init();
	}
	BufferWindow(const WindowPos &wpos, const char *title, ID id, TypedBuffer &&buffer) : buffer(move(buffer)) {
		Create(wpos, title, CHILD | CLIPCHILDREN | CLIPSIBLINGS | VISIBLE, NOEX, id);
		Init();
	}
};

Control app::MakeBufferWindow(const WindowPos &wpos, text title, ID id, const TypedBuffer &buffer) {
	return *new BufferWindow(wpos, title, id, buffer);
}

Control app::MakeBufferWindow(const WindowPos &wpos, text title, ID id, TypedBuffer &&buffer) {
	return *new BufferWindow(wpos, title, id, move(buffer));
}

//-----------------------------------------------------------------------------
//	VertexWindow
//-----------------------------------------------------------------------------

LRESULT VertexWindow::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE:
			vw.Create(GetChildWindowPos(), "verts", CHILD | CLIPSIBLINGS | vw.OWNERDATA);
			vw.id	= id;
			return 0;

		case WM_SIZE:
			vw.Resize(Point(lParam));
			break;

		case WM_CHAR:
			if (wParam == 27)
				edit_control.SetText("");
			if (edit_control.hWnd)
				edit_control.Destroy();
			break;

		case WM_NOTIFY: {
			NMHDR	*nmh = (NMHDR*)lParam;
			switch (nmh->code) {
				case NM_CUSTOMDRAW:
					return CustomDraw((NMCUSTOMDRAW*)nmh, Parent());

				case LVN_GETDISPINFO: {
					ListViewControl::Item	&i	= (ListViewControl::Item&)((NMLVDISPINFO*)nmh)->item;
					int						row = i.iItem, col = i.iSubItem;
					if (i.mask & LVIF_TEXT) {
						fixed_accum	ba(i.TextBuffer());

						if (col-- == 0) {
							if (row < num_verts)
								ba << row;
							else
								ba << row % num_verts << "/" << row / num_verts;

						} else if (indexing && col-- == 0) {
							ba << indexing[row];

						} else {

							for (auto &b : buffers) {
								int	nc	= b.format.Count();
								if (col < nc) {
									if (uint32 div = b.divider) {
										row /= div;
									} else {
										row %= num_verts;
										if (indexing)
											row = indexing[row];
									}

									if (row < b.size()) {
										void	*data	= b[row];
										b.format.Get(ba, data, col);
									}
									break;
								}
								col -= nc;
							}
						}
					}
					return 1;
				}

				case MeshNotification::SET:
					return ((MeshNotification*)nmh)->Process(vw);
			}
			return Parent()(message, wParam, lParam);
		}

//		case WM_COMMAND:
//			return Parent()(message, wParam, lParam);

		case WM_NCDESTROY:
			delete this;
			return 0;

		default:
			if (message >= LVM_FIRST && message < LVM_FIRST + 0x100)
				return vw.SendMessage(message, wParam, lParam);
			break;
	}
	return Super(message, wParam, lParam);
}

VertexWindow::VertexWindow(const WindowPos &wpos, const char *title, ID id, dynamic_array<uint32> &&_indexing) : num_instances(1), indexing(move(_indexing)) {
	Create(wpos, title, CHILD | CLIPCHILDREN | CLIPSIBLINGS, NOEX, id);
	AddColumn(0, "#", 50, RGB(255,255,255));
	if (indexing)
		AddColumn(1, "index", 50, RGB(224,224,224));
}

uint32 VertexWindow::NumUnique() const {
	uint32	n = 0;
	for (auto& i : buffers)
		n = max(n, i.size32());
	return n;
}

void VertexWindow::Show() {
	Control::Show();
	SetColumnWidths(vw.GetHeader());

	num_verts = indexing.empty() ? NumUnique() : indexing.size32();
	vw.SetCount(num_verts * num_instances);
	vw.Show();
}

void VertexWindow::AddVertices(range<TypedBuffer*> _buffers, const dynamic_array<uint32> &_indexing, uint32 _num_instances) {
	bool	had_index = !indexing.empty();
	indexing.append(_indexing);

	if (indexing && !had_index)
		AddColumn(1, "index", 50, RGB(224,224,224));

	num_instances += _num_instances;

	for (auto&& i : make_pair(buffers, _buffers))
		i.a += i.b;

	num_verts = indexing.empty() ? NumUnique() : indexing.size32();
	vw.SetCount(num_verts * num_instances);
}


void VertexWindow::AddBuffer(const TypedBuffer& buffer, const char *name) {
	buffers.push_back(buffer);

	if (!!buffer.format) {
		int		nc	= vw.NumColumns();
		auto	x	= buffers.size() - 1;
		buffer_accum<512>		ba;
		if (name)
			ba << name;
		else
			ba << 'b' << x;

		nc		= MakeHeaders(vw, nc, buffer.format, ba);
		AddColour(nc, MakeColour(x + 1));
	}
}

VertexWindow *app::MakeVertexWindow(const WindowPos &wpos, text title, ID id, range<named<TypedBuffer>*> buffers, const indices &ix, uint32 num_instances) {
	auto	*c	= ix
		? new VertexWindow(wpos, title, id, ix)
		: new VertexWindow(wpos, title, id, {});
	c->num_instances	= num_instances;

	for (auto &i : buffers)
		c->AddBuffer(i, i.name());

	c->Show();
	return c;
}

//-----------------------------------------------------------------------------
//	VertexOutputWindow
//-----------------------------------------------------------------------------

LRESULT VertexOutputWindow::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE:
			vw.Create(GetChildWindowPos(), "vb", CHILD | CLIPSIBLINGS);
			return 0;

		case WM_SIZE:
			vw.Resize(Point(lParam));
			break;

		case WM_NOTIFY: {
			NMHDR	*nmh = (NMHDR*)lParam;
			switch (nmh->code) {
				case NM_CUSTOMDRAW:
					return CustomDraw((NMCUSTOMDRAW*)nmh, Parent());

				case MeshNotification::SET:
					return ((MeshNotification*)nmh)->Process(vw);
			}
			return Parent()(message, wParam, lParam);
		}

		case WM_NCDESTROY:
			delete this;
			return 0;

		default:
			if (message >= LVM_FIRST && message < LVM_FIRST + 0x100)
				return vw.SendMessage(message, wParam, lParam);
			break;
	}
	return Super(message, wParam, lParam);
}

void VertexOutputWindow::FillShaderIndex(int num) {
	for (int i = 0; i < num; i++) {
		char				text[64];
		ListViewControl2::Item	item(text);
		fixed_accum(text) << i;
		item.Insert(vw);
	}
}

//-----------------------------------------------------------------------------
//	SpacePartition
//-----------------------------------------------------------------------------

struct SpacePartition {
	cuboid		ext;
	uint64		*vecs;
	uint32		*indices;
	size_t		num;

	static uint64		mask1(const iorf &i)		{ return part_bits<1, 2, 21>(i.m >> 2); }
	static uint64		clamp_mask1(const iorf &i)	{ return part_bits<1, 2, 21>(i.e < 0x80 ? 0 : i.e == 0x80 ? i.m >> 2 : 0x1fffff); }
	static iorf			unmask1(uint64 m)			{ return iorf(0x40000000u | unpart_bits<1, 2, 21>(m) << 2); }

	static uint64		clamp_mask(param(position3) p);
	static uint64		mask(param(position3) p);
	static position3	unmask(uint64 m);

	float3x4			matrix()	const	{ return translate(float3(3)) * scale(0.999f) * ext.inv_matrix(); }
	int					closest(param(position3) p);
	bool				closest(param(position3) p0, param(position3) p1);

	void				init(const stride_iterator<const float3p> &begin, const stride_iterator<const float3p> &end);
	void				init(const stride_iterator<const float4p> &begin, const stride_iterator<const float4p> &end);
	void				init(const stride_iterator<const float3p> &begin, size_t n)	{ init(begin, begin + n); }
	void				init(const stride_iterator<const float4p> &begin, size_t n)	{ init(begin, begin + n); }

	SpacePartition() : ext(empty), vecs(0), indices(0), num(0) {}
	~SpacePartition() { delete[] vecs; delete[] indices; }
	SpacePartition(const stride_iterator<const float3p> &begin, const stride_iterator<const float3p> &end) { init(begin, end); }
	SpacePartition(const stride_iterator<const float4p> &begin, const stride_iterator<const float4p> &end) { init(begin, end); }
};

//-----------------------------------------------------------------------------
//	transpose_bits
//-----------------------------------------------------------------------------

//transpose MxN matrix of B bits
/*
template<typename T, int M, int N, int B, int I> struct s_transpose_bits {
	enum {
		S = B << (I - 1),
		M1	= (M >> (I - 1)) + N
	};
	static inline T f(T x) {
		T	mm = s_bitblocks<T, I * 4, 1>::value & ((T(1) << M) - 1);
		T	mn = s_bitblocks<T, M, 1>::value * mm;

		x = ((x & m) << M1) | ((x >> M) & m);

		x = (x & ~(m | (m << M))) | ((x & m) << M) | ((x >> M) & m);
		x = s_transpose_bits<T, M, N, B, I - 1>::f(x);
		return x;
	}
};

template<typename T, int M, int N, int B> T transpose_bits(T x) {
	return s_transpose_bits<T, M, N, B, 0>::f(x);
}

struct s_test_transpose {
	s_test_transpose() {
		uint64	t = 0x123456789ABCDEF0ull;
		t = transpose_bits<uint64,21,3,1>(t);
	}
} test_transpose;
*/
//-----------------------------------------------------------------------------
//	MeshWindow
//-----------------------------------------------------------------------------

/*
template<typename T, int N, int I> struct s_interleave_n<T, N, I> {
	static inline T f(T x) {
		T	m = s_bitblocks<T, (N + 1) << (I + 1), 1 << I>::value;
		return s_interleave_n<T, N, I - 1, true>::f((x & m) ^ ((x & ~m) << (N << I)));
	}
	static inline T g(T x) {
		x = s_interleave_n<T, N, I - 1>::g(x);
		T	m = s_bitblocks<T, (N + 1) << (I + 1), 1 << I>::value;
		return (x & m) ^ ((x & ~m) >> (N << I));
	}
};
template<typename T, int N> struct s_interleave_n<T, N, 0> {
	static inline T f(T x) {
		T	m = s_bitblocks<T, (N + 1) * 2, 1>::value;
		return (x & m) ^ ((x & ~m) << N);
	}
	static inline T g(T x) {
		T	m = s_bitblocks<T, (N + 1) * 2, 1>::value;
		return (x & m) ^ ((x & ~m) >> N);
	}
};
template<typename T, int N> inline T interleave(T x)	{ return s_interleave_n<unsigned_t<T>, sizeof(T) * 2>::f(x); }
template<typename T, int N> inline T uninterleave(T x)	{ return s_interleave_n<unsigned_t<T>, sizeof(T) * 2>::g(x); }

void SpacePartition::insert(param(position3) p) {
	uint64	x	= mask(p);
	node	*n	= &root;

	for (int b = 21; b--;) {
		uint8	i = uint8(x >> (b * 3)) & 7;
		uint8	c = n->count(i);
		if (c == 0) {
			if (node *n2 = n->child[i].n) {
				n = n2;
				continue;
			}
			n->child[i].v = new uint64(x);
			n->inc(i);
			return;

		} else if (c == 15) {
			uint64	*v0	= n->child[i].v, *v1 = v0 + 15, *v = v0;
			n = n->makenode(i);

			for (int j = 0; j < 8; j++) {
				while (v < v1 && (uint8(*v >> (b * 3)) & 7) <= j)
					++v;
				if (int c = v - v0) {
					n->flags	|= c << (j * 4);
					n->child[j].v = v;
					v0 = v;
				}
			}
		} else {
			uint64	*v	= n->child[i].v, *v1 = v + c;
			while (v < v1 && x > *v)
				v++;
			while (v1 > v) {
				v1[0] = v1[-1];
				--v1;
			}
			*v	= x;
			n->inc(i);
			return;
		}
	}
}
*/
uint64 SpacePartition::clamp_mask(param(position3) p) {
	iorf	*i = (iorf*)&p;
	return clamp_mask1(i[0]) | (clamp_mask1(i[1]) << 1) | (clamp_mask1(i[2]) << 2);
}

uint64 SpacePartition::mask(param(position3) p) {
	iorf	*i = (iorf*)&p;
	//ISO_ASSERT(i[0].e == 0x80 && i[1].e == 0x80 && i[2].e == 0x80);
	return (mask1(i[0]) << 0) | (mask1(i[1]) << 1) | (mask1(i[2]) << 2);
}

position3 SpacePartition::unmask(uint64 m) {
	return position3(unmask1(m >> 0).f(), unmask1(m >> 1).f(), unmask1(m >> 2).f());
}

/*
void SpacePartition::init(const stride_iterator<const float3p> &begin, const stride_iterator<const float3p> &end) {
	num		= distance(begin, end);
	vecs	= new uint64[num];
	indices	= new uint32[num];

	ext = empty;
	for (stride_iterator<const float3p> i = begin; i != end; ++i)
		ext |= position3(*i);
	ext.p1 += (ext.extent() == zero).select(one, zero);

	float3x4	mat	= matrix();
	uint64		*p	= vecs;
	for (stride_iterator<const float3p> i = begin; i != end; ++i)
		*p++ = mask(mat * position3(*i));

	for (int i = 0; i < num; ++i)
		indices[i] = i;

	buddy_iterator<uint64*, uint32*>	buddy(vecs, indices);
	sort(buddy, buddy + num);
}

void SpacePartition::init(const stride_iterator<const float4p> &begin, const stride_iterator<const float4p> &end) {
	num		= distance(begin, end);
	vecs	= new uint64[num];
	indices	= new uint32[num];

	ext = empty;
	for (stride_iterator<const float4p> i = begin; i != end; ++i)
		ext |= project(float4(*i));
	ext.p1 += (ext.extent() == zero).select(one, zero);

	float3x4	mat	= matrix();
	uint64		*p	= vecs;
	for (stride_iterator<const float4p> i = begin; i != end; ++i)
		*p++ = mask(mat * project(float4(*i)));

	for (int i = 0; i < num; ++i)
		indices[i] = i;

	buddy_iterator<uint64*, uint32*>	buddy(vecs, indices);
	sort(buddy, buddy + num);
}

int SpacePartition::closest(param(position3) p) {
	float3x4	mat	= matrix();
	uint64		m	= clamp_mask(mat * p);
	uint64		*a	= lower_bound(vecs, vecs + num, m);
	if (a == vecs + num)
		return -1;

	if (a == vecs)
		return indices[0];

	uint64	c	= 0x1249249249249249ull;

	uint64	d	= m ^ *a;
//	uint64	d	= a[0] ^ a[-1];
	int		s	= highest_set_index(d);
	switch (s % 3) {
		case 0: {
			m &= ~((c * 6) >> (63 - s));
			a = lower_bound(vecs, vecs + num, m);
			break;
		}
		case 1:
			m -= 2ull << s;
			a = lower_bound(vecs, vecs + num, m);
			break;
		case 2:
			break;
	}

	if (a != vecs && m - a[-1] < a[0] - m)
		--a;
	return indices[a - vecs];
}

bool SpacePartition::closest(param(position3) _p0, param(position3) _p1) {
	position3	p0	= _p0;
	position3	p1	= _p1;

	if (ext.clip(p0, p1))
		return false;

	float3x4	mat	= matrix();
	p0				= mat * p0;
	p1				= mat * p1;
	vector3		d	= p1 - p0;
	float		dd	= reduce_max(d);
	uint32		di = iorf(dd).m >> 2;

	uint64		m0	= mask(p0);
	uint64		m1	= mask(p1);
	uint64		*v0	= lower_bound(vecs, vecs + num, m0);
	uint64		*v1	= lower_bound(vecs, vecs + num, m1);

	uint64		dm	= m0 ^ m1;
	uint64		h	= highest_set((dm | (dm >> 1) | (dm >> 2)) & s_bitblocks<uint64,3,1>::value);

	for (;;) {
		bool	xcross	= dm & (h << 0);
		bool	ycross	= dm & (h << 1);
		bool	zcross	= dm & (h << 2);
	}

	return true;
}
*/

//-----------------------------------------------------------------------------
//	Topology
//-----------------------------------------------------------------------------

PrimType GetHWType(const Topology2 &top, int &num_prims) {
	static const PrimType conv[] = {
		PRIM_UNKNOWN,	//UNKNOWN
		PRIM_POINTLIST,	//POINTLIST
		PRIM_LINELIST,	//LINELIST
		PRIM_LINESTRIP,	//LINESTRIP
		PRIM_LINELIST,	//LINELOOP
		PRIM_TRILIST,	//TRILIST
		PRIM_TRISTRIP,	//TRISTRIP
		PRIM_TRIFAN,	//TRIFAN
		PRIM_RECTLIST,	//RECTLIST
		PRIM_TRILIST,	//QUADLIST,
		PRIM_TRISTRIP,	//QUADSTRIP,
		PRIM_TRIFAN,	//POLYGON,
		PRIM_LINELIST,	//LINELIST_ADJ
		PRIM_LINESTRIP,	//LINESTRIP_ADJ
		PRIM_TRILIST,	//TRILIST_ADJ
		PRIM_TRISTRIP,	//TRISTRIP_ADJ
		PRIM_POINTLIST,	//PATCH,
	};
	num_prims *= top.hw_mul;
	return conv[top.hw.type];
}

//-----------------------------------------------------------------------------
//	Tesselation
//-----------------------------------------------------------------------------

static uint16	*put_tri(uint16 *p, uint32 a, uint32 b, uint32 c) {
	p[0] = a;
	p[1] = b;
	p[2] = c;
	return p + 3;
}
static uint16	*put_quad(uint16 *p, uint32 a, uint32 b, uint32 c, uint32 d) {
	p[0] = a;
	p[1] = p[4] = b;
	p[2] = p[3] = d;
	p[5] = c;
	return p + 6;
}

static uint16	*put_strip(uint16 *p, uint32 a0, int an, uint32 b0, int bn) {
	if (an < 0 || bn < 0)
		return p;

	for (uint32 n = min(an, bn); n--; ++a0, ++b0)
		p = put_quad(p, a0, b0, b0 + 1, a0 + 1);

	if (an < bn) {
		for (uint32 n = bn - an; n--; ++b0)
			p = put_tri(p, a0, b0, b0 + 1);
	} else {
		for (uint32 n = an - bn; n--; ++a0)
			p = put_tri(p, a0, b0, a0 + 1);
	}
	return p;
}

//quad
Tesselation::Tesselation(param(float4) edges, param(float2) inside, Spacing spacing) {
	if (any(edges <= 0))
		return;

	auto	ei	= effective(spacing, inside);
	auto	ee	= effective(spacing, edges);

	if (all(ee == 1)) {
		if (any(ei == 1)) {
			auto	*p	= uvs.resize(4).begin();
			p[0] = float2{0, 0};
			p[1] = float2{1, 0};
			p[2] = float2{0, 1};
			p[3] = float2{1, 1};

			put_quad(indices.resize(2 * 3).begin(), 3, 2, 1, 0);
			return;
		}
	}
	
	ei = select(ei == 1, effective_min(spacing), ei) - 2;

	uint32	nuv_outer	= reduce_add(ee);
	uint32	nuv			= nuv_outer + reduce_mul(ei + 1);
	uint32	ntri		= nuv_outer + (ei.x + ei.y) * 2 + ei.x * ei.y * 2;

	auto	*p			= uvs.resize(nuv).begin();
	auto	*ix			= indices.resize(ntri * 3).begin();
	uint32	a			= 0, b = nuv_outer;
	float4	fi			= one - (to<float>(ee) - edges) * half;

	//outer ring uvs
	*p++ = zero;
	for (int i = 0; i < ee.x - 1; i++)
		*p++ = float2{(i + fi.x) / edges.x, 0};

	*p++ = float2{1, 0};
	for (int i = 0; i < ee.y - 1; i++)
		*p++ = float2{1, (i + fi.y) / edges.y};

	*p++ = float2{1, 1};
	for (int i = 0; i < ee.z - 1; i++)
		*p++ = float2{1 - (i + fi.z) / edges.z, 1};

	*p++ = float2{0, 1};
	for (int i = 0; i < ee.w - 1; i++)
		*p++ = float2{0, 1 - (i + fi.w) / edges.w};

	//outer ring indices
	ix	= put_strip(ix, a, ee.x, b, ei.x);
	a	+= ee.x;
	b	+= ei.x;

	ix = put_strip(ix, a, ee.y, b, ei.y);
	a	+= ee.y;
	b	+= ei.y;

	ix	= put_strip(ix, a, ee.z, b, ei.x);
	a	+= ee.z;
	b	+= ei.x;

	ix	= put_strip(ix, a, ee.w - 1, b, ei.y - 1);
	if (ei.y > 0)
		ix	= put_quad(ix, 0, a + ee.w - 1, b + ei.y - 1, nuv_outer);
	else
		ix	= put_quad(ix, 0, b, a + ee.w - 2, a + ee.w - 1);

	a	+= ee.w;
	b	+= ei.y;

	//inner rings

	float2	t = (one - to<float>(ei) / inside) * half;

	while (ei.x > 0 && ei.y > 0) {
		//uvs
		for (int i = 0; i < ei.x; i++)
			*p++ = float2{t.x + i / inside.x, t.y};

		for (int i = 0; i < ei.y; i++)
			*p++ = float2{1 - t.x, t.y + i / inside.y};

		for (int i = 0; i < ei.x; i++)
			*p++ = float2{1 - t.x - i / inside.x, 1 - t.y};

		for (int i = 0; i < ei.y; i++)
			*p++ = float2{t.x, 1 - t.y - i / inside.y};

		//indices
		if (ei.x == 1) {
			for (int i = 0; i < ei.y; i++)
				ix	= put_quad(ix, (i == 0 ? a : b - i), b - i - 1, a + i + 2, a + i + 1);

		} else if (ei.y == 1) {
			for (int i = 0; i < ei.x; i++)
				ix	= put_quad(ix, (i == 0 ? a : b - i), b - i - 1, a + i + 2, a + i + 1);

		} else {
			ix	= put_strip(ix, a, ei.x, b, ei.x - 2);
			ix	= put_strip(ix, a + ei.x, ei.y, b + ei.x - 2, ei.y - 2);
			ix	= put_strip(ix, a + ei.x + ei.y, ei.x, b + ei.x + ei.y - 4, ei.x - 2);

			uint32	c	= b + (ei.x + ei.y - 4) * 2;

			if (ei.y > 2) {
				ix	= put_strip(ix, b - ei.y, ei.y - 1, b + ei.x + ei.y + ei.x - 6, ei.y - 3);
				ix	= put_quad(ix, a, b - 1, c - 1, b);
			} else {
				ix	= put_quad(ix, a, b - 1, b - 2, c);
			}
			a	= b;
			b	= c;
		}

		t += one / inside;

		ei.x -= 2;
		ei.y -= 2;
	}

	if (ei.x == 0) {
		for (int i = 0; i <= ei.y; i++)
			*p++ = t + float2{0, i / inside.y};

	} else if (ei.y == 0) {
		for (int i = 0; i <= ei.x; i++)
			*p++ = t + float2{i / inside.x, 0};
	}
}

//tri
Tesselation::Tesselation(param(float3) edges, float inside, Spacing spacing) {
	if (any(edges <= 0))
		return;

	int		ei	= effective(spacing, inside);
	auto	ee	= effective(spacing, edges);
	if (all(ee == 1)) {
		if (ei == 1) {
			auto	*p	= uvs.resize(3).begin();
			p[0] = float2{0, 0};
			p[1] = float2{1, 0};
			p[2] = float2{0, 1};

			put_tri(indices.resize(3).begin(), 2, 1, 0);
			return;
		}
		ei = effective_min(spacing);
	}

	uint32	nuv_outer	= reduce_add(ee);
	uint32	nuv_inner	= (ei & 1 ? square(ei + 1) * 3 / 4 : (square(ei / 2) + ei / 2) * 3 + 1) - ei * 3;
	uint32	ntri_outer	= nuv_outer + (ei - 2) * 3;
	uint32	ntri_inner	= ei & 1 ? (ei - 3) * (ei - 1) * 3 / 2 + 1 : (ei - 4) * ei * 3 / 2 + 6;

	auto	*p			= uvs.resize(nuv_outer + nuv_inner).begin();
	auto	*ix			= indices.resize((ntri_outer + ntri_inner) * 3).begin();
	uint32	a			= 0, b = nuv_outer;
	float3	fi			= one - (to<float>(ee) - edges) * half;

	ei -= 2;

	//outer ring uvs

	// 1,0,0 ... 1-t,t,0 ... 0,1,0
	*p++ = float2{1, 0};
	for (int i = 0; i < ee.x - 1; i++) {
		float	t = (i + fi.x) / edges.x;
		*p++ = float2{1 - t, t};
	}

	// 0,1,0 ... 0,1-t,t ... 0,0,1
	*p++ = float2{0, 1};
	for (int i = 0; i < ee.y - 1; i++) {
		float	t = (i + fi.y) / edges.y;
		*p++ = float2{0, 1 - t};
	}

	// 0,0,1 ... t,0,1-t ... 1,0,0
	*p++ = float2{0, 0};
	for (int i = 0; i < ee.z - 1; i++) {
		float	t = (i + fi.z) / edges.z;
		*p++ = float2{t, 0};
	}

	//outer ring indices
	ix	= put_strip(ix, a, ee.x, b, ei);
	a	+= ee.x;
	b	+= ei;

	ix = put_strip(ix, a, ee.y, b, ei);
	a	+= ee.y;
	b	+= ei;

	ix	= put_strip(ix, a, ee.z - 1, b, ei - 1);
	if (ei > 0)
		ix	= put_quad(ix, 0, a + ee.z - 1, b + ei - 1, nuv_outer);
	else
		ix	= put_quad(ix, 0, a + ee.z - 1, a + ee.z - 2, b);

	a	+= ee.z;
	b	+= ei;

	//inner rings
	float	t	= (1 - ei / inside) / 2 * 4 / 5;

	while (ei > 0) {
		//uvs
		for (int i = 0; i < ei; i++)
			*p++ = float2{t + (ei - i) / inside, t + i / inside};

		for (int i = 0; i < ei; i++)
			*p++ = float2{t, t + (ei - i) / inside};

		for (int i = 0; i < ei; i++)
			*p++ = float2{t + i / inside, t};

		//indices
		if (ei == 1) {
			ix	= put_tri(ix, a + 2, a + 1, a + 0);

		} else {
			ix	= put_strip(ix, a, ei, b, ei - 2);
			ix	= put_strip(ix, a + ei, ei, b + ei - 2, ei - 2);

			uint32	c	= b + (ei - 2) * 3;
			if (ei > 2) {
				ix	= put_strip(ix, a + ei * 2, ei - 1, b + ei * 2 - 4, ei - 3);
				ix	= put_quad(ix, a, b - 1, c - 1, b);
			} else {
				ix	= put_quad(ix, a, b - 1, b - 2, c);
			}
			a	= b;
			b	= c;
		}

		t	+= 1 / inside * 4 / 5;
		ei	-= 2;
	}

	if (ei == 0)
		*p++ = float2{t, t};

}

//isoline
Tesselation::Tesselation(param(float2) edges, Spacing spacing) {
	if (any(edges <= 0))
		return;

	uint32	e0 = effective(EQUAL, edges.x);
	uint32	e1 = effective(spacing, edges.y);
	auto	*p	= uvs.resize(e0 * (e1 + 1)).begin();

	for (int x = 0; x < e0; x++) {
		float	u = float(x) / e0;

		*p++ = float2{u, 0};
		float	fy = 1 - (e1 - edges.y) / 2;
		for (int y = 0; y < e1 - 1; y++) {
			float	v = (y + fy) / edges.y;
			*p++ = float2{u, v};
		}
		*p++ = float2{u, 1};
	}
}

//-----------------------------------------------------------------------------
//	Finders
//-----------------------------------------------------------------------------

//template<typename I> dynamic_array<cuboid> get_extents(I prims, int num_prims) {
//	dynamic_array<cuboid>	exts(num_prims);
//	for (int i = 0; i < num_prims; ++i)
//		exts[i] = get_box(prims[i]);
//	return exts;
//}

int FindVertex(param(float4x4) mat, const kd_tree<dynamic_array<float3p>> &partition) {
	typedef kd_tree<dynamic_array<float3p>>::kd_node	kd_node;

	struct stack_element {
		const kd_node	*node;
		float			t0, t1;
	};

	stack_element	stack[32], *sp = stack;
	const kd_node	*node = partition.nodes.begin();

	float4x4 cof		= cofactors(mat);
	float	min_dist	= maximum;
	int		min_index	= -1;
	float	t0			= 0, t1 = maximum;

	for (;;) {
		while (node->axis >= 0) {
			float4	p		= cof[node->axis] - cof.w * node->split;
			int		side	= p.w > zero;
			float	t		= -p.w / p.z;

			if (t > 0) {
				if (t < t1) {
					sp->node	= node->child[1 - side];
					sp->t0		= t;
					sp->t1		= t1;
					++sp;
					t1		= t;
				}
				if (t < t0)
					break;
			}
			node	= node->child[side];
		}

		if (node->axis < 0) {
			kd_leaf	*leaf = (kd_leaf*)node;
			for (auto &i : leaf->indices) {
				position3	p		= project(mat * float4(position3(partition.data[i])));
				float		dist	= len2(p.v.xy);
				if (dist < min_dist) {
					min_dist	= dist;
					min_index	= i;
				}
			}
		}

		if (sp == stack)
			return min_index;

		--sp;
		node	= sp->node;
		t0		= sp->t0;
		t1		= sp->t1;
	}

	return -1;
}

//int FindFromIndex(const uint32 *p, int v) {
//	for (const uint32 *p2 = p; ; ++p2) {
//		if (*p2 == v)
//			return p2 - p;
//	}
//}

template<typename I> int FindVertex(param(float4x4) mat, I verts, int num_verts, float tol) {
	float	closestd	= tol;
	int		closestv	= -1;
	for (int i = 0; i < num_verts; ++i) {
		position3	p = project(mat * float4(verts[i]));
		float		d	= len2(p.v.xy);
		if (d < closestd) {
			closestd = d;
			closestv = i;
		}
	}
	return closestv;
}

template<typename I> int FindPrim(param(float4x4) mat, I prims, int num_prims) {
	float4		pn			= float4(zero, zero, -one, one) / mat;
	float4		pf			= float4(zero, zero, one, one) / mat;
	position3	from		= project(pn);
	float3		dir			= select(pf.w == zero, pf.xyz, project(pf) - from);

	float		closestt	= 1;
	int			closestf	= -1;

	position3		pos[64];
	for (int i = 0; i < num_prims; ++i) {
		auto	prim		= prims[i];
		uint32	num_verts	= prim.size();
		if (num_verts < 3)
			continue;

		copy(prim, pos);

		if (num_verts > 3) {
			if (!get_box(pos, pos + num_verts).contains(zero))
				continue;
		}

		triangle3	tri(pos[0], pos[1], pos[2]);
		float		t;
		if (tri.ray_check(from, dir, t)) {
			if (closestf < 0 || t <= closestt) {
				closestt = t;
				closestf = i;
			}
		}
	}
	return closestf;
}

template<typename I> int FindVertPrim(param(float4x4) mat, I prims, int num_prims, int num_verts, float tol, int &face) {
	face	= prims->size() < 3 ? -1 : FindPrim(mat, prims, num_prims);
	if (face >= 0) {
		float4	pos[64];
		copy(prims[face], pos);
		return FindVertex(mat, pos, 3, 1e38f);
	}
	return FindVertex(mat, prims.i, num_verts, tol);
}

template<typename I> int FindVertPrim(octree &oct, param(float4x4) mat, I prims, int num_prims, int &face) {
	if (!oct.nodes) {
		dynamic_array<cuboid>	exts(num_prims);
		cuboid					*ext	= exts;
		for (auto &&i : make_range_n(prims, num_prims))
			*ext++ = cuboid(get_extent(i));
		oct.init(exts, num_prims);
	}

	face	= oct.shoot_ray(mat, 0.25f, [prims](int i, param(ray3) r, float &t) {
		return prim_check_ray(prims[i], r, t);
	});

	if (face >= 0) {
		float4	pos[64];
		copy(prims[face], pos);
		return FindVertex(mat, pos, 3, 1e38f);
	}
	return -1;
}


//-----------------------------------------------------------------------------
//	Axes
//-----------------------------------------------------------------------------

void Axes::draw(GraphicsContext &ctx, param(float3x3) rot, const ISO::Browser &params)	const {
	ctx.SetDepthTestEnable(true);
	ctx.SetDepthTest(DT_USUAL);
	DrawAxes(ctx, .05f, .2f, .2f, 0.25f, tech, (float3x4)rot, params);
}

int Axes::click(const Rect &client, const point &mouse, param(float3x3) rot) const {
	return GetAxesClick(proj() * rot, (to<float>(mouse) - float2{size / 2, client.Height() - size / 2}) / float2{size / 2, size / 2});
}

//-----------------------------------------------------------------------------
//	MeshWindow
//-----------------------------------------------------------------------------

struct vertex_idx;
struct vertex_norm;

struct float3_in4 {
	float4p	v;
	operator position3() const { return project(float4(v)); }
};

struct vertex_tex {
	float3p	pos;
	float2p	uv;
};

namespace iso {
template<> VertexElements GetVE<vertex_idx>() {
	static VertexElement ve[] = {
		VertexElement(0, GetComponentType<float4p>(), "position"_usage, 0),
		VertexElement(0, GetComponentType<uint8[4]>(), "bones"_usage, 1)
	};
	return ve;
};
template<> VertexElements GetVE<vertex_tex>() {
	static VertexElement ve[] = {
		VertexElement(&vertex_tex::pos, "position"_usage),
		VertexElement(&vertex_tex::uv, "texcoord"_usage)
	};
	return ve;
};
template<> VertexElements GetVE<vertex_norm>() {
	static VertexElement ve[] = {
		VertexElement(0, GetComponentType<float4p>(), "position"_usage, 0),
		VertexElement(0, GetComponentType<float3p>(), "normal"_usage, 1)
	};
	return ve;
};
}

void MeshWindow::SetScissor(param(rectangle) _scissor) {
	scissor = cuboid(position3(_scissor.a, zero), position3(_scissor.b, one));
	flags.set(SCISSOR);
}

void MeshWindow::SetScissor(param(cuboid) _scissor) {
	scissor = _scissor;
	flags.set(SCISSOR);
}

void MeshWindow::SetDepthBounds(float zmin, float zmax) {
	if (!flags.test(SCISSOR))
		SetScissor(cuboid(cuboid::with_centre(position3(viewport.y), viewport.x)));
	scissor.a.v.z = zmin;
	scissor.b.v.z = zmax;
}

void MeshWindow::SetScreenSize(const point &s) {
	screen_size = s;
	flags.set(SCREEN_RECT);
}

void MeshWindow::SetColourTexture(const Texture &_ct) {
	ct = _ct;
	Invalidate();
}

Texture MipDepth(const Texture &_dt) {
	point	size	= _dt.Size();
	int		width	= size.x, height = size.y;
	int		mips	= log2(max(width, height));
	Texture	dt2(TEXF_R32F, width, height, 1, mips, MEM_TARGET);

	GraphicsContext		ctx;
	graphics.BeginScene(ctx);

	Texture	dt3 = _dt;
	AddShaderParameter("_zbuffer", dt3);
	static pass	*depth_min		= *ISO::root("data")["default"]["depth_min"][0];

	for (int i = 0; i < mips; i++) {
		AddShaderParameter("mip", 0);//max(i - 1, 0));
		ctx.SetRenderTarget(dt2.GetSurface(i));
		Set(ctx, depth_min);
		ImmediateStream<vertex_tex>	ims(ctx, PRIM_QUADLIST, 4);
		vertex_tex	*p	= ims.begin();
		p[0].pos = float3{+1, -1, 1}; p[0].uv = float2{1, 0};
		p[1].pos = float3{-1, -1, 1}; p[1].uv = float2{0, 0};
		p[2].pos = float3{-1, +1, 1}; p[2].uv = float2{0, 1};
		p[3].pos = float3{+1, +1, 1}; p[3].uv = float2{1, 1};
		dt3 = Texture(dt2.GetSurface(i));
	}
	graphics.EndScene(ctx);
	return dt2;
}

void MeshWindow::SetDepthTexture(const Texture &_dt) {
	dt = _dt;
//	dt = MipDepth(_dt);
	Invalidate();
}

void MeshWindow::GetMatrices(float4x4 *world, float4x4 *proj) const {
	float2		size	= to<float>(Size());
	float4x4	vpmat	= (float4x4)float3x3(scale(viewport.x * float3{1, 1, zscale}));
	float2		zoom2	= reduce_min(abs(size / viewport.x.xy)) * viewport.x.x * zoom / size;

	switch (mode) {
		case SCREEN_PERSP: {
			float4x4 unpersp = float4x4(
				float4{1,0,0,0},
				float4{0,1,0,0},
				float4{0,0,0,-zoom},	//-0.9f
				float4{0,0,1 + viewport.y.z / viewport.x.z,1}
			);

			*proj	= perspective_projection(zoom2 * float2{0.5f, -0.5f}, 1.f);
			*world	= vpmat * unpersp;
			break;
		}
		case SCREEN:
			vpmat.w.z = (viewport.y.z - half) * zscale;
			*proj	= float4x4(float3x3(scale(concat(zoom2, zero))));
			*world	= vpmat;
			break;

		case FIXEDPROJ:
			*proj	= scale(concat(zoom2, rlen(extent.extent()), one)) * fixed_proj;
			*world	= float4x4(translate(-extent.centre()));
			break;

		default:
			*proj	= perspective_projection(zoom2, move_scale * 10 / 256);
			*world	= float4x4(translate(-extent.centre()));
			break;
	}
}

float4x4 MeshWindow::GetMatrix() const {
	float4x4	world, proj;
	GetMatrices(&world, &proj);
	return proj * (float3x4)view_loc * world;
}

LRESULT MeshWindow::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE:
			addref();
			toolbar.Create(*this, NULL, CHILD | CLIPSIBLINGS | VISIBLE | toolbar.NORESIZE | toolbar.FLAT | toolbar.TOOLTIPS);
			toolbar.Init(IDR_TOOLBAR_MESH);
			toolbar.CheckButton(ID_MESH_BACKFACE, cull == BFC_FRONT);
			toolbar.CheckButton(ID_MESH_BOUNDS, flags.test(BOUNDING_EDGES));
			toolbar.CheckButton(ID_MESH_FILL, flags.test(FILL));
			SetSize((RenderWindow*)hWnd, GetClientRect().Size());
			break;

		case WM_SIZE:
			SetSize((RenderWindow*)hWnd, Point(lParam));
			toolbar.Resize(toolbar.GetItemRect(toolbar.Count() - 1).BottomRight() + Point(3, 3));
			break;

		case WM_PAINT:
			Paint();
			break;

		case WM_LBUTTONDOWN: {
			Point		mouse(lParam);
			if (int axis = axes.click(GetClientRect(), mouse, view_loc.rot)) {
				quaternion	q = GetAxesRot(axis);
				if (all(q == target_loc.rot))
					q = GetAxesRot(-axis);
				target_loc = view_loc;
				target_loc.rot = q;
				Timer::Start(0.01f);
				break;
			}
			Select(mouse);
		}
		// fall through
		case WM_RBUTTONDOWN:
			Stop();
			prevmouse	= Point(lParam);
			prevbutt	= wParam;
			SetFocus();
			break;

		case WM_LBUTTONDBLCLK:
			ResetView();
			break;

		case WM_MOUSEACTIVATE:
			SetAccelerator(*this, IDR_ACCELERATOR_MESH);
			break;

		case WM_MOUSEMOVE:
			MouseMove(Point(lParam), wParam);
			break;

		case WM_MOUSEWHEEL:
			if (GetRect().Contains(Point(lParam)))
				MouseWheel(ToClient(Point(lParam)), LOWORD(wParam), (short)HIWORD(wParam));
			break;

		case WM_COMMAND:
			switch (uint16 id = LOWORD(wParam)) {
				case ID_MESH_BACKFACE:
					cull = cull == BFC_BACK ? BFC_FRONT : BFC_BACK;
					toolbar.CheckButton(id, cull == BFC_FRONT);
					Invalidate();
					break;
				case ID_MESH_RESET:
					ResetView();
					break;
				case ID_MESH_BOUNDS:
					toolbar.CheckButton(id, flags.flip_test(BOUNDING_EDGES));
					Invalidate();
					break;
				case ID_MESH_FILL:
					toolbar.CheckButton(id, flags.flip_test(FILL));
					Invalidate();
					break;
				case ID_MESH_PROJECTION:
					if (mode == FIXEDPROJ) {
						mode = prev_mode;
					} else {
						prev_mode = mode;
						mode	= FIXEDPROJ;
					}
					Invalidate();
					break;
				default:
					return 0;
			}
			return 1;

		case WM_ISO_TIMER: {
			float		rate	= 0.1f;

			if (mode == SCREEN)
				zoom	= lerp(zoom, 0.8f, rate);

			view_loc = lerp(view_loc, target_loc, rate);
			if (approx_equal(view_loc, target_loc)) {
				view_loc	= target_loc;
				Stop();
			}
			Invalidate();
			break;
		}

		case WM_NOTIFY: {
			NMHDR	*nmh = (NMHDR*)lParam;
			switch (nmh->code) {
				case TTN_GETDISPINFOA: {
					TOOLTIPTEXT	*ttt	= (TOOLTIPTEXT*)nmh;
					ttt->hinst			= GetLocalInstance();
					ttt->lpszText		= MAKEINTRESOURCE(ttt->hdr.idFrom);
					break;
				}
			}
			break;
		}
		case WM_NCDESTROY:
			release();//delete this;
			return 0;
	}
	return Super(message, wParam, lParam);
}

void MeshWindow::MouseMove(Point mouse, int buttons) {
	if (prevbutt) {
		if (buttons & MK_RBUTTON) {
			float3	move{float(mouse.x - prevmouse.x), float(mouse.y - prevmouse.y), zero};
			view_loc *= translate(move * move_scale / 256);
			Invalidate();

		} else if (buttons & MK_LBUTTON) {
			quaternion	rot	= normalise(quaternion(int(prevmouse.y - mouse.y), int(mouse.x - prevmouse.x), 0, 256));
			if (buttons & MK_CONTROL) {
				light_rot =  rot * light_rot;

			} else if (mode == SCREEN_PERSP) {
				float4x4	world, proj;
				GetMatrices(&world, &proj);
				view_loc = rotate_around_to(view_loc, world * extent.centre(), view_loc.rot * rot);

			} else {
				view_loc = rotate_around(view_loc, extent.centre(), rot);
				//view_loc = view_loc * rot;

			}
			Invalidate();
		}
	}
	prevmouse	= mouse;
}

void MeshWindow::MouseWheel(Point mouse, int buttons, int x) {
	Stop();
	float	mag	= iso::pow(1.05f, x / 64.f);
	switch (mode) {
		case SCREEN:
			zoom	*= mag;
			break;

		case SCREEN_PERSP:
			if (buttons & MK_CONTROL) {
				zscale	*= mag;
				//zoom	= pow(zoom, m);
			} else {
				Point	size = GetClientRect().Size();
				view_loc *= translate(float3{float(mouse.x) / size.x * 2 - 1, float(mouse.y) / size.y * 2 - 1, 1} * float(x * move_scale / 1024));
			}
			break;
		default:
			if (buttons & MK_CONTROL)
				zoom	*= mag;
			else
				view_loc *= translate(float3{zero, zero, x * move_scale / 1024});
			break;
	}
	Invalidate();
}

void MeshWindow::ResetView() {
	target_loc.reset();
	if (mode == PERSPECTIVE)
		target_loc.trans4.z = move_scale;
	Timer::Start(0.01f);
}

int MeshWindow::ChunkOffset(int i) const {
#if 0
	if (topology.chunks)
		return i % topology.chunks;
#endif
	if (patch_starts.size()) {
		auto j = lower_boundc(patch_starts, i);
		if (j != patch_starts.end())
			return i - *j;
	}
	return i;
}

int	MeshWindow::PrimFromVertex(int i)	const {
	if (items.empty())
		return 0;
	return ((MeshInstance*)items.front().get())->PrimFromVertex(i, false);
}

void MeshWindow::Select(const Point &mouse) {
	point		size		= Size();
	float		sx			= float(size.x) / 2, sy = float(size.y) / 2, sz = max(sx, sy);
	float4x4	mat			= translate(sx - mouse.x, sy - mouse.y, 0) * scale(sx, sy, one) * GetMatrix();

	for (auto& i : items) {
		int v = i->Select(this, mat);
		if (v >= 0) {
			MeshNotification::Set(*this, v).Send(Parent());
			break;
		}
	}
}

void MeshWindow::SetSelection(int i, bool zoom) {
	select = i;
	for (auto& x : items) {
		x->Select(this, i);
	}
}

void MeshWindow::AddDrawable(Item *p) {
	items.push_back(p);
	extent |= p->extent;
	if (mode == PERSPECTIVE) {
		float size	= len(extent.extent());
		move_scale	= size;
		view_loc.trans4 = position3(zero, zero, size);
	}
}

void MeshWindow::Paint() {
	static pass *blend4			= *ISO::root("data")["default"]["blend4"][0];
	static pass *specular		= *ISO::root("data")["default"]["specular"][0];
	static pass *thickline		= *ISO::root("data")["default"]["thicklineR"][0];

	axes.tech = specular;

#if 0
	Texture	dt2;
	if (dt)
		dt2 = MipDepth(dt);
#endif

	DeviceContextPaint	dc(*this);
	GraphicsContext		ctx;
	graphics.BeginScene(ctx);

	ctx.SetRenderTarget(GetDispSurface());
	ctx.Clear(colour(0,0,0));
	ctx.SetBlend(BLENDOP_ADD, BLEND_ONE, BLEND_INV_SRC_ALPHA);

	float4x4	world, proj0;
	GetMatrices(&world, &proj0);

	float4x4	proj			= hardware_fix(proj0);
	float3x4	view			= view_loc;
	float4x4	viewProj		= proj * view;
	float4x4	worldviewproj	= viewProj * world;

	float3x4	iview			= inverse(view);
	float3		shadowlight_dir	= light_rot * float3{0, 0, -1};
	colour		shadowlight_col(0.75f);
	colour		ambient(float3(0.25f));
	colour		tint(one);
	float2		point_size		= float2{8.f / width, 8.f / height};

	ShaderVal	vals[] = {
		{"projection",			&proj},
		{"worldViewProj",		&worldviewproj},
		{"iview",				&iview},
		{"world",				&world},
		{"view",				&view},
		{"viewProj",			&viewProj},
		{"tint",				&tint},
		{"shadowlight_dir",		&shadowlight_dir},
		{"shadowlight_col",		&shadowlight_col},
		{"light_ambient",		&ambient},
		{"point_size",			&point_size},
	};
	ShaderVals	shader_params(vals);
	auto		shader_browser = ISO::MakeBrowser(shader_params);

#if 0
	if (dt) {
		tint				= one;
		AddShaderParameter("diffuse_samp", dt);
		Set(ctx, tex_blend, shader_browser);
		ImmediateStream<vertex_tex>	ims(ctx, PRIM_QUADLIST, 4);
		vertex_tex	*p	= ims.begin();
		p[0].pos.set(+1, -1, 1); p[0].uv.set(1, 0);
		p[1].pos.set(-1, -1, 1); p[1].uv.set(0, 0);
		p[2].pos.set(-1, +1, 1); p[2].uv.set(0, 1);
		p[3].pos.set(+1, +1, 1); p[3].uv.set(1, 1);
	} else if (ct) {
		tint				= one;
		AddShaderParameter("diffuse_samp", ct);
		Set(ctx, tex_blend, shader_browser);
		ImmediateStream<vertex_tex>	ims(ctx, PRIM_QUADLIST, 4);
		vertex_tex	*p	= ims.begin();
		p[0].pos.set(+1, -1, 1); p[0].uv.set(1, 0);
		p[1].pos.set(-1, -1, 1); p[1].uv.set(0, 0);
		p[2].pos.set(-1, +1, 1); p[2].uv.set(0, 1);
		p[3].pos.set(+1, +1, 1); p[3].uv.set(1, 1);
	}
#elif 1
	if (dt) {
		AddShaderParameter("linear_samp", dt);
		AddShaderParameter("diffuse_samp", ct ? ct : dt);
		static pass	*depth_ray	= *ISO::root("data")["default"]["depth_ray"][0];
		Set(ctx, depth_ray, shader_browser);
		ImmediateStream<float3p>	ims(ctx, PRIM_QUADLIST, 4);
		float3p	*p	= ims.begin();
		p[0] = float3{+1, -1, 1};
		p[1] = float3{-1, -1, 1};
		p[2] = float3{-1, +1, 1};
		p[3] = float3{+1, +1, 1};
	}
#endif

	if (!ct && flags.test(FRUSTUM_PLANES)) {
		tint	= colour(0.25f, 0.25f, 0.25f);
		Set(ctx, blend4, shader_browser);
		ImmediateStream<float3p>	ims(ctx, PRIM_QUADLIST, 4);
		float3p	*p	= ims.begin();
		p[0] = float3{+1, -1, 1};
		p[1] = float3{-1, -1, 1};
		p[2] = float3{-1, +1, 1};
		p[3] = float3{+1, +1, 1};
	}

	for (auto &i : items)
		i->Draw(this, ctx);

	ctx.SetBlendEnable(false);
	ctx.SetBackFaceCull(BFC_NONE);
	ctx.SetFillMode(FILL_SOLID);
	ctx.SetDepthTestEnable(false);

	if (flags.test(FRUSTUM_EDGES)) {
		worldviewproj = viewProj * world * (translate(zero, zero, (one + viewport.y.z) * half) * scale(one, one, viewport.x.z * half));
		tint = colour(0.5f, 0.5f, 0.5f);
		Set(ctx, blend4, shader_browser);
		DrawWireFrameBox(ctx);
	}

	if (flags.test(SCISSOR)) {
		worldviewproj	= viewProj * world * cuboid((scissor - viewport.y) / scale(viewport.x)).matrix();
		tint = colour(1,1,0);
		Set(ctx, blend4, shader_browser);
		DrawWireFrameBox(ctx);
	}

	if (flags.test(SCREEN_RECT)) {
		rectangle	r = rectangle::with_length(position2(zero), to<float>(screen_size));
		r = (r - viewport.y.xy) / scale(viewport.x.xy);
		worldviewproj	= viewProj * world * translate(0,0,1) * float3x4(r.matrix());
		tint = colour(1,1,1);
		Set(ctx, blend4, shader_browser);
		DrawWireFrameRect(ctx);
	}

	if (flags.test(BOUNDING_EDGES)) {
		ctx.SetDepthTestEnable(true);
		ctx.SetBlend(BLENDOP_ADD, BLEND_ONE, BLEND_INV_SRC_ALPHA);
		ctx.SetBlendEnable(true);

		worldviewproj	= viewProj * world * extent.matrix();
		tint			= colour(0,1,0);
		Set(ctx, thickline, shader_browser);
		//Set(ctx, blend4, shader_browser);
		DrawWireFrameBox(ctx);
	}

	ctx.SetWindow(axes.window(GetClientRect()));
	proj			= hardware_fix(axes.proj());
	viewProj		= proj * view_loc.rot;
	iview			= (float3x4)float3x3((quaternion)inverse(view_loc.rot));
	shadowlight_dir	= inverse(view_loc.rot) * float3{0,0,-1};

	axes.draw(ctx, mode == PERSPECTIVE ? float3x3(world) : identity, shader_browser);

	graphics.EndScene(ctx);
	Present();
}

//-----------------------------------------------------------------------------
// MeshInstance
//-----------------------------------------------------------------------------

void MeshInstance::DrawMesh(GraphicsContext &ctx, int v, int n) {
	if (topology && v < num_verts) {
		PrimType	prim = GetHWType(topology, n);

		if (ib)
			ctx.DrawIndexedPrimitive(prim, 0, num_verts, v, n);
		else
			ctx.DrawPrimitive(prim, v, n);
	}
}

void SetThickPoint(GraphicsContext &ctx, float size, const ISO::Browser &params) {
#ifdef USE_DX11
	static pass *thickpoint	= *ISO::root("data")["default"]["thickpoint4"][0];
	params["point_size"].UnsafeSet(addr(size / to<float>(ctx.GetWindow().extent())));
	Set(ctx, thickpoint, params);
#else
	ctx.Device()->SetRenderState(D3DRS_POINTSIZE, iorf(8.f).i);
#endif
}
/*
mw->GetMatrices(&world, &proj0);
mw->view_loc
mw->cull
mw->light_rot
mw->Size()
mw->flags
mw->mode
mw->select
*/

void MeshInstance::Draw(MeshWindow *mw, GraphicsContext &ctx) {
	static pass	*blend_idx		= *ISO::root("data")["default"]["blend_idx"][0];
	static pass *blend4			= *ISO::root("data")["default"]["blend4"][0];
	static pass *specular4		= *ISO::root("data")["default"]["specular4"][0];

	float4x4	world, proj0;
	mw->GetMatrices(&world, &proj0);

	world	= world * transform;

	float4x4	proj			= hardware_fix(proj0);
	float3x4	view			= mw->view_loc;
	float4x4	viewProj		= proj * view;
	float4x4	worldviewproj	= viewProj * world;

	float3x4	iview			= inverse(view);
	bool		flip			= proj.det() < 0;
	BackFaceCull cull2			= mw->cull == BFC_NONE ? BFC_NONE : reverse(mw->cull, flip);
	float		flip_normals	= cull2 == BFC_FRONT ? -1 : 1;
	float3		shadowlight_dir	= mw->light_rot * float3{0, 0, -1};
	colour		shadowlight_col(0.75f);
	colour		ambient(float3(0.25f));
	colour		tint(one);
	colour		clip_tint(one);
	colour		col(one);
	float2		point_size		= 8.f / to<float>(mw->Size());

	ShaderVal	vals[] = {
		{"projection",			&proj},
		{"worldViewProj",		&worldviewproj},
		{"matrices",			&matrices},
		{"diffuse_colour",		&col},
		{"iview",				&iview},
		{"world",				&world},
		{"view",				&view},
		{"viewProj",			&viewProj},
		{"flip_normals",		&flip_normals},
		{"tint",				&tint},
		{"clip_tint",			&clip_tint},
		{"shadowlight_dir",		&shadowlight_dir},
		{"shadowlight_col",		&shadowlight_col},
		{"light_ambient",		&ambient},
		{"point_size",			&point_size},
		{"flags",				&mw->flags},
	};
	ShaderVals	shader_params(vals);
	auto		shader_browser = ISO::MakeBrowser(shader_params);

	tint		= colour(1,0.95f,0.8f);
	clip_tint	= mw->mode == mw->PERSPECTIVE ? tint : colour(1, 0, 0, 1);

	ctx.SetVertices(0, vb);

	if (topology.type == Topology::POINTLIST || topology.type == Topology::PATCH) {
		ctx.SetBlendEnable(true);
		SetThickPoint(ctx, 4, shader_browser);
		ctx.SetVertexType<float4p>();

	} else if (vb_matrix) {
		Set(ctx, blend_idx, shader_browser);
		ctx.SetVertices(1, vb_matrix);
		ctx.SetVertexType<vertex_idx>();
	
	} else if (vb_norm) {
		Set(ctx, specular4, shader_browser);
		ctx.SetVertices(1, vb_norm);
		ctx.SetVertexType<vertex_norm>();

	} else {
		Set(ctx, blend4, shader_browser);
		ctx.SetVertexType<float4p>();
	}

	if (ib)
		ctx.SetIndices(ib);

	if (mw->flags.test(mw->FILL)) {
		ctx.SetBackFaceCull(cull2);
		ctx.SetFillMode(FILL_SOLID);
		ctx.SetZBuffer(Surface(TEXF_D24S8, mw->Size(), MEM_DEPTH));
		//			ctx.SetZBuffer(Surface(TEXF_D32F, Size()));

		ctx.SetDepthTest(DT_USUAL);
		ctx.SetDepthTestEnable(true);
		ctx.ClearZ();

		DrawMesh(ctx);
		if (mw->cull != BFC_NONE) {
			ctx.SetBackFaceCull(reverse(mw->cull, !flip));
			ctx.SetFillMode(FILL_WIREFRAME);
			DrawMesh(ctx);
		}

	} else {
		//ctx.SetBackFaceCull(BFC_NONE);
		//Set(ctx, specular4, shader_browser);
		ctx.SetBackFaceCull(cull2);
		ctx.SetFillMode(FILL_WIREFRAME);
		DrawMesh(ctx);
	}

	if (mw->select >= 0) {
		if (mw->patch_starts.size()) {
			tint = colour(0, 1, 0);

			Set(ctx, vb_matrix ? blend_idx : blend4, shader_browser);
			ctx.SetBackFaceCull(BFC_NONE);
			ctx.SetDepthTestEnable(false);
			ctx.SetFillMode(FILL_WIREFRAME);

			//auto		j		= mw->ChunkOffset(mw->select);
			//int			end		= j;
			//int			start	= mw->select - j;

			auto		j		= lower_boundc(mw->patch_starts, mw->select);
			int			end		= j[0];
			int			start	= j == mw->patch_starts.begin() ? 0 : j[-1];
			int			n		= 1;
			PrimType	prim	= GetHWType(topology, n);
			if (ib)
				ctx.DrawIndexedVertices(prim, 0, num_verts, start, end - start);
			else
				ctx.DrawVertices(prim, start, end - start);
		}

		int	prim	= PrimFromVertex(mw->select, false);
		int	off		= topology.ToHWOffset(max(mw->select - VertexFromPrim(prim, false), 0));
		int	vert	= VertexFromPrim(prim * topology.hw_mul, true);

		tint = colour(1,0.5f,0.5f);

		switch (topology.type) {
			case Topology::POINTLIST:
				break;

			case Topology::PATCH: {
			#if 0
				int			cp		= topology.hw_mul;
				PrimType	prim	= PatchPrim(cp);
				static technique *convex_hull	= ISO::root("data")["default"]["convex_hull"];
				Set(ctx, (*convex_hull)[cp - 3], shader_browser);
				try {
					if (flags.test(FILL)) {
						ctx.SetFillMode(FILL_SOLID);
						ctx.SetBackFaceCull(cull2);
						if (ib)
							ctx.DrawIndexedVertices(prim, 0, num_verts, vert, cp);
						else
							ctx.DrawVertices(prim, vert, cp);
					}
				} catch (...) {}

				try {
					ctx.SetBackFaceCull(BFC_NONE);
					ctx.SetDepthTestEnable(false);
					ctx.SetFillMode(FILL_WIREFRAME);
					if (ib)
						ctx.DrawIndexedVertices(prim, 0, num_verts, vert, cp);
					else
						ctx.DrawVertices(prim, vert, cp);
				} catch (...) {}

				tint = colour(1,0.5f,0.5f);
			#endif
			}
			// fall through
			default: {
				if (topology.type == Topology::POINTLIST || topology.type == Topology::PATCH)
					SetThickPoint(ctx, 8, shader_browser);
				else
					Set(ctx, vb_matrix ? blend_idx : blend4, shader_browser);

				if (mw->flags.test(mw->FILL)) {
					ctx.SetFillMode(FILL_SOLID);
					ctx.SetBackFaceCull(cull2);
					DrawMesh(ctx, vert, 1);
				}

				ctx.SetBackFaceCull(BFC_NONE);
				ctx.SetDepthTestEnable(false);
				ctx.SetFillMode(FILL_WIREFRAME);
				DrawMesh(ctx, vert, 1);
				break;
			}
		}

		if (vert < num_verts) {
			tint = colour(1,0,0);
			if (topology.type == Topology::POINTLIST || topology.type == Topology::PATCH)
				SetThickPoint(ctx, 8, shader_browser);
			else
				Set(ctx, vb_matrix ? blend_idx : blend4, shader_browser);

			SetThickPoint(ctx, 8, shader_browser);
			ctx.SetBlendEnable(true);
			ctx.SetFillMode(FILL_SOLID);
			ctx.DrawPrimitive(PRIM_POINTLIST, ib ? ib.Data()[vert + off] : vert + off, 1);
		}
	}
}

int MeshInstance::Select(MeshWindow *mw, const float4x4 &mat) {
	int			hw_prims	= num_prims * topology.hw_mul;
	int			face		= -1;
	int			vert		= ib
		? FindVertPrim(oct, mat, make_prim_iterator(topology.hw, make_indexed_iterator(vb.Data().begin(), ib.Data().begin())), hw_prims, face)
		: FindVertPrim(oct, mat, make_prim_iterator(topology.hw, vb.Data().begin()), hw_prims, face);

	if (vert >= 0) {
		vert += topology.hw.first_vert(face % topology.hw_mul);
		face /= topology.hw_mul;
		vert = topology.first_vert(face) + topology.FromHWOffset(vert);

		//deindex?
		//if (ib)
		//	vert = ib.Data()[vert];

		MeshNotification::Set(*mw, vert).Send(mw->Parent());
	}
	return vert;
}

void MeshInstance::Select(MeshWindow *mw, int i) {
	position3	pos[64];
	if (ib) {
		copy(make_prim_iterator(topology.hw, make_indexed_iterator(vb.Data().begin(), ib.Data().begin()))[i], pos);
	} else {
		copy(make_prim_iterator(topology.hw, vb.Data().begin())[i], pos);
	}

	float3		normal	= GetNormal(pos);
	position3	centre	= centroid(pos);
	float4x4	world, proj;
	mw->GetMatrices(&world, &proj);

	if (reverse(mw->cull, proj.det() < 0) == BFC_FRONT)
		normal = -normal;

	centre	= world * centre;
	float	dist		= sqrt(len(normal));

	float4	min_pos		= float4{0,0,-1,1} / proj;
	float	min_dist	= len(project(min_pos));

	dist	= max(dist, min_dist);

	auto	rot	= quaternion::between(normalise(normal), float3{0,0,-1});
	mw->SetTarget({rot, position3(0, 0, dist) - rot * float3(centre)});
}

MeshInstance::MeshInstance(const Topology2 topology, const TypedBuffer &verts, const indices &ix, bool use_w) : topology(topology), num_verts(ix.num), num_prims(topology.verts_to_prims(num_verts)) {
	int			num_hw		= topology.NumHWVertices(num_verts);
	uint32		num_recs	= ix.max_index() + 1;

	if (num_hw && num_recs <= verts.size()) {
		dynamic_array<float3>	prim_norms(num_prims);

		if (ix) {
			float4p	*vec	= vb.Begin(num_recs);
			float4p	*v0		= vec;

			for (auto &&i : make_range_n(verts.begin(), num_recs)) {
				assign(*vec, i);
				if (!use_w)
					vec->w	= 1;
				vec++;
			}

			uint32	*i0			= ib.Begin(num_hw);
			uint32	*idx		= i0;
			dynamic_bitarray<uint32>	used(num_recs);
			for (int i = 0; i < num_verts; i++) {
				uint32	j	= min(ix[i], num_recs - 1);
				used[j]		= true;
				*idx		= j;
				idx			= topology.Adjust(idx, i);
			}

			for (int x = 0; (x = used.next(x, true)) < used.size(); x++) {
				if (v0[x].w)
					extent |= project(float4(v0[x]));
			}

			if (topology.type >= Topology::TRILIST) {
				auto	v		= make_indexed_iterator(v0, make_const(i0));
				GetFaceNormals(prim_norms,
					make_range_n(make_prim_iterator(topology.hw, v), num_prims)
				);

				int		ix0		= used.lowest(true);
				int		ix1		= used.highest(true);
				dynamic_array<NormalRecord>	vert_norms(ix1 - ix0 + 1);
				AddNormals(vert_norms - ix0, prim_norms,
					make_range_n(make_prim_iterator(topology.hw, i0), num_prims)
				);

				float3p	*norm	= vb_norm.Begin(num_recs);
				for (int i = 0; i < ix1 - ix0 + 1; i++) {
					vert_norms[i].Normalise();
					norm[i + ix0] = vert_norms[i].Get(1);
				}
				//c->vb_norm.End();
			}

			//c->ib.End();
			//c->vb.End();

		} else {
			float4p	*vec	= vb.Begin(num_hw);
			float4p	*v0		= vec;
			int		x		= 0;
			for (auto &&i : make_range_n(verts.begin(), num_recs)) {
				assign(*vec, i);
				if (!use_w)
					vec->w	= 1;
				extent |= project(float4(*vec));
				vec = topology.Adjust(vec, x++);
			}

			if (topology.type >= Topology::TRILIST) {
				GetFaceNormals(prim_norms,
					make_range_n(make_prim_iterator(topology.hw, v0), num_prims)
				);
				float3p	*norm	= vb_norm.Begin(num_hw);
				for (int i = 0; i < num_hw; i++)
					norm[i] = prim_norms[topology.FirstPrimFromVertex(i, false) / topology.hw_mul];
				//c->vb_norm.End();
			}
			//c->vb.End();
		}
	}
}


MeshWindow *app::MakeMeshView(const WindowPos &wpos, Topology2 topology, const TypedBuffer &vb, const indices &ix, param(float3x2) viewport, BackFaceCull cull, MeshWindow::MODE mode) {
	graphics.Init();

	auto	mesh	= new MeshInstance(topology, vb, ix, mode != MeshWindow::PERSPECTIVE);
	auto	mw		= new MeshWindow(viewport, cull, mode);

	mw->AddDrawable(mesh);
	mw->Create(wpos, "Mesh", Control::CHILD | Control::CLIPCHILDREN | Control::CLIPSIBLINGS, Control::NOEX);
	return mw;
}



void	MeshAABBInstance::Draw(MeshWindow *mw, GraphicsContext &ctx) {
	static pass *blend4			= *ISO::root("data")["default"]["blend4"][0];

	float4x4	world, proj0;
	mw->GetMatrices(&world, &proj0);

	float4x4	proj			= hardware_fix(proj0);
	float3x4	view			= mw->view_loc;
	float4x4	viewProj		= proj * view;
	float4x4	worldviewproj	= viewProj * world;
	colour		tint			= colour(0.5f, 0.5f, 0.5f);

	ShaderVal	vals[] = {
		{"worldViewProj",		&worldviewproj},
		{"tint",				&tint},
	};
	ShaderVals	shader_params(vals);
	auto		shader_browser = ISO::MakeBrowser(shader_params);

	for (auto& i : boxes) {
		worldviewproj	= viewProj * world * i.matrix();
		Set(ctx, blend4, shader_browser);
		DrawWireFrameBox(ctx);
	}
}

int		MeshAABBInstance::Select(MeshWindow *mw, const float4x4 &mat) {
	return -1;
}
void	MeshAABBInstance::Select(MeshWindow *mw, int i) {
}

MeshAABBInstance::MeshAABBInstance(const TypedBuffer &b) {
	for (auto i : b) {
		auto&	box = *(cuboid*)&i.t;
		boxes.push_back(box);
		extent	|= box;
	}
}

//-----------------------------------------------------------------------------
//	Compute Grid
//-----------------------------------------------------------------------------

const GUID CLSID_GridEffect		= {0xB7B36C92, 0x3498, 0x4A94, {0x9E, 0x95, 0x9F, 0x24, 0x6F, 0x92, 0x45, 0xC1}};
const GUID GUID_GridPixelShader	= {0xB7B36C92, 0x3498, 0x4A94, {0x9E, 0x95, 0x9F, 0x24, 0x6F, 0x92, 0x45, 0xC2}};

struct GridEffectConstants {
	float2x3		itransform;
	uint32			dummy[2];
	d2d::vector4	sizes;
	d2d::vector4	colour1;
	d2d::vector4	colour2;
	int				mode;
};

class GridEffect : public com_inherit<type_list<ID2D1Transform>, com<ID2D1DrawTransform, d2d::CustomEffect>>, public GridEffectConstants {
	com_ptr2<ID2D1DrawInfo> draw_info;
	d2d::matrix		transform;

public:
	enum {
		PARAM_transform,
		PARAM_sizes,
		PARAM_colour1,
		PARAM_colour2,
		PARAM_mode,
	};
	enum MODE {
		MODE_XOR,
		MODE_AND,
	};

	GridEffect() : transform(float2x3(identity)) {
		sizes	= d2d::vector4(1, 1, 1, 1);
		colour1	= d2d::vector4(0,0,0,1);
		colour2 = d2d::vector4(1,1,1,1);
		mode	= MODE_XOR;
	}

	static HRESULT Register(ID2D1Factory1 *factory) {
		return d2d::CustomEffect::Register<GridEffect>(factory, CLSID_GridEffect,
			"GridEffect", "Isopod", "Sample", "Renders a grid",
			meta::make_tuple(),//"Dummy",	//inputs
			EFFECT_FIELD(GridEffect, transform),
			EFFECT_FIELD(GridEffect, sizes),
			EFFECT_FIELD(GridEffect, colour1),
			EFFECT_FIELD(GridEffect, colour2),
			EFFECT_FIELD(GridEffect, mode)
		);
	}

// ID2D1EffectImpl
	STDMETHOD(Initialize)(ID2D1EffectContext* context, ID2D1TransformGraph* graph) {
		HRESULT	hr;
//		context->GetDpi(&constants.dpi.x, &constants.dpi.y);
#ifdef PLAT_WIN32
        Resource	data("d2d_grid", "SHADER");
		hr = context->LoadPixelShader(GUID_GridPixelShader, data, data.size32());
#endif
        hr = graph->SetSingleTransformNode(this);
		return hr;
	}

	STDMETHOD(PrepareForRender)(D2D1_CHANGE_TYPE change_type) {
		itransform	= inverse(float2x3(transform));
		return draw_info->SetPixelShaderConstantBuffer((BYTE*)(GridEffectConstants*)this, sizeof(GridEffectConstants));
	}

// ID2D1DrawTransform
	STDMETHOD(SetDrawInfo)(ID2D1DrawInfo *_draw_info) {
		HRESULT	hr = S_OK;
		draw_info = _draw_info;
		hr = draw_info->SetPixelShader(GUID_GridPixelShader);
		return hr;
	}

// ID2D1Transform
	STDMETHOD(MapOutputRectToInputRects)(const D2D1_RECT_L *out, D2D1_RECT_L *in, UINT32 in_count) const {
		clear(in[0]);
		return S_OK;
	}

	STDMETHOD(MapInputRectsToOutputRect)(const D2D1_RECT_L *in, const D2D1_RECT_L *opaque, UINT32 in_count, D2D1_RECT_L *out, D2D1_RECT_L *opaque_subrect) {
		out[0].left    = LONG_MIN;
		out[0].top     = LONG_MIN;
		out[0].right   = LONG_MAX;
		out[0].bottom  = LONG_MAX;

		clear(*opaque_subrect);
		return S_OK;
	}

	STDMETHOD(MapInvalidRect)(UINT32 input_index, D2D1_RECT_L invalid_in, D2D1_RECT_L *invalid_out) const {
		// Indicate that the entire output may be invalid.
		invalid_out->left    = LONG_MIN;
		invalid_out->top     = LONG_MIN;
		invalid_out->right   = LONG_MAX;
		invalid_out->bottom  = LONG_MAX;
		return S_OK;
	}

	// ID2D1TransformNode
	STDMETHOD_(UINT32, GetInputCount)() const {
		return 0;
	}
};

d2d::Bitmap MakeChecker(d2d::Target &target, int w, int h) {
	auto	g = make_auto_block<d2d::texel>(w + 1, h + 1);
	fill(g, d2d::texel(0, 0, 0, 0xff));
	for (auto &&y : g)
		y[w].a = 0;
	for (auto &x : g[h])
		x.a = 0;
	return d2d::Bitmap(target, g);
}

string_accum& ComputeGrid::GetSelectedText(string_accum &&a, uint32x3 group, uint32x3 dim) const {
	return a << "thread: " << group << ", group: " << ifelse(dim_swap, dim.yxz, dim);
}

LRESULT ComputeGrid::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE:
			tooltip.Create(*this, NULL, POPUP);// | TTS_NOPREFIX | TTS_ALWAYSTIP);
			tooltip.Add(*this);
			tip_on	= false;
			break;

		case WM_MOUSEWHEEL: {
			d2d::point	pt	= ToClient(Point(lParam));
			float	mult	= iso::pow(1.05f, (short)HIWORD(wParam) / 64.f);
			zoom	*= mult;
			pos		= pt + (pos - pt) * mult;
			Invalidate();
			return 0;
		}

		case WM_RBUTTONDOWN:
			if (~selected) {
				NMITEMACTIVATE	nm;
				nm.iItem	= selected;
				if (dim_swap) {
					auto	group_loc	= split_index(selected, group.xy);
					auto	selected2	= flat_index(group_loc.yxz, group.yx);
					nm.iItem = selected2;
				}
				return SendNotification(nm.hdr, *this, NM_RCLICK);
			}
			return 0;

		case WM_MOUSEMOVE: {
			Point	mouse	= Point(lParam);

			bool	is_sel = ~selected;
			if (is_sel != tip_on) {
				tooltip.Activate(*this, tip_on = is_sel);
				if (is_sel)
					TrackMouse(TME_LEAVE);
			}

			if (is_sel)
				tooltip.Track();

			if (wParam & MK_LBUTTON) {
				pos	+= mouse - prevmouse;
				Invalidate();
			} else {
				auto	prev = exchange(selected, ~0u);
				uint3p	dim_loc, group_loc;
				if (GetLoc(ClientToWorld(mouse), dim_loc, group_loc))
					selected = flat_index(group_loc, group.xy) + flat_index(dim_loc, dim.xy) * reduce_mul(group);
				//selected = flat_index(uint32x3(group_loc), uint32x3(group).xy) + flat_index(uint32x3(dim_loc), uint32x3(dim).xy) * (group.x * group.y * group.z);
				
				if (prev != selected)
					Invalidate();
			}
			prevmouse	= mouse;
			return 0;
		}

		case WM_MOUSELEAVE:
			tooltip.Activate(*this, tip_on = false);
			break;

		case WM_PAINT:
			if (DeviceContextPaint	dc = DeviceContextPaint(*this))
				Paint();
			return 0;

		case WM_NOTIFY: {
			NMHDR	*nmh = (NMHDR*)lParam;
			switch (nmh->code) {
				case TTN_GETDISPINFOA: {
					NMTTDISPINFOA	*nmtdi	= (NMTTDISPINFOA*)nmh;
					uint32		group_size		= group.x * group.y * group.z;
					GetSelectedText(fixed_accum(nmtdi->szText), split_index(selected, uint32x3(group)).xyz, split_index(selected / group_size, uint32x3(dim)).xyz);
					return 0;
				}
			}
			break;
		}
		case WM_NCDESTROY:
			delete this;
			return 0;

	}
	return d2d::Window::Proc(message, wParam, lParam);
}

ComputeGrid::ComputeGrid(const WindowPos &wpos, const char *title, ID id, const uint3p &_dim, const uint3p &group) : font(write, L"Arial", 0.75f), dim(_dim), group(group) {
	ISO_ASSERT(reduce_min(dim) > 0 && reduce_min(group) > 0);
	GridEffect::Register(factory);

	//font->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);

	dim_swap = group.y < group.x && dim.x > dim.y;
	if (dim_swap)
		dim = dim.yxz;

	int		slack		= 1;
	int		width1		= dim.x * (group.x + slack) + slack;
	int		height1		= dim.y * (group.y + slack);
	int		width		= dim.z * (width1 + slack * 2) - slack;

	auto	size		= wpos.rect.Size();
	if (size.x * height1 < size.y * width) {
		zoom	= float(size.x) / width;
		pos.y	= (size.y - zoom * height1) / 2;
	} else {
		zoom	= float(size.y) / height1;
		pos.x	= (size.x - zoom * width) / 2;
	}

	Create(wpos, title, CHILD | CLIPSIBLINGS | VISIBLE, NOEX, id);
	Rebind(this);
	Invalidate();

}

void ComputeGrid::Paint() const {
//	UsePixels();
	SetTransform(identity);

	d2d::SolidBrush	black(*this, colour(0, 0, 0, 1));
	d2d::SolidBrush	white(*this, colour(1, 1, 1, 1));
	d2d::SolidBrush	white2(*this, colour(0.9f, 0.9f, 0.9f, 1));
	d2d::rect		client = GetClientRect();

	Fill(client, black);

	float2x3	transform = GetTransform();
	SetTransform(transform);

	d2d::rect	visible_rect(
		ClientToWorld(d2d::point(0, 0)),
		ClientToWorld(client.Size()) + d2d::point(1, 1)
	);

	int		slack		= 1;
	int		width1		= dim.x * (group.x + slack) + slack;
	int		height1		= dim.y * (group.y + slack) + slack;
	float	line_width	= max(1 / 64.f, 1 / zoom);

	uint32		group_size		= group.x * group.y * group.z;
	uint32x3	selected_group	= split_index(selected, uint32x3(group)).xyz;
	uint32x3	selected_dim	= split_index(selected / group_size, uint32x3(dim)).xyz;

	for (int z = 0; z < dim.z; z++) {
		d2d::rect	zrect = d2d::rect::with_ext(z * (width1 + slack * 2), 0, width1 + slack, height1 + slack);
		if (!zrect.Overlaps(visible_rect))
			continue;

		Fill(zrect, z & 1 ? white2 : white);
#if 1
		d2d::rect	ext	= d2d::TextLayout(write, to_string(z), font, 1000).GetExtent();
		d2d::point	s	= ext.Size() * 1.01f;//max(ext.Width(), ext.Height()) * 1.01f;
		SetTransform(transform * translate(zrect.TopLeft()) * scale(as_vec(zrect.Size()) / as_vec(s)));
		font->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
		DrawText(d2d::rect(d2d::point(0, 0), s), to_string(z), font, d2d::SolidBrush(*this, colour(0, 0, 0, 0.25f)));
#else
		SetTransform(transform * translate(zrect.TopLeft()) * scale(zrect.Size()));
		font->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
		DrawText(d2d::rect(0, 0, 1, 1), to_string(z), font, d2d::SolidBrush(*this, colour(0, 0, 0, 0.25f)));
		SetTransform(transform);
#endif
		int	minx = max(ceil((visible_rect.left - (zrect.left + slack + group.x + 1)) / (group.x + slack)), 0);
		int	miny = max(ceil((visible_rect.top - (zrect.top + slack + group.y + 1)) / (group.y + slack)), 0);
		int	maxx = min((visible_rect.right - (zrect.left + slack)) / (group.x + slack) + 1, dim.x);
		int	maxy = min((visible_rect.bottom - (zrect.top + slack)) / (group.y + slack) + 1, dim.y);

		if (zoom < 4) {
#if 1
			auto	transform2	= transform * translate(zrect.TopLeft() + slack * 2);
			float	a			= min(2 / zoom, 1);
			SetTransform(identity);
			DrawImage(
				transform2 * d2d::point(minx, miny),
				d2d::Effect(*this, CLSID_GridEffect)
					.SetValue(GridEffect::PARAM_transform,	d2d::matrix(transform2))
					.SetValue(GridEffect::PARAM_sizes,		d2d::vector4(group.x,group.y,1,1))
					.SetValue(GridEffect::PARAM_colour1,	d2d::vector4(0, 0, 0, 0))
					.SetValue(GridEffect::PARAM_colour2,	d2d::vector4(0, 0, 0, a))
					.SetValue(GridEffect::PARAM_mode,		GridEffect::MODE_AND),
				(transform2 * d2d::rect(minx, miny, maxx * (group.x + slack) - slack, maxy * (group.y + slack) - slack))
			);
			SetTransform(transform);

#else
			d2d::SolidBrush	black(*this, colour(0, 0, 0, min(2 / zoom, 1)));

			for (int y = miny; y < maxy; y++) {
				for (int x = minx; x < maxx; x++) {
					d2d::rect	rect = zrect.Subbox(
						slack + x * (group.x + slack),
						slack + y * (group.y + slack),
						group.x, group.y
					);
					Fill(rect, black);
				}
			}
#endif
			if (~selected && z == selected_dim.z) {
				auto	rect = zrect.Subbox(
					slack * 2 + selected_dim.x * (group.x + slack),
					slack * 2 + selected_dim.y * (group.y + slack),
					group.x, group.y
				);
				Fill(rect, d2d::SolidBrush(*this, colour(0, 0, 1, .5f)));
				Fill(rect.Subbox(selected_group.x, selected_group.y, 1, 1), d2d::SolidBrush(*this, colour(0, 1, 0, 1)));
			}

		}
		else {
			SetTransform(transform);

			font->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
			for (int x = minx; x < maxx; x++) {
				int	dx = x * (group.x + slack) + slack * 2;
				if (miny == 0)
					DrawText(zrect.Subbox(dx, 1, 4, 1), to_string(x), font, black);
				if (maxy == dim.y)
					DrawText(zrect.Subbox(dx, dim.y * (group.y + slack) + slack, 4, 1), to_string(x), font, black);
			}

			font->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING);
			for (int y = miny; y < maxy; y++) {
				int	dy = y * (group.y + slack) + slack * 2;
				if (minx == 0)
					DrawText(zrect.Subbox(0, dy, 1.95f, 1), to_string(y), font, black);
				if (maxx == dim.x)
					DrawText(zrect.Subbox(dim.x * (group.x + slack) + slack, dy, 1.95f, 1), to_string(y), font, black);
			}

			if (~selected && z == selected_dim.z) {
				Fill(
					zrect.Subbox(
						slack * 2 + selected_dim.x * (group.x + slack),
						slack * 2 + selected_dim.y * (group.y + slack),
						group.x, group.y
					),
					d2d::SolidBrush(*this, colour(0, 0, 1, .5f))
				);
			}

			for (int z1 = 0; z1 < group.z; ++z1) {
				float	zf = (z1 + 1.f) / group.z;
				d2d::SolidBrush	line(*this, colour(0, 0, 0, zf));

				for (int y = miny; y < maxy; y++) {
					for (int x = minx; x < maxx; x++) {
						d2d::rect	rect = zrect.Subbox(
							slack * 2 + x * (group.x + slack) + slack - zf,
							slack * 2 + y * (group.y + slack) + slack - zf,
							group.x, group.y
						);

						for (int y1 = 0; y1 <= group.y; y1++)
							DrawLine(d2d::point(rect.left, rect.top + y1), d2d::point(rect.right, rect.top + y1), line, line_width);

						for (int x1 = 0; x1 <= group.x; x1++)
							DrawLine(d2d::point(rect.left + x1, rect.top), d2d::point(rect.left + x1, rect.bottom), line, line_width);
					}
				}

				if (~selected && z == selected_dim.z && z1 == selected_group.z) {
					d2d::rect	rect = zrect.Subbox(
						slack * 2 + selected_dim.x * (group.x + slack) + slack - zf + selected_group.x,
						slack * 2 + selected_dim.y * (group.y + slack) + slack - zf + selected_group.y,
						1, 1
					);
					Fill(rect, d2d::SolidBrush(*this, colour(0, 1, 0, zf)));

					buffer_accum16<256>	ba;
					ba << "thread:\n" << selected_group << "\ngroup:\n";
					if (dim_swap)
						ba << selected_dim.yxz;
					else
						ba << selected_dim;

					d2d::rect	ext	= d2d::TextLayout(write, ba, font, 1000).GetExtent();
					float		s	= max(ext.Width(), ext.Height()) * 1.01f;

					SetTransform(transform * scale(1 / s));
					font->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
					DrawText(rect * s, ba, font, black);
					SetTransform(transform);
				}
			}
		}
	}
}

bool ComputeGrid::GetLoc(position2 mouse, uint3p &dim_loc, uint3p &group_loc) const {
	int		slack		= 1;
	int		width1		= dim.x * (group.x + slack) + slack;
	int		height1		= dim.y * (group.y + slack) + slack;

	float	x		= mouse.v.x, y = mouse.v.y;

	int		dimz	= floor(x / (width1 + slack * 2));
	if (dimz < 0 || dimz >= dim.z)
		return false;

	x -= dimz * (width1 + slack * 2) + slack * 2;
	y -= slack * 2;

	int		dimx	= floor(x / (group.x + slack));
	if (dimx < 0 || dimx >= dim.x)
		return false;

	int		dimy	= floor(y / (group.y + slack));
	if (dimy < 0 || dimy >= dim.y)
		return false;

	dim_loc = {dimx, dimy, dimz};

	x -= dimx * (group.x + slack);
	y -= dimy * (group.y + slack);

	if (x < group.x && y < group.y) {
		group_loc.x = int(x);
		group_loc.y = int(y);
		group_loc.z = group.z - 1 - int(min(frac(x), frac(y)) * group.z);
		return true;

	} else if (group.z > 1) {
		float	f = max(x - group.x, y - group.y);
		x	-= f;
		y	-= f;
		if (x >= 0 && y > 0) {
			group_loc.x = int(x);
			group_loc.y = int(y);
			group_loc.z = group.z - 2 - int(f * group.z);
			return true;
		}
	}
	return false;

}

//-----------------------------------------------------------------------------
//	TimingWindow
//-----------------------------------------------------------------------------

//TimingWindow	*TimingWindow::me;

TimingWindow::TimingWindow(const WindowPos &wpos, Control _owner) {
	Create(wpos, "Timing", CHILD | CLIPSIBLINGS | VISIBLE, CLIENTEDGE, ID);
	owner	= _owner;
//	me		= this;
}

LRESULT TimingWindow::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE:
			tooltip.Create(*this, NULL, POPUP);// | TTS_NOPREFIX | TTS_ALWAYSTIP);
			tooltip.Add(*this);
			tip_on	= false;
			break;

		case WM_SIZE:
			if (!d2d.Resize(Point(lParam)))
				d2d.DeInit();
			Invalidate();
			break;

		case WM_MOUSEMOVE: {
			Point	mouse	= Point(lParam);
			if (!tip_on) {
				TrackMouse(TME_LEAVE);
				tooltip.Activate(*this, tip_on = true);
			}
			tooltip.Track();
			if (DragStrip().Contains(mouse))
				CURSOR_LINKBATCH.Set();
			else
				Cursor::LoadSystem(IDC_SIZEWE).Set();
			break;
		}

  		case WM_MOUSEWHEEL:
			if (GetRect().Contains(Point(lParam))) {
				if (wParam & MK_SHIFT) {
					yscale	*= iso::pow(1.05f, (short)HIWORD(wParam) / 64.f);
				} else {
					float	x	= float(ToClient(Point(lParam)).x) / GetClientRect().Width();
					float	t	= x * tscale + time;
					tscale		*= iso::pow(0.95f, (short)HIWORD(wParam) / 64.f);
					time		= t - x * tscale;
				}
				Invalidate(0, false);
			}
			break;

		case WM_MOUSELEAVE:
			tooltip.Activate(*this, tip_on = false);
			break;

		case WM_NCDESTROY:
			delete this;
		case WM_DESTROY:
			return 0;
	}
	return Super(message, wParam, lParam);
}

void TimingWindow::DrawGrid(d2d::Target &d2d, IDWriteTextFormat *font) {
	auto	size	= d2d.Size();

	{// horizontal grid lines
		float	lscale	= 1.75f - log10(size.y / yscale);
		float	scale	= pow(10.f, int(floor(lscale)));
		float	fade	= ceil(lscale) - lscale;

		d2d::SolidBrush	textbrush[2] = {
			{d2d,	colour(1,1,1, max(fade * 2 - 1, 0))},
			{d2d,	colour(1,1,1, 1)},
		};
		d2d::SolidBrush	linebrush[2] = {
			{d2d,	colour(1,1,1, fade * 0.75f)},
			{d2d,	colour(1,1,1, 0.75f)}
		};

		for (int i = 1, i1 = int(yscale / scale); i <= i1; ++i) {
			float	y		= i * scale;
			float	y2		= size.y - size.y * y / yscale;
			bool	mul10	= (abs(i) % 10) == 0;
			d2d.DrawLine(d2d::point(0, y2), d2d::point(size.x, y2), linebrush[mul10]);
			if (mul10 || fade > 0.5f)
				d2d.DrawText(d2d::rect(0, y2, size.x, y2 + 20), to_string(y), font, textbrush[mul10], D2D1_DRAW_TEXT_OPTIONS_CLIP);
		}
	}

	//scrub bar
	d2d.Fill(d2d::rect(0, 0, size.x, 12), d2d::SolidBrush(d2d, colour(0.5f)));

	{// vertical grid lines
		float	lscale	= 2 - log10(size.x / tscale);
		float	scale	= pow(10.f, int(floor(lscale)));
		float	fade	= ceil(lscale) - lscale;

		float	t0		= time, t1 = tscale + time;

		char	si_suffix[4];
		float	si_scale = scale / SIsuffix(max(abs(t0), abs(t1)), si_suffix);

		d2d::SolidBrush	textbrush[2] = {
			{d2d,	colour(1,1,1, max(fade * 2 - 1, 0))},
			{d2d,	colour(1,1,1, 1)},
		};
		d2d::SolidBrush	linebrush[2] = {
			{d2d,	colour(1,1,1, fade * 0.75f)},
			{d2d,	colour(1,1,1, 0.75f)}
		};

		for (int i = int(floor(t0 / scale)), i1 = int(t1 / scale); i <= i1; i++) {
			float	t		= i * scale;
			float	t2		= (t - time) * size.x / tscale;
			bool	mul10	= (abs(i) % 10) == 0;
			d2d.DrawLine(d2d::point(t2, 0), d2d::point(t2, size.y), linebrush[mul10]);
			if (mul10 || fade > 0.5f)
				d2d.DrawText(d2d::rect(t2, 0, t2 + size.x, 20), str(buffer_accum<256>() << i * si_scale << (char*)si_suffix << 's'), font, textbrush[mul10], D2D1_DRAW_TEXT_OPTIONS_CLIP);
		}
	}

}

void TimingWindow::DrawMarker(d2d::Target &d2d, const d2d::point &pos) {
	//marker
	if (!marker_geom) {
		com_ptr<ID2D1GeometrySink>	sink;
		d2d.CreatePath(&marker_geom, &sink);
		sink->BeginFigure(d2d::point(-10, 0), D2D1_FIGURE_BEGIN_FILLED);
		sink->AddLine(d2d::point(+10, 0));
		sink->AddLine(d2d::point(0, 10));
		sink->EndFigure(D2D1_FIGURE_END_CLOSED);
		sink->Close();
	}
	d2d.SetTransform(translate(pos));
	d2d.Fill(marker_geom, d2d::SolidBrush(d2d, colour(1,0,0)));
}
