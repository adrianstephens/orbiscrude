#include "viewtree.h"
#include "main.h"
#include "windows\d2d.h"
#include "extra/random.h"

using namespace app;

//-----------------------------------------------------------------------------

struct Individual : TreeNode {
	ISO_ptr<void>	p;
	string16		name;
	enum {UNKNOWN, MALE, FEMALE} sex;
	bool			set;

	Individual		*father, *mother, *child;
	Individual		*same_father, *same_mother;
	dynamic_array<Individual*>	spouses;

	float2			vel;

	static ISO_ptr<void> Find(const anything *pa, tag2 id, int depth = 2);

	ISO_ptr<void> Find(tag2 id) const { return Find(p, id); }
	const char *GetString(tag2 id) {
		if (ISO::Browser2 b = Find(id)) {
			if (!b.Is<string>())
				b = b[0];
			return b.GetString();
		}
		return 0;
	}

	Individual(param(rectangle) r, ISO_ptr<void> &_p) : TreeNode(r), p(_p), set(false) {
		mother = father = child = 0;
		same_mother = same_father = 0;

		name = GetString("NAME");
		name = replace(name, "/", "");

		sex = UNKNOWN;
		if (const char *s = GetString("SEX"))
			sex = s[0] == 'F' ? FEMALE : MALE;
	}

	int		NumChildren() const {
		int	n = 0;
		for (Individual *i = child; i; i = i->mother == this ? i->same_mother : i->same_father)
			n++;
		return n;
	}
	void	AddSpouse(Individual *s) {
		if (find(spouses, s) == spouses.end())
			spouses.push_back(s);
	}

	bool operator==(const ISO_ptr<void> &_p) const { return p == _p; }
};

ISO_ptr<void> Individual::Find(const anything *pa, tag2 id, int depth) {
	const anything &a = *pa;
	for (int i = 0, n = a.Count(); i < n; i++) {
		ISO_ptr<void>	p = a[i];
		if (p.IsID(id))
			return p;
		if (p.IsType<anything>() && depth && (p = Find(p, id, depth - 1)))
			return p;
	}
	return ISO_NULL;
}

struct Spring {
	Individual	*a, *b, *c;
	Spring(Individual *a, Individual *b, Individual *c = 0) : a(a), b(b), c(c)	{}
};

Individual *Find(dynamic_array<Individual> &individuals, ISO_ptr<anything> p) {
	return p && *p ? find(individuals, (*p)[0]) : 0;
}

Individual *Find(dynamic_array<Individual> &individuals, const anything *a, tag2 id) {
	return Find(individuals, Individual::Find(a, id));
}

void SetupRelationships(dynamic_array<Individual> &individuals) {
	for (bool more = true; more;) {
		more = false;
		for (auto &i : individuals) {
			if (!i.mother || !i.father) {
				//ISO_ASSERT(!i.name.begins("Mary"));
				if (ISO_ptr<void> a = i.Find(tag2("FAMC"))) {
					Individual *husb = Find(individuals, a, tag2("HUSB"));
					Individual *wife = Find(individuals, a, tag2("WIFE"));
					if (husb && wife) {
						husb->AddSpouse(wife);
						wife->AddSpouse(husb);
					}
					if (husb && !i.father) {
						i.father		= husb;
						i.same_father	= husb->child;
						husb->child		= &i;
						more			= true;
					}
					if (wife && !i.mother) {
						i.mother		= wife;
						i.same_mother	= wife->child;
						wife->child		= &i;
						more			= true;
					}
				} else if (ISO_ptr<void> a = i.Find(tag2("FAMS"))) {
					Individual *husb = 0, *wife = 0;
					for (auto &&j : ISO::Browser(a)["FAM"]) {
						if (j.GetName() == "HUSB") {
							husb = Find(individuals, j);

						} else if (j.GetName() == "WIFE") {
							wife = Find(individuals, j);

						} else if (j.GetName() == "CHIL") {
							Individual	*c = Find(individuals, j);
							if (husb && !c->father) {
								c->father		= husb;
								c->same_father	= husb->child;
								husb->child		= c;
								more			= true;
							}
							if (wife && !c->mother) {
								c->mother		= wife;
								c->same_mother	= wife->child;
								wife->child		= c;
								more			= true;
							}
						}
					}
					if (husb && wife) {
						husb->AddSpouse(wife);
						wife->AddSpouse(husb);
					}
				}
			}
		}
	}
}

