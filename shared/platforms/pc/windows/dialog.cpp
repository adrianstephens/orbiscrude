#include "dialog.h"

using namespace iso::win;

//-----------------------------------------------------------------------------
//	Dialogs
//-----------------------------------------------------------------------------

size_t DialogBoxCreator::CalcSize() const {
	size_t	total = sizeof(DialogBoxEx)	// inc. menu_id
		+ 2 // class_id
		+ (title.length() + 1) * 2;

	if (style & DS_SETFONT)
		total += sizeof(DialogBoxEx::Font) + str(font.Name()).length() * 2;

	total = align(total, 4);

	for (auto i = controls.begin(), e = controls.end(); i != e; ++i)
		total += i->CalcSize();
	return total;
}

void *DialogBoxCreator::Make(DialogBoxEx *db) const {
	db->ver			= 1;
	db->signature	= 0xffff;
	db->help_id		= help_id;
	db->ex_style	= ex_style;
	db->style		= style;
	db->num_items	= uint16(controls.size());
	db->rect		= rect;

	db->menu_id		= none;
	db->class_id	= none;
	string_copy(db->caption->begin(), title);

	if (style & DS_SETFONT) {
		DialogBoxEx::Font	&df = db->font;
		df.pointsize	= font.PointSize();
		df.weight		= font.Weight();
		df.italic		= font.Italic();
		string_copy(df.name.begin(), str(font.Name()));
	}

	DialogBoxEx::Control	*dc = db->controls;
	for (auto i = controls.begin(), e = controls.end(); i != e; ++i, dc = dc->next())
		i->Make(dc);
	return dc;
}

size_t DialogBoxCreator::Control2::CalcSize() const {
	return align(sizeof(DialogBoxEx::Control) + 2 + (title.length() + 1) * 2, 4);
}
void DialogBoxCreator::Control2::Make(DialogBoxEx::Control *c) const {
	c->help_id		= help_id;
	c->ex_style		= ex_style;
	c->style		= style;
	c->rect			= rect;
	c->id			= id;
	c->class_id		= type;
	c->title		= title;
	c->extra_bytes	= 0;
}
