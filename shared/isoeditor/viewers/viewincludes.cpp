#include "viewtree.h"
#include "main.h"
#include "windows/d2d.h"
#include "utilities.h"
#include "stream.h"
#include "base/bits.h"
#include "base/hash.h"

using namespace app;

//-----------------------------------------------------------------------------

struct FileNode : TreeNode {
	uint32			index;
	const char		*fn;
	com_string		label;
	float2			vel;
	int				type;

	typedef	dynamic_array<FileNode*>	array;

	array	depends;
	array	dependents;
	uint32	depth;

	FileNode(param(rectangle) r, const char *fn, int type)
		: TreeNode(r), fn(fn), label(filename(fn).name_ext()), vel(zero), type(type), depth(0)
	{}

	void	AddDependency(FileNode *d) {
		depends.push_back(d);
		d->dependents.push_back(this);
		//ISO_TRACEF(fn) << " includes " << d->fn << '\n';
	}
	void	SetDepth(uint32 d, bool up, hash_map<FileNode*, bool> &hash) {
		if (d > depth) {
			if (hash.check(this))
				return;	// circular
			hash[this] = true;
			depth = d++;
			if (up) {
				for (auto &i : depends)
					i->SetDepth(d, up, hash);
			} else {
				for (auto &i : depends)
					i->SetDepth(d, up, hash);
			}
		}
	}
	uint32	NumLeafs() {
		if (dependents.empty())
			return 1;
		uint32	n = 0;
		for (auto &i : dependents)
			n += i->NumLeafs();
		return n;
	}
};

void SetTreePos(FileNode *node, int x, int y) {
	*(rectangle*)node = rectangle::with_centre(position2(x * 100, y * 100), node->half_extent());
	//x -= node->NumLeafs() / 2;
	for (auto &i : node->dependents) {
		SetTreePos(i, x, y + 1);
		x += i->NumLeafs();
	}
}


struct Spring {
	FileNode	*a, *b;
	Spring(FileNode *_a, FileNode *_b) : a(_a), b(_b)	{}
};

float	damping		= 0.9f;
float	spring_k	= 0.1f;

class ViewIncludes : public aligned<Window<ViewIncludes>,16>, public WindowTimer<ViewIncludes> {
	d2d::WND					d2d;
	d2d::Write					write;
	d2d::Font					font;
	ISO_ptr<anything>			ged;
	FileNode::array				files;
	dynamic_array<Spring>		springs;
	FileNode					*selected;

	float	zoom;
	float2	pos;
	Point	prevmouse;
	Tree	tree;

	void	Paint(const Rect &r);
	void	Physics(float dt);
public:
	LRESULT Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
	ViewIncludes(const WindowPos &wpos, ISO_ptr<void> p);
};


struct includes : dynamic_array<string>	{
	filename	root;

	includes(const char *_root) : root(_root) {}

	void	add(const string &s) {
		if (*s == '%')
			return;
		filename	f(s);
		if (f.is_relative())
			f = root.relative(s);
		for (auto &i : *this) {
			if (i == f)
				return;
		}
		push_back(f);
	}
};


bool alphasort(const FileNode *a, const FileNode *b)	{ return istr(a->fn) < b->fn; }
bool alphafind(const FileNode *a, const char *b)		{ return istr(a->fn) < b; }