static float	spacing			= 200;
static float	damping			= 0.75f;
static float	spouse_spring_k	= 1;
static float	child_spring_k	= 1;
position2 SetupTree(Individual *i, param(position2) pos);

void SetupTreeUp(Individual *i, param(position2) pos) {
	if (!i || i->set)
		return;

	SetupTree(i, pos);
//	i->SetPos(pos);
//	i->set = true;

//	SetupTree(i->spouse, pos + float2(100,0));

	SetupTreeUp(i->father, pos + float2{-spacing, -spacing});
	SetupTreeUp(i->mother, pos + float2{+spacing, -spacing});
}

position2 SetupTree(Individual *i, param(position2) pos) {
	if (!i || i->set)
		return pos;

	i->set_pos(pos + float2{random.to(spacing / 2), random.to(spacing / 2)});
	i->set = true;

	position2	pos2 = pos;
	for (auto s : i->spouses) {
		if (!s->set) {
			pos2 += float2{spacing, 0};
			SetupTreeUp(s, pos2);
		}
	}

	position2	end	= pos + float2{0,spacing};
	int		n	= i->NumChildren(), x = -n;
	for (Individual *c = i->child; c; c = c->mother == i ? c->same_mother : c->same_father, x += 2) {
		position2	bot = SetupTree(c, pos + float2{x * spacing, spacing});
		end.v.y = max(end.v.y, bot.v.y);
	}

	return end;
}

class ViewGED : public Window<ViewGED> {
	d2d::WND					d2d;
	d2d::Write					write;
	d2d::Font					font;
	ISO_ptr<anything>			ged;
	dynamic_array<Individual>	individuals;
	dynamic_array<Spring>		spouse_springs, child_springs;
	Individual					*selected;
	IsoEditor					&main;

	float	zoom;
	float2	pos;
	Point	prevmouse;
	Tree	tree;

