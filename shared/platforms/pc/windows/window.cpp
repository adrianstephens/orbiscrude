#include "window.h"
//#define USE_GDI_SCALING

#ifndef USE_GDI_SCALING
#include "dib.h"
#endif

namespace iso { namespace win {

HACCEL		haccel;
HWND		hwnd_accel;
HWND		hwnd_dialog;


HHOOK	LatchMouseButtons::hook;
int		LatchMouseButtons::buttons;

void SetModelessDialog(HWND hWnd) {
	hwnd_dialog	= hWnd;
}

bool TranslateMessage(MSG *msg) {
	bool	ret = ::TranslateMessage(msg);
	if (ret && (msg->message == WM_KEYDOWN || msg->message == WM_KEYUP) && (msg->wParam == VK_SHIFT || msg->wParam == VK_CONTROL)) {
		if (Control c = GetControlAt(msg->pt)) {
			uint32	flags	= (GetKeyState(VK_LBUTTON)	& 0x8000 ? MK_LBUTTON	: 0)
							| (GetKeyState(VK_RBUTTON)	& 0x8000 ? MK_RBUTTON	: 0)
							| (GetKeyState(VK_SHIFT)	& 0x8000 ? MK_SHIFT		: 0)
							| (GetKeyState(VK_CONTROL)	& 0x8000 ? MK_CONTROL	: 0)
							| (GetKeyState(VK_MBUTTON)	& 0x8000 ? MK_MBUTTON	: 0);
			c.SendMessage(WM_MOUSEMOVE, flags, lparam(c.ToClient(msg->pt)));
		}
	}
	return ret;
}

void ProcessMessage(MSG &msg) {
	if (!hwnd_dialog || !IsDialogMessage(hwnd_dialog, &msg)) {
		if (!TranslateAccelerator(hwnd_accel, haccel, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
}

bool ProcessMessage(bool wait) {
	MSG			msg;
	if (wait) {
		if (!GetMessage(&msg, NULL, 0, 0))
			return false;
		ProcessMessage(msg);
	} else {
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT)
				return false;
			ProcessMessage(msg);
		}
	}
	return true;
}

uint32 ProcessMessage(const HANDLE *handles, int num, bool all, float timeout, uint32 mask) {
	uint32 r = MsgWaitForMultipleObjects(num, handles, all, timeout ? uint32(timeout * 1000) : INFINITE, mask);
	if (r == num) {
		MSG			msg;
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT)
				return false;
//			if (!GetMessage(&msg, NULL, 0, 0))
//				return 0;
			ProcessMessage(msg);
		}
		return 1;
	}
	return r < 0x100 ? r + 2 : r;
}

uint16 GetMouseButtons() {
	return	(GetKeyState(VK_CONTROL ) & 0x8000) / (0x8000 / MK_CONTROL )
		+	(GetKeyState(VK_LBUTTON ) & 0x8000) / (0x8000 / MK_LBUTTON )
		+	(GetKeyState(VK_MBUTTON ) & 0x8000) / (0x8000 / MK_MBUTTON )
		+	(GetKeyState(VK_RBUTTON ) & 0x8000) / (0x8000 / MK_RBUTTON )
		+	(GetKeyState(VK_SHIFT   ) & 0x8000) / (0x8000 / MK_SHIFT   )
		+	(GetKeyState(VK_XBUTTON1) & 0x8000) / (0x8000 / MK_XBUTTON1)
		+	(GetKeyState(VK_XBUTTON2) & 0x8000) / (0x8000 / MK_XBUTTON2);
}

//-----------------------------------------------------------------------------
//	Accelerator
//-----------------------------------------------------------------------------

const char *vk_names[] = {
	"",				//0x00
	"Lbutton",		//0x01
	"Rbutton",		//0x02
	"Cancel",		//0x03
	"Mbutton",		//0x04
	"Xbutton1",		//0x05
	"Xbutton2",		//0x06
	"",				//0x07
	"Bkspace",		//0x08
	"Tab",			//0x09
	"",				//0x0A
	"",				//0x0B
	"Clear",		//0x0C
	"Ret",			//0x0D
	"",				//0x0E
	"",				//0x0F
	"Shift",		//0x10
	"Control",		//0x11
	"Menu",			//0x12
	"Pause",		//0x13
	"Capital",		//0x14
	"Kana",			//0x15
	"",				//0x16
	"Junja",		//0x17
	"Final",		//0x18
	"Hanja",		//0x19
	"",				//0x1A
	"Esc",			//0x1B
	"Convert",		//0x1C
	"Nonconvert",	//0x1D
	"Accept",		//0x1E
	"Modechange",	//0x1F
	"Space",		//0x20
	"PgUp",			//0x21
	"PgDown",		//0x22
	"End",			//0x23
	"Home",			//0x24
	"Left Arrow",	//0x25
	"Up Arrow",		//0x26
	"Right Arrow",	//0x27
	"Down Arrow",	//0x28
	"Select",		//0x29
	"Print",		//0x2A
	"Execute",		//0x2B
	"Snapshot",		//0x2C
	"Insert",		//0x2D
	"Del",			//0x2E
	"Help",			//0x2F
};
const char *vk_names2[] = {
	"Num 0",		//0x60
	"Num 1",		//0x61
	"Num 2",		//0x62
	"Num 3",		//0x63
	"Num 4",		//0x64
	"Num 5",		//0x65
	"Num 6",		//0x66
	"Num 7",		//0x67
	"Num 8",		//0x68
	"Num 9",		//0x69
	"Num *",		//0x6A
	"Num +",		//0x6B
	"Separator",	//0x6C
	"Num -",		//0x6D
	"Num .",		//0x6E
	"Num /",		//0x6F
};

string_accum& operator<<(string_accum &a, Accelerator::Key v) {
	a << onlyif(v.f & v.CTRL, "Ctrl+") << onlyif(v.f & v.ALT, "Alt+") << onlyif(v.f & v.SHIFT, "Shift+");
	if (!(v.f & v.ASCII)) {
		if (v.k < num_elements(vk_names))
			return a << vk_names[v.k];
		if (v.k >= VK_F1)
			return a << 'F' << v.k - VK_F1 + 1;
		if (v.k >= VK_NUMPAD0)
			return a << vk_names2[v.k - VK_NUMPAD0];
	}
	return a << char(v.k);
}

void SetAccelerator(HWND hWnd, Accelerator a) {
	haccel		= a;
	hwnd_accel	= hWnd;
//	ISO_TRACEF("Set Accelerator 0x%0x\n", hWnd);
}

void Accelerator::Add(const ACCEL &a) {
	int		n		= CopyAcceleratorTable(h, NULL, 0);
	ACCEL	*temp	= alloc_auto(ACCEL, n + 1);
	CopyAcceleratorTable(h, temp, n);
	temp[n]			= a;
	DestroyAcceleratorTable(h);
	h = CreateAcceleratorTable(temp, n + 1);
}

void Accelerator::Add(const ACCEL *a, int n) {
	int		n0		= CopyAcceleratorTable(h, NULL, 0);
	ACCEL	*temp	= alloc_auto(ACCEL, n0 + n);
	CopyAcceleratorTable(h, temp, n0);
	memcpy(temp + n0, a, n * sizeof(ACCEL));
	DestroyAcceleratorTable(h);
	h = CreateAcceleratorTable(temp, n + 1);
}

Accelerator::Key Accelerator::GetKey(uint16 cmd) const {
	int		n		= CopyAcceleratorTable(h, NULL, 0);
	ACCEL	*temp	= alloc_auto(ACCEL, n);
	CopyAcceleratorTable(h, temp, n);
	for (auto &i : make_range_n(temp, n)) {
		if (i.cmd == cmd)
			return {i.key, Key::FLAG(i.fVirt ^ Key::ASCII)};
	}
	return 0;
}

string_param Accelerator::KeyText(const char *text, uint16 cmd) {
	if (auto k = GetKey(cmd))
		return move(buffer_accum<256>() << text << '\t' << k);
	return text;
}

//-----------------------------------------------------------------------------
//	Menu
//-----------------------------------------------------------------------------

Menu Menu::GetSubMenuByID(int id) const {
	for (int i = 0, n = Count(); i < n; i++) {
		Menu::Item	item = GetItemByPos(i, MIIM_ID | MIIM_FTYPE | MIIM_STATE | MIIM_SUBMENU);
		if (item.wID == id)
			return item.SubMenu();
	}
	return Menu();
}

int Menu::FindPosition(const char *name) const {
	for (int i = 0, n = Count(); i < n; i++) {
		if (GetItemTextByPos(i) == str(name))
			return i;
	}
	return -1;
}

int Menu::FindPosition(int id) const {
	for (int i = 0, n = Count(); i < n; i++) {
		if (GetItemID(i) == id)
			return i;
	}
	return -1;
}

int Menu::FindPosition(Menu m) const {
	for (int i = 0, n = Count(); i < n; i++) {
		if (GetSubMenuByPos(i) == m)
			return i;
	}
	return -1;
}

Menu Menu::GetSubMenuByName(const char *name) const {
	int	i = FindPosition(name);
	return i < 0 ? Menu() : GetSubMenuByPos(i);
}

void Menu::Append(const Menu &m) const {
	Menu	sub;
//	for (int i = 0; sub = m.GetSubMenuByPos(i); i++)
//		Append(str(m.GetItemTextByPos(i)), sub);
	for (auto &i : m.Items())
		Append(i);
}

Menu Menu::FindSubMenu(const char *name) const {
	for (int i = 0, n = Count(); i < n; i++) {
		if (Menu m = GetSubMenuByPos(i)) {
			if (Menu m2 = m.FindSubMenu(name))
				return m2;
		} else if (GetItemTextByPos(i) == str(name)) {
			return *this;
		}
	}
	return Menu();
}

Menu Menu::FindSubMenu(int id) const {
	for (int i = 0, n = Count(); i < n; i++) {
		if (Menu m = GetSubMenuByPos(i)) {
			if (Menu m2 = m.FindSubMenu(id))
				return m2;
		} else if (GetItemID(i) == id) {
			return *this;
		}
	}
	return Menu();
}

void Menu::Radio(int id) const {
	if (Menu m = FindSubMenu(id)) {
		int	last_separator = 0;
		for (int i = 0, n = m.Count(); i < n; i++) {
			Menu::Item	item = m.GetItemByPos(i, MIIM_ID | MIIM_FTYPE | MIIM_STATE);
			if (item.Type() & MFT_SEPARATOR)
				last_separator = i + 1;
			if (item.ID() == id)
				break;
		}
		for (int i = last_separator, n = m.Count(); i < n; i++) {
			Menu::Item	item = m.GetItemByPos(i, MIIM_ID | MIIM_FTYPE | MIIM_STATE);
			if (item.Type() & MFT_SEPARATOR)
				break;
			item.fState = item.wID == id ? (item.fState | MFS_CHECKED) : (item.fState & ~MFS_CHECKED);
			item.fType |= MFT_RADIOCHECK;
			item.SetByPos(m, i);
		}
	}
}

void Menu::RadioDirect(int id, int id0, int id1) const {
	for (int i = 0, n = Count(); i < n; i++) {
		Menu::Item	item = GetItemByPos(i, MIIM_ID | MIIM_FTYPE | MIIM_STATE);
		if (item.fType & MFT_RADIOCHECK) {
			item.Check(item.wID == id);
			item.SetByPos(*this, i);
		} else if (between(item.wID, id0, id1)) {
			item.Check(item.wID == id);
			item.fType |= MFT_RADIOCHECK;
			item.SetByPos(*this, i);
		}
	}
}

void Menu::RadioByPos(int i, int i0, int i1) const {
	for (int j = max(i0, 0), j1 = min(i1 + 1, Count()); j < j1; j++) {
		Menu::Item	item = GetItemByPos(j, MIIM_ID | MIIM_FTYPE | MIIM_STATE);
		item.fState = j == i ? (item.fState | MFS_CHECKED) : (item.fState & ~MFS_CHECKED);
		item.fType |= MFT_RADIOCHECK;
		item.SetByPos(*this, j);
	}
}

void Menu::Radio(int id, int id0, int id1) const {
	if (Menu m = FindSubMenu(id))
		m.RadioDirect(id, id0, id1);
}

//-----------------------------------------------------------------------------
//	Font
//-----------------------------------------------------------------------------

const char *font_weights[] = {"thin","extralight","light","normal","medium","semibold","bold","extrabold","black"};

template<> Font::Params &Font::Params::Description(const char *desc) {
	if (desc) {
		string_scan		ss(desc);
		if (ss.scan(';') || ss.scan(' ')) {
			memcpy(lfFaceName, desc, ss.getp() - desc);
			lfFaceName[ss.getp() - desc] = 0;
			count_string	tok = ss.move(1).get_token();

			for (int i = 0; i < num_elements(font_weights); i++) {
				if (tok == istr(font_weights[i])) {
					lfWeight = (i + 1) * 100;
					tok	= ss.get_token();
					break;
				}
			}

			if (tok == "italic") {
				lfItalic = true;
				tok = ss.get_token();
			}

			PointSize(from_string<int>(tok.begin()));
		} else {
			strcpy(lfFaceName, desc);
		}
	}
	return *this;
}

template<> Font::Params16& Font::Params16::Description(const char16* desc) {
	Font::Params p(desc);
	lfHeight			= p.lfHeight;
    lfWidth				= p.lfWidth;
    lfEscapement		= p.lfEscapement;
    lfOrientation		= p.lfOrientation;
    lfWeight			= p.lfWeight;
    lfItalic			= p.lfItalic;
    lfUnderline			= p.lfUnderline;
    lfStrikeOut			= p.lfStrikeOut;
    lfCharSet			= p.lfCharSet;
    lfOutPrecision		= p.lfOutPrecision;
    lfClipPrecision		= p.lfClipPrecision;
    lfQuality			= p.lfQuality;
    lfPitchAndFamily	= p.lfPitchAndFamily;
    string_copy(lfFaceName, p.lfFaceName);
	return *this;
}

template<> fixed_string<256> Font::Params::Description() const {
	buffer_accum<256>	sa;
	sa	<< Name() << "; "
		<< (lfWeight == 0 ? "" : font_weights[clamp((lfWeight + 50) / 100 - 1, 0, num_elements(font_weights))])
		<< onlyif(lfItalic, " italic") << ' '
		<< PointSize();
	return str(sa);
}

//-----------------------------------------------------------------------------
//	Bitmap & Image Scaling
//-----------------------------------------------------------------------------

Bitmap Bitmap::CreateDIBSection(int w, int h, int bits, uint32 planes, void **data) {
	BITMAPINFOHEADER bi;
	clear(bi);
	bi.biSize			= sizeof(BITMAPINFOHEADER);
	bi.biWidth			= w;
	bi.biHeight			= -h;
	bi.biPlanes			= planes;
	bi.biBitCount		= bits;
	bi.biCompression	= BI_RGB;
	return CreateDIBSection(bi, data);
}

BitmapScaler::BitmapScaler(const Bitmap &bm1, void *data1, const POINT &size1, const Bitmap &bm2, void *data2, const POINT &size2) : size1(size1), size2(size2), data1(data1), data2(data2) {
	dc1		= DeviceContext().Compatible();
	old1	= dc1.Select(bm1);
#ifdef USE_GDI_SCALING
	dc2		= DeviceContext().Compatible();
	old2	= dc2.Select(bm2);
#endif
}

BitmapScaler::~BitmapScaler() {
	dc1.Select(old1);
#ifdef USE_GDI_SCALING
	dc2.Select(old2);
#endif
}

#ifdef USE_GDI_SCALING
void BitmapScaler::Scale() {
	dc2.Blit(dc1, Rect(Point(0, 0), size1), Rect(Point(0, 0), size2));
}
#else
void BitmapScaler::Scale() {
	if (between(size2.x * 4, size1.x * 3, size1.x * 5) && between(size2.y * 4, size1.y * 3, size1.y * 5))
		resample_point(MakeDIBBlock((DIBHEADER::RGBQUAD*)data2, size2), MakeDIBBlock((DIBHEADER::RGBQUAD*)data1, size1));
	else
		resample_via<HDRpixel>(MakeDIBBlock((DIBHEADER::RGBQUAD*)data2, size2), MakeDIBBlock((DIBHEADER::RGBQUAD*)data1, size1));
}
#endif

Bitmap Bitmap::ScaledTo(const POINT &newsize) const {
	if (!h)
		return *this;

	Point	oldsize = GetSize();
	if (oldsize == newsize)
		return *this;

	void			*data2;
	Bitmap			bm2 = Bitmap::CreateDIBSection(newsize, 32, 1, &data2);
	BitmapScaler(*this, GetBits(DeviceContext::Screen(), 0, oldsize.y), oldsize, bm2, data2, newsize).Scale();
	return bm2;
}

//-----------------------------------------------------------------------------
//	Control
//-----------------------------------------------------------------------------

Control	Control::DescendantByID(ID id) const {
	for (auto c : Children()) {
		if (c.id == id)
			return c;
		if (Control d = c.DescendantByID(id))
			return d;
	}
	return Control();
}

Control Control::DescendantAt(const POINT &p) const {
	Control c = *this, prev;
	while (prev != c && c) {
		prev	= c;
		c		= c.ChildAt(p);
	}
	return prev;
};

//-----------------------------------------------------------------------------
//	TabControl
//-----------------------------------------------------------------------------

int TabControl::HitTest(const POINT &pt) const {
	for (int i = 0, n = Count(); i < n; i++) {
		if (GetItemRect(i).Contains(pt))
			return i;
	}
	return -1;
}

//-----------------------------------------------------------------------------
//	ToolBarControl
//-----------------------------------------------------------------------------

bool ToolBarControl::Init(const Data *tb, int image) const {
	if (!tb)
		return false;

	SendMessage(TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON));
	SetBitmapSize(tb->GetSize());

