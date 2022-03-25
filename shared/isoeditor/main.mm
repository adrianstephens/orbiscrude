#include "iso/iso.h"
#include "iso/iso_files.h"
#include "iso/iso_binary.h"
#include "iso/iso_script.h"
#include "filetypes/bitmap/bitmap.h"
#include "sockets.h"
#include "main.h"
#include "devices/device.h"

#import <Cocoa/Cocoa.h>

extern char **environ;

using namespace iso;
ISO_ptr<void>	GetRemote(tag2 id, const char *target, const char *spec)	{ return ISO_NULL; }
ISO_ptr<void>	GetRemote(tag2 id, ISO_ptr<void> p, const char *spec)		{ return ISO_NULL; }
win::Control MakeTextViewer(const win::WindowPos &wpos, const char *title, const char *text, size_t len) { return win::Control(); }
win::Control MakeHTMLViewer(const win::WindowPos &wpos, const char *title, const char *text, size_t len) { return win::Control(); }
//Control EditWiiShader(app::MainWindow &main, const iso::rect &rect, void *p);

bool DisassembleX360Shader(void *p, ostream_ref file, bool ps)					{ return false; }
bool DisassemblePS3Shader(void *p, ostream_ref file, bool ps)					{ return false; }
bool DisassemblePS3Shader(void *p, size_t size, ostream_ref file, bool ps)		{ return false; }



namespace app {

ImageListBitmap GetTreeIcon(const ISO::Browser2 &b) {
	return Bitmap();
}

//-----------------------------------------------------------------------------
//	MainWindow
//-----------------------------------------------------------------------------

MainWindow		*MainWindow::me;

MainWindow::MainWindow() : SeparateWindow(48)	{ me = this; }
MainWindow::~MainWindow()						{ me = 0; }

void MainWindow::AddView(Control c)						{}
void MainWindow::SetTitle(const char *title)			{}
void MainWindow::AddLoadFilters(multi_string filters)	{}


//-----------------------------------------------------------------------------
//	IsoEditor
//-----------------------------------------------------------------------------

IsoEditor::IsoEditor() : root_ptr("root"), max_expand(1000) {
	tree.SetHandler<TreeMessageExpand>(this);
	tree.SetHandler<TreeMessagePreview>(this);
	sidebar.SetHandler<TreeMessageSelect>(this);
	ISOTree(tree).Setup(root_ptr, TVI_ROOT, max_expand);
	SetHandler<ControlMessageDestroy>(this);
}

void IsoEditor::operator()(ControlMessageDestroy &m) {
	[NSApp terminate: nil];
}

//void IsoEditor::expand(HTREEITEM h) {
void IsoEditor::operator()(TreeMessageExpand &m) { HTREEITEM h = m.item;
	ISO::Browser2	b = GetBrowser(h);
	ISOTree(tree).Setup(b, h, abs(max_expand));
}

//Control	IsoEditor::preview(HTREEITEM h) {
void IsoEditor::operator()(TreeMessagePreview &msg) { HTREEITEM h = msg.item;
	ISO::Browser2	b = SkipPointer(GetBrowser(h));
	
	if (b.Is<bitmap>()) {
		if (const char *fn = b.External()) {
			b = FileHandler::CachedRead(fn);
		}
		if (bitmap *bm	= b) {
			block<ISO_rgba,2>	bl	= bm->All();
			win::Bitmap			wbm;
			wbm.Create(bl[0], bl.size<1>(), bl.size<2>(), bl.pitch(), 32);

			ImageControl	c;
			c.Create();
			c.SetImage(wbm);
			msg.view = c;
		}
		return;
	}
	
	char			s[256];
	fixed_accum		m(s);
	bool	done = false;

	if (b.IsPtr()) {
		if (b.HasCRCType()) {
			m << "unknown type";
			done = true;
		}/* else if (tag id = b.GetName()) {
			m << id;
			done = true;
		}*/
	}

	if (!done) {
		const ISO::Type *t = b.GetTypeDef()->SkipUser();
		if (t && t->GetType() == ISO::REFERENCE && *(ISO_ptr<void>*)b) {
			if (b.HasCRCType()) {
				m << "unknown type";
				done = true;
			} else if (tag id = b.GetName()) {
				m << id;
				done = true;
			} else {
				b = *b;
				t = b.GetTypeDef()->SkipUser();
			}
		}

		if (!done && b.External()) {
			m << (char*)b;
			done = true;
		}
		if (!done) {
			if (t && t->GetType() == ISO::STRING) {
				const void	*s = t->ReadPtr(b);
				if (t->flags & ISO::TypeString::UTF16) {
					m << (const char16*)s;
				} else {
					m << (const char*)s;
				}
			} else {
				memory_writer	mo(s);//memory_block(s)));
				ISO::ScriptWriter(mo).SetFlags(ISO::SCRIPT_ONLYNAMES).DumpData(b);
				m.move(mo.tell32());
			}
		}
	}
	
	TextControl	c;
	c.Create();
	c.SetText(m);
	
	msg.view = c;
	//return c;
}
void IsoEditor::operator()(TreeMessageSelect &msg) { HTREEITEM h = msg.item;
	if (DeviceCreate *dev = sidebar.GetItem(h).Param())
		AddEntry(unconst((*dev)(*this)), false);
}

void IsoEditor::ModalError(const char *s) {
}

RegKey	Settings(bool write) {
	return RegKey();
}

void init_root() {
//	ISO_allocate::flags.set(ISO_allocate::TOOL_DELETE);
	ISO::root().Add(ISO_ptr<anything>("externals"));
	ISO::root().Add(ISO::GetEnvironment("environment", environ));

	ISO_ptr<anything>	variables("variables");
	ISO::root().Add(variables);
	variables->Append(ISO_ptr<int>("isoeditor",		1));
	variables->Append(ISO_ptr<int>("raw",			1));
	variables->Append(ISO_ptr<string>("exportfor",	"neutral"));

	{
		RegKey	reg0	= Settings();
		if (!reg0.HasSubKey("defaults")) {
			RegKey	reg(reg0, "defaults");
			reg.values()["keepexternals"] = uint32(1);
		}
		RegKey	reg(reg0, "defaults");
		for (RegKey::Value::iterator i = reg.values().begin(), e = reg.values().end(); i != e; ++i) {
			RegKey::Value	v = *i;
			switch (v.type) {
				case RegKey::sz:		variables->Append(ISO_ptr<string>(v.name, v)); break;
				case RegKey::uint32:	variables->Append(ISO_ptr<uint32>(v.name, v.get<uint32>())); break;
			}
		}
	}
}


}// namespace app

@interface AppDelegate : NSObject<NSApplicationDelegate> {
	app::IsoEditor	main;
};
@property(weak) IBOutlet NSOutlineView	*sidebar;
@property(weak) IBOutlet NSBrowser		*tree;
@property(weak) IBOutlet NSWindow		*window;
@end

@implementation AppDelegate

-(void)applicationDidFinishLaunching:(NSNotification*)aNotification {
	app::init_root();
	main.Bind(self.window);
	main.tree.Bind(self.tree);
	main.sidebar.Bind(self.sidebar);
	app::DeviceAdd	add(main.sidebar);
	for (app::Device::iterator i = app::Device::begin(); i; ++i)
		(*i)(add);
}

-(void)applicationWillTerminate:(NSNotification*)aNotification {
}

@end

//void StartWebServer(const char *dir, PORT port, bool secure);

int main(int argc, const char * argv[]) {
//	StartWebServer("/Volumes/DevHD/dev/orbiscrude/website", 1080, false);

	return NSApplicationMain(argc, argv);
}
