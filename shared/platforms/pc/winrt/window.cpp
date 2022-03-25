#include "window.h"

namespace iso { namespace win {

struct TREEITEM : TreeControl::ItemData {
};

HTREEITEM TVI_ROOT, TVI_FIRST, TVI_LAST;

TreeControl::TreeControl() {}
	
HTREEITEM	TreeControl::GetParentItem(HTREEITEM h) {
	return TVI_ROOT;
}
HTREEITEM	TreeControl::GetChildItem(HTREEITEM h) {
	return TVI_ROOT;
}
HTREEITEM	TreeControl::GetNextItem(HTREEITEM h) {
	return TVI_ROOT;
}
arbitrary	TreeControl::GetItemParam(HTREEITEM h) {
	return h->param;
}
bool		TreeControl::GetItem(HTREEITEM h, ItemData *i) {
	*i = *h;
	return true;
}
bool		TreeControl::SetItem(HTREEITEM h, const ItemData *i) {
//	*h = *i;
	return true;
}
bool		TreeControl::GetItem(Item *i) {
	*(ItemData*)i = *i->hItem;
	return true;
}
bool		TreeControl::SetItem(const Item *i) {
	*(ItemData*)i->hItem = *i;
	return true;
}
TreeControl::Item TreeControl::GetItem(HTREEITEM h) {
	TreeControl::Item	i(h);
	GetItem(h, &i);
	return i;
}
HTREEITEM	TreeControl::Insert(const Item *i, HTREEITEM parent, HTREEITEM after) {
	return 0;
}
HTREEITEM	TreeControl::InsertItem(const char *name, HTREEITEM parent, HTREEITEM after, arbitrary param, int children) {
	return 0;
}
bool		TreeControl::DeleteItem(HTREEITEM h) {
	return false;
}
bool		TreeControl::SetSelectedItem(HTREEITEM h) {
	return false;
}
bool		TreeControl::ExpandItem(HTREEITEM h) const {
	return false;
}
bool		TreeControl::ExpandedOnce(HTREEITEM h) const {
	return false;
}
bool		TreeControl::EnsureVisible(HTREEITEM hItem) const {
	return false;
}
HTREEITEM	TreeControl::GetSelectedItem() const {
	return 0;
}
int			TreeControl::FindChildIndex(HTREEITEM hParent, HTREEITEM hChild) const {
	return 0;
}
HTREEITEM	TreeControl::GetChildItem(HTREEITEM hParent, int i) const {
	return 0;
}
void		TreeControl::DeleteChildren(HTREEITEM hItem) {
	while (HTREEITEM hChild = GetChildItem(hItem))
		DeleteItem(hChild);
}

} } // namespace iso::win