	int			nb			= tb->size();
	TBBUTTON	*buttons	= alloc_auto(TBBUTTON, nb);
	memset(buttons, 0, sizeof(TBBUTTON) * nb);

	for (int i = 0; i < nb; i++) {
		if (int id = (*tb)[i]) {
			buttons[i].iBitmap	= image++;
			buttons[i].idCommand= id;
			buttons[i].fsState	= TBSTATE_ENABLED;
			buttons[i].fsStyle	= BTNS_BUTTON;
			buttons[i].iString	= id;
//			const char *caption	= LoadString(id);
//			buttons[i].iString	= INT_PTR(caption ? ::strdup(caption) : 0);
		} else {
			buttons[i].iBitmap	= -1;//I_IMAGENONE;
			buttons[i].fsStyle	= BTNS_SEP;
		}
	}
	AddButtons(buttons, nb);
//	SendMessage(TB_AUTOSIZE, 0, 0);

	return true;
}

bool ToolBarControl::Add(Menu menu) const {
	SetExtendedStyle(MIXEDBUTTONS);
	for (int i = 0, n = menu.Count(); i < n; i++) {
		Menu::Item	item = menu.GetItemByPos(i, MIIM_ID | MIIM_FTYPE | MIIM_SUBMENU);
		if (Menu sub = item.SubMenu()) {
			ToolBarControl::Button(menu.GetItemTextByPos(i), i, BTNS_DROPDOWN | BTNS_AUTOSIZE).NoBitmap().Insert(*this);
			GetItem(i).Param((HMENU)sub).Set(*this, i);
		} else if (item.Type() & MFT_SEPARATOR) {
			ToolBarControl::Separator().Insert(*this);
		} else {
			ToolBarControl::Button(menu.GetItemTextByPos(i), item.ID(), BTNS_AUTOSIZE).Insert(*this);
		}
	}
//	SendMessage(TB_AUTOSIZE);
	return true;
}