ViewIncludes::ViewIncludes(const WindowPos &wpos, ISO_ptr<void> p) : font(write, L"Verdana", 10), selected(0), zoom(1), pos(zero) {
	Create(wpos, NULL, CHILD | VISIBLE, CLIENTEDGE);
#if 0
	FileNode	*a = new FileNode(rectangle::with_length(float2(0,0), float2(90,70)), "A", 0);
	FileNode	*b = new FileNode(rectangle::with_length(float2(200,0), float2(90,70)), "B", 1);
	files.push_back(a);
	files.push_back(b);
//	a->AddDependency(b);
#else
	ISO::Browser	b(p);
	const char *projectpath = b["ProjectPath"].GetString();
	includes	inc_dirs(projectpath);

	for (ISO::Browser::iterator i = b.begin(), e = b.end(); i != e; ++i) {
		if (i.GetName() == "ItemGroup") {
			for (ISO::Browser::iterator i2 = i->begin(), e2 = i->end(); i2 != e2; ++i2) {
				tag		name	= i2.GetName();
				int		type	= name == "ClCompile" ? 0 : name == "ClInclude" ? 1 : -1;
				if (type >= 0) {
					const char *fn = (*i2)["Include"].GetString();
					int	n = files.size32();
					int	x = even_bits(n), y = even_bits(n >> 1);

					com_ptr<IDWriteTextLayout>	layout;
					DWRITE_TEXT_METRICS			metrics;
					write.CreateTextLayout(&layout, str16(name), font, 1e9, 1e9);
					layout->GetMetrics(&metrics);

					files.push_back(new FileNode(
						rectangle::with_length(position2(x * 100, y * 100), float2{metrics.width, metrics.height}),
						fn, type
					));
				}
			}
		} else if (i.GetName() == "ItemDefinitionGroup") {
			if (const char *s = (*i)["ClCompile"]["AdditionalIncludeDirectories"][0].GetString()) {
				const char *p = s;
				while (p = strchr(s, ';')) {
					inc_dirs.add(str(s, p));
					s = p + 1;
				}
				inc_dirs.add(s);
			}
		}
	}

	sort(files, alphasort);

	auto d = files.begin();
	if (!files.empty()) {
		for (auto &i : files) {
			if (istr(i->fn) != (&i)[1]->fn) {
				i->index = d - files.begin();
				*d++ = i;
			}
		}
	}
	files.resize(d - files.begin());

	for (auto &i : files) {
		filename	fn(i->fn);
		FileInput	file(fn);
		auto		text = make_text_reader(file);
		if (file.exists()) {
			char	line[256];
			while (text.read_line(line)) {
				string_scan	ss(line);
				if (ss.skip_whitespace().getc() == '#' && ss.get_token() == "include") {
					filename	inc;
					switch (ss.skip_whitespace().getc()) {
						case '<':	inc = ss.get_token(~char_set('>')); break;
						case '"':	inc = ss.get_token(~char_set('"')); break;
						default:	continue;
					}
					inc = inc.convert_to_backslash();
					if (inc.is_relative()) {
						filename	afn = fn.relative(inc);
						if (exists(afn)) {
							inc = afn;
						} else {
							for (auto &i : inc_dirs) {
								filename	afn = filename(i).add_dir(inc);
								if (exists(afn)) {
									inc = afn;
									break;
								}
							}
						}
					}
					auto	n = lower_boundc(files, inc, alphafind);
					if (n != files.end() && (*n)->fn == inc)
						i->AddDependency(*n);
					else
						ISO_TRACEF("Can't find ") << inc << '\n';
				}
			}
		}
	}
#endif

	for (FileNode *node : files) {
		if (node->dependents.empty()) {
			hash_map<FileNode*, bool> hash;
			node->SetDepth(1, true, hash);
		}
		for (auto &i : node->depends)
			new (springs) Spring(node, i);
	}

	dynamic_array<int>	counts;
	for (FileNode *node : files) {
		uint32	depth = node->depth;
		if (depth > counts.size())
			counts.resize(depth, 0);
		++counts[depth - 1];
	}
	for (FileNode *node : files) {
		uint32		depth	= node->depth;
		int			x		= counts[depth - 1];
		counts[depth - 1] -= 2;
		node->set_centre(position2(-x * 100 / 2, int(counts.size() - depth) * 100));
	}


	tree.Init((TreeNode**)files.begin(), files.size32());
#ifdef _DEBUG
	SetTimer(1, .1f);
#else
	Timer::Start(1 / 30.f);
#endif
}

void ViewIncludes::Paint(const Rect &r) {

	d2d.Clear(colour(1,1,1));

	d2d::SolidBrush	white(d2d, colour(1,1,1)), black(d2d, colour(0,0,0));
	com_ptr<ID2D1SolidColorBrush>	brushes[2];
	d2d.CreateBrush(&brushes[0], colour(0,1,0));
	d2d.CreateBrush(&brushes[1], colour(1,0,0));

	d2d.SetTransform(translate(pos.x, pos.y) * scale(zoom));

	rectangle	client = rectangle(position2{r.Left(), r.Top()}, position2{r.Right(), r.Bottom()} - pos) / zoom;
	dynamic_bitarray<>	visible(files.size32());
	for (Tree::iterator it = tree.begin(); it; ) {
		if (!overlap(*it, client)) {
			it.skip();
		} else {
			if (it->IsLeaf()) {
				FileNode	*i = (FileNode*)(TreeNode*)it;
				visible.set(i->index);
			}
			++it;
		}
	}

	for (auto i : visible.where(true)) {
		FileNode	*n = files[i];
		for (FileNode *d : n->depends) {
			d2d.DrawLine(
				float2{n->centre().v.x, n->a.v.y},
				float2{d->centre().v.x, d->b.v.y},
				black
			);
		}
		for (FileNode *d : n->dependents) {
			if (!visible.test(d->index)) {
				d2d.DrawLine(
					float2{n->centre().v.x, n->a.v.y},
					float2{d->centre().v.x, d->b.v.y},
					black
				);
			}
		}

	}
	for (auto i : visible.where(true)) {
		FileNode	*n = files[i];
		d2d.Fill(*n, brushes[n->type]);
		d2d.DrawText(*n, n->label, font, white);
	}
}