	void	Paint(const Rect &r);
	void	Update(float dt);
public:
	LRESULT Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
		switch (message) {
			case WM_CREATE:
				d2d.Init(hWnd, GetClientRect().Size());
				SetTimer(1, .1f);
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
				selected	= (Individual*)tree.Find(position2(float2{float(prevmouse.x), float(prevmouse.y)} - pos) / zoom);
				if (selected)
					main.Select(selected->p);
				return 0;

			case WM_LBUTTONUP:
				if (selected) {
					tree.Remove(selected);
					tree.Insert(selected);
					selected = 0;
				}
				break;

			case WM_MOUSEMOVE:
				if (wParam & MK_LBUTTON) {
					Point	mouse(lParam);
					float2	move{float(mouse.x - prevmouse.x), float(mouse.y - prevmouse.y)};
					if (selected) {
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

			case WM_TIMER: {
				float	dt = 0.1f;
				Update(dt);
				Invalidate();
				SetTimer(1, dt);
				return 0;
			}
			case WM_NCDESTROY:
				delete this;
				return 0;
		}
		return Super(message, wParam, lParam);
	}

	Individual *Find(const anything *a, tag2 id) {
		ISO_ptr<void> p = Individual::Find(a, id);
		return p ? find(individuals, (*(anything*)p)[0]) : 0;
	}

	ViewGED(MainWindow &_main, const WindowPos &wpos, ISO_ptr<void> p);
};

ViewGED::ViewGED(MainWindow &_main, const WindowPos &wpos, ISO_ptr<void> p) : main((IsoEditor&)_main), zoom(1), font(write, L"Verdana", 10), selected(0), pos(zero) {
	ged = p;

	for (int i = 0, n = ged->Count(); i < n; i++) {
		if ((*ged)[i].IsID("INDI"))
			individuals.emplace_back(rectangle::with_length(position2(zero), float2{90, 70}), (*ged)[i]);
	}
	SetupRelationships(individuals);
	position2	pos(zero);
	for (auto &i : individuals) {
		if (!i.mother && !i.father)
			pos = SetupTree(&i, pos);
	}

	uint32		size	= individuals.size32();
	TreeNode	**leafs	= new TreeNode*[size];
	for (size_t i = 0; i < size; i++)
		leafs[i] = &individuals[i];

	auto	e = individuals.end();
	for (auto &i : individuals) {
		for (auto s : i.spouses)
			if (&i < s)
				spouse_springs.emplace_back(e, s);

		if (i.father || i.mother)
			child_springs.emplace_back(e, i.father, i.mother);
	}

	tree.Init(leafs, size);
	delete[] leafs;
	Create(wpos, NULL, CHILD | VISIBLE, CLIENTEDGE);
}


void ViewGED::Update(float dt) {
	Tree::intersections	in;
	tree.CollectIntersections(in, float2(200));
	for (auto &i : individuals) {
		i.set_pos(i.a + i.vel * dt);
		i.vel *= damping;
	}

	/*
	for (dynamic_array<Spring>::iterator i = spouse_springs.begin(), e = spouse_springs.end(); i != e; ++i) {
		float2		d	= i->a->centre() - i->b->centre();
		d.x				+= (d.x < 0).select(200, -200);
		d				*= spouse_spring_k;
		i->a->vel		-= d;
		i->b->vel		+= d;
	}
	for (dynamic_array<Spring>::iterator i = child_springs.begin(), e = child_springs.end(); i != e; ++i) {
		float2		p	= i->b && i->c ? (i->b->centre() + i->c->centre()) / 2 : (i->b ? i->b : i->c)->centre();
		float2		d	= (i->a->centre() - p - float2(0, 200)) * float2(child_spring_k / 2, child_spring_k);
		i->a->vel		-= d;
		if (i->b && i->c) {
			i->b->vel	+= d * half;
			i->c->vel	+= d * half;
		} else {
			(i->b ? i->b : i->c)->vel += d;
		}
	}
	*/
	for (auto &i : in) {
		Individual	*a = (Individual*)i.a;
		Individual	*b = (Individual*)i.b;
		float2		d = a->centre() - b->centre();
		float		m	= max(float(len2(d)), 0.0001f);
		d	= d / m * 4000;
		a->vel += d;
		b->vel -= d;
	}

	size_t		size	= individuals.size();
	TreeNode	**leafs	= new TreeNode*[size];
	for (size_t i = 0; i < size; i++)
		leafs[i] = &individuals[i];

	tree.Init(leafs, (int)size);
}


void ViewGED::Paint(const Rect &r) {

	d2d.Clear(colour(1,1,1));

	d2d::SolidBrush	white(d2d, colour(1,1,1)), black(d2d, colour(0,0,0));
	com_ptr<ID2D1SolidColorBrush>	sex_brushes[3];
	d2d.CreateBrush(&sex_brushes[0], colour(0,1,0));
	d2d.CreateBrush(&sex_brushes[1], colour(.5f,.5f,1));
	d2d.CreateBrush(&sex_brushes[2], colour(1,.5f,.5f));

	d2d.SetTransform(translate(pos.x, pos.y) * scale(zoom));

	rectangle	client = (rectangle(position2(r.Left(), r.Top()), position2(r.Right(), r.Bottom())) - pos) / zoom;

	for (auto it = tree.begin(); it; ) {
		if (!overlap(*it, client)) {
			it.skip();
			continue;
		}

		if (it->IsLeaf()) {
			Individual	*i = (Individual*)(TreeNode*)it;
			d2d.Fill(*i, sex_brushes[i->sex]);
			d2d.DrawText(*i, i->name, font, white);

			for (auto s : i->spouses) {
				if (s->a.v.x > i->a.v.x) {
					d2d.DrawLine(
						float2{i->b.v.x, i->centre().v.y},
						float2{s->a.v.x, s->centre().v.y},
						black
					);
				}
			}
			if (i->father) {
				d2d.DrawLine(
					float2{i->centre().v.x, i->a.v.y},
					float2{i->father->centre().v.x, i->father->b.v.y},
					sex_brushes[0]
				);
			}
		}

		++it;
	}
}

class EditorGED : public Editor {
	virtual bool Matches(const ISO::Type *type) {
		return type->Is("GED");
	}
	virtual Control Create(MainWindow &main, const WindowPos &pos, const ISO_ptr<void> &p) {
		return *new ViewGED(main, pos, p);
	}
} editorged;