bool ToolBarControl::Init(Menu menu) const {
	SendMessage(TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON));
	SetBitmapSize(Point(0,0));
	ButtonSize(Point(80,20));

	for (int i = 0, n = menu.Count(); i < n; i++) {
#if 1
		Menu::Item	item = menu.GetItemByPos(i, MIIM_ID | MIIM_FTYPE | MIIM_SUBMENU);
		if (Menu sub = item.SubMenu()) {
			ToolBarControl::Button(menu.GetItemTextByPos(i), i, BTNS_DROPDOWN | BTNS_AUTOSIZE).Insert(*this);
			GetItem(i).Param((HMENU)sub).Set(*this, i);
		} else if (item.Type() & MFT_SEPARATOR) {
			ToolBarControl::Separator().Insert(*this);
		} else {
			ToolBarControl::Button(menu.GetItemTextByPos(i), item.ID(), BTNS_AUTOSIZE).Insert(*this);
		}
#else
		if (Menu sub = menu.GetSubMenuByPos(i)) {
			ToolBarControl::Button(menu.GetItemTextByPos<256>(i), i, BTNS_DROPDOWN | BTNS_AUTOSIZE).Insert(*this);
			GetItem(i).Param((HMENU)menu.GetSubMenuByPos(i)).Set(*this, i);
		} else {
			ToolBarControl::Button(menu.GetItemTextByPos<256>(i), menu.GetItemID(i), BTNS_AUTOSIZE).Insert(*this);
		}
#endif
	}
	SendMessage(TB_AUTOSIZE);
	return true;
}