void ViewIncludes::Physics(float dt) {
	Tree::intersections	in;
	tree.CollectIntersections(in, float2(100));
	for (FileNode *n : files) {
		if (n == selected)
			n->vel = zero;
		n->set_pos(n->a + n->vel * dt);
		n->vel *= damping;
	}
	for (auto &i : in) {
		FileNode	*a = (FileNode*)i.a;
		FileNode	*b = (FileNode*)i.b;
		float2		d	= a->centre() - b->centre();
		float		m	= len2(d);
		if (m < 10000) {
			float	x = sqrt(m);
			d	= d / x * (100 - x);
			a->vel += d;
			b->vel -= d;
		}
	}
	for (auto &i : springs) {
		float2		d	= i.a->centre() - i.b->centre();
		d.x				*= 0.25f;
		d.y				-= 200;
		d				*= spring_k;
		i.a->vel		-= d;
		i.b->vel		+= d;

		if (len(d) > 10000)
			d = d;
	}
	tree.Init((TreeNode**)files.begin(), files.size32());
}

LRESULT ViewIncludes::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE:
			d2d.Init(hWnd, GetClientRect().Size());
			break;

		case WM_SIZE: {
			if (!d2d.Resize(Point(lParam)))
				d2d.DeInit();
			Invalidate();
			break;
		}

		case WM_ERASEBKGND:
			return TRUE;

		case WM_PAINT: {
			DeviceContextPaint	dc(*this);
			Rect		r = GetClientRect();
			d2d.Init(hWnd, r.Size());
			if (!d2d.Occluded()) {
				d2d.BeginDraw();
				Paint(r);
				if (d2d.EndDraw())
					d2d.DeInit();
			}
			break;
		}

		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
			prevmouse	= Point(lParam);
			SetFocus();
			selected	= (FileNode*)tree.Find(position2(float2{float(prevmouse.x), float(prevmouse.y)} - pos) / zoom);
			return 0;

		case WM_LBUTTONUP:
			if (selected) {
				//tree.Remove(selected);
				//tree.Insert(selected);
				selected = 0;
			}
			break;

		case WM_MOUSEMOVE:
			if (wParam & MK_LBUTTON) {
				Point	mouse(lParam);
				float2	move{float(mouse.x - prevmouse.x), float(mouse.y - prevmouse.y)};
				if (selected) {
					selected->vel = move / zoom * 18;
					*(rectangle*)selected += move / zoom;
				} else {
					pos += move;
				}
				prevmouse	= mouse;
				Invalidate();
			}
			break;

		case WM_MOUSEWHEEL: {
			Point	pt0		= ToClient(Point(lParam));
			float2	pt		= float2{float(pt0.x), float(pt0.y)};
			float	mult	= iso::pow(1.05f, (short)HIWORD(wParam) / 64.f);
			zoom	*= mult;
			pos		= pt + (pos - pt) * mult;
			Invalidate();
			break;
		}

		case WM_ISO_TIMER:
		case WM_TIMER:
			Physics(0.1f);
			Invalidate();
			Update();
			SetTimer(1, .1f);
			return 0;

		case WM_NCDESTROY:
			delete this;
			return 0;
	}
	return Super(message, wParam, lParam);
}


class EditorInclude : public Editor {
	virtual bool Matches(const ISO::Type *type) {
		return type->Is("VSProject");
	}
	virtual Control Create(MainWindow &main, const WindowPos &wpos, const ISO_ptr<void> &p) {
		return *new ViewIncludes(wpos, p);
	}
} editorincludes;
