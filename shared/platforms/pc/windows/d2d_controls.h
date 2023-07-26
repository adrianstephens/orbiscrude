#ifndef D2D_CONTROLS_H
#define D2D_CONTROLS_H

//#include "window.h"
#include "d2d.h"
//#include "base/array.h"

namespace iso { namespace d2d {

#if 0
//-----------------------------------------------------------------------------
//	TabWindow
//-----------------------------------------------------------------------------
class TabWindow : public win::Window<TabWindow> {
	dynamic_array<win::Control>	controls;
	int				selected;
public:
	LRESULT			Proc(UINT message, WPARAM wParam, LPARAM lParam);

	int				Count()						const	{ return controls.size(); }

	bool			RemoveItem(int i)					{ controls.erase(controls + i); }
	bool			SetSelectedIndex(int i)				{ selected = i;	}
	int				GetSelectedIndex()			const	{ return selected; }

	win::Rect		GetItemRect(int i)			const;

	win::Rect&		AdjustRect(win::Rect &rect, bool client = false) const;
	win::Rect		GetChildRect()				const;
	win::WindowPos	GetChildWindowPos()			const;
	win::Rect		GetDisplayRect()			const;
	int				HitTest(const POINT &pt)	const;
};
#endif

struct RenderRegion {
	float2x3	matrix;
	float2		size;

	RenderRegion(const float2x3 &matrix, float2 size) : matrix(matrix), size(size) {}
};


} } // namespace iso::d2d

#endif D2D_CONTROLS_H