//-----------------------------------------------------------------------------
//	TreeControl
//-----------------------------------------------------------------------------

int TreeControl::FindChildIndex(HTREEITEM hParent, HTREEITEM hChild) const {
	int	i = 0;
	for (HTREEITEM h = GetChildItem(hParent); h != hChild; h = GetNextItem(h))
		++i;
	return i;
}

HTREEITEM TreeControl::GetChildItem(HTREEITEM hParent, int i) const {
	HTREEITEM h = GetChildItem(hParent);
	while (i--)
		h = GetNextItem(h);
	return h;
}

HTREEITEM TreeControl::_CopyChildren(HTREEITEM hItem, HTREEITEM hItemTo) const {
	for (HTREEITEM child = GetChildItem(hItem), next; child; child = next) {
		next = GetNextItem(child);
		_Copy(child, hItemTo, TVI_LAST);
	}
	return hItemTo;
}

HTREEITEM TreeControl::_Copy(HTREEITEM hItem, HTREEITEM hItemTo, HTREEITEM hItemPos) const {
	fixed_string<256>	text;
	TreeControl::Item	item(hItem, TVIF_HANDLE | TVIF_IMAGE | TVIF_PARAM | TVIF_SELECTEDIMAGE | TVIF_STATE);

	item.Text(text).Get(*this);
	item.stateMask = ~(TVIS_DROPHILITED | TVIS_EXPANDED | TVIS_EXPANDEDONCE | TVIS_EXPANDPARTIAL | TVIS_SELECTED);
	return _CopyChildren(hItem, item.Insert(*this, hItemTo, hItemPos));
}

bool TreeControl::_CopyCheck(HTREEITEM hItem, HTREEITEM hItemTo) const {
	if (hItem == NULL || hItemTo == NULL)
		return false;

	// check we're not trying to move to a descendant
	for (HTREEITEM hItemParent = hItemTo; hItemParent; hItemParent = GetParentItem(hItemParent)) {
		if (hItemParent == hItem)
			return false;
	}
	return true;
}

//-----------------------------------------------------------------------------
//	Window
//-----------------------------------------------------------------------------

WindowClass::WindowClass(const char *name, WNDPROC proc, uint32 style, uint32 cextra, uint32 wextra, HBRUSH bg, HCURSOR hc) {
	WNDCLASSA	wc;
	wc.style		 = style;			// Class style.
	wc.lpfnWndProc	 = proc;			// Window procedure for this class.
	wc.cbClsExtra	 = cextra;			// per-class extra data.
	wc.cbWndExtra	 = wextra;			// per-window extra data.
	wc.hInstance	 = GetDefaultInstance(); // Application that owns the class.
	wc.hIcon		 = NULL;
	wc.hCursor		 = hc;
	wc.hbrBackground = bg;
	wc.lpszMenuName	 = NULL;			// Name of menu resource in .RC file.
	wc.lpszClassName = name;			// Name used in call to CreateWindow.

	atom = RegisterClassA(&wc);
}

//-----------------------------------------------------------------------------
//	Miscellaneous
//-----------------------------------------------------------------------------

NONCLIENTMETRICSA GetNonClientMetrics() {
	NONCLIENTMETRICSA	metrics = {sizeof(NONCLIENTMETRICS)};
	SystemParametersInfoA(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0);
	return metrics;
}

Region::Region(const RECT *r, int n) : H<HRGN>(CreateRectRgnIndirect(r++)) {
	while (--n)
		*this |= *r++;
}

uint64 GetVersion(const char *path) {
	const VersionLink	*p	= Resource(GetModuleHandleA(path), VS_VERSION_INFO, RT_VERSION);
	VS_FIXEDFILEINFO	*v	= p->data();
	return v->dwSignature == 0xfeef04bd ? (uint64(v->dwFileVersionMS) << 32) | v->dwFileVersionLS : 0;
}

} } //namespace iso::win
