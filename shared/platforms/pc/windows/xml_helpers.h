#ifndef XML_HELPERS_H
#define XML_HELPERS_H

#include "WIN32_Helpers.h"
#include "defs.h"
#include <msxml.h>

using namespace iso;
//-----------------------------------------------------------------------------
//	XML
//-----------------------------------------------------------------------------

class XML_node;

class XML_attributes {
    com_ptr<IXMLDOMNamedNodeMap>	map;
public:
	XML_attributes(IXMLDOMNamedNodeMap *_map) : map(_map)	{}

	int			Count()	{
		long len;
		map->get_length(&len);
		return len;
	}

	XML_node	get(const SysString &s);
	XML_node	operator[](int i);
};

class XML_nodelist {
    com_ptr2<IXMLDOMNodeList>	list;
public:
	XML_nodelist()	{}
	XML_nodelist(IXMLDOMNodeList *_list) : list(_list)	{}

	int			Count()	{
		long len;
		list->get_length(&len);
		return len;
	}

	XML_node	operator[](int i);
};

class XML_node {
    com_ptr<IXMLDOMNode>		node;
public:
	XML_node(IXMLDOMNode *_node) : node(_node)	{}

	XML_attributes	attributes() {
		IXMLDOMNamedNodeMap	*map	= NULL;
		if (node)
			node->get_attributes(&map);
		return map;
	}

	XML_node		attribute(const SysString &s) {
		return attributes().get(s);
	}

	XML_node		get(const SysString &s) {
		IXMLDOMNode		*node2	= NULL;
		if (node)
			node->selectSingleNode(s, &node2);
		return node2;
	}

	XML_nodelist	operator[](const SysString &s) {
		IXMLDOMNodeList	*list	= NULL;
		if (node)
			node->selectNodes(s, &list);
		return list;
	}

	char*			name() {
		SysString	s;
		node->get_baseName(&s);
		return s;
	}

	char*			text() {
		SysString	s;
		node->get_text(&s);
		return s;
	}

	Variant			value() {
		Variant	v;
		node->get_nodeTypedValue(&v);
		return v;
	}

	operator BSTR()	{
		BSTR		bstr	= NULL;
		if (node)
			node->get_text(&bstr);
		return bstr;
	}

	operator IXMLDOMNode*()	{ return node; }
};

class XML_doc {
	com_ptr<IXMLDOMDocument>	dom;
public:
	XML_doc() {
		HRESULT			hr;
		hr = CoInitialize(NULL);
		hr = CoCreateInstance(CLSID_DOMDocument, NULL, CLSCTX_INPROC_SERVER, IID_IXMLDOMDocument, (void**)&dom);
	}

	XML_doc(BSTR xml) {
		HRESULT			hr;
		VARIANT_BOOL	status;
		hr = CoInitialize(NULL);
		hr = CoCreateInstance(CLSID_DOMDocument, NULL, CLSCTX_INPROC_SERVER, IID_IXMLDOMDocument, (void**)&dom);
//		hr = dom->put_validateOnParse(VARIANT_FALSE);
//		hr = dom->put_resolveExternals(VARIANT_FALSE);
		hr = dom->loadXML(xml, &status);
	}

	void	InitWrite() {
		IXMLDOMProcessingInstruction *pi;
		HRESULT	hr = dom->createProcessingInstruction(SysString(L"xml"), SysString(L"version='1.0'"), &pi);
	}

	bool	Load(const char *filename) {
		VARIANT_BOOL	status;
		dom->load(Variant(SysString(filename)), &status);
		return status == VARIANT_TRUE;
	}

	bool	Save(IStream &file) {
		return SUCCEEDED(dom->save(Variant(&file)));
	}

	char*	GetError() {
		HRESULT				hr;
		IXMLDOMParseError*	err;
		SysString			s;

		hr = dom->get_parseError(&err);
		hr = err->get_reason(&s);
		return s;
	}

	XML_node		get(const SysString &s) {
		IXMLDOMNode		*node	= NULL;
		if (dom)
			dom->selectSingleNode(s, &node);
		return node;
	}

	XML_nodelist	operator[](const SysString &s) {
		IXMLDOMNodeList	*list	= NULL;
		if (dom)
			dom->selectNodes(s, &list);
		return list;
	}

	bool		Append(XML_node &node) {
		com_ptr<IXMLDOMNode> newnode;
		if (dom)
			return SUCCEEDED(dom->appendChild(node, &newnode));
		return false;
	}

	XML_node	Create(const SysString &s) {
		IXMLDOMElement	*node	= NULL;
		HRESULT	hr = dom->createElement(s, &node);
		return node;
	}
};


inline XML_node XML_nodelist::operator[](int i)
{
	IXMLDOMNode	*node	= NULL;
	HRESULT		hr		= list->get_item(i, &node);
	return node;
}

inline XML_node XML_attributes::operator[](int i)
{
	IXMLDOMNode	*node	= NULL;
	HRESULT		hr		= map->get_item(i, &node);
	return node;
}

inline XML_node XML_attributes::get(const SysString &s)
{
	IXMLDOMNode	*node	= NULL;
	HRESULT		hr		= map->getNamedItem(s, &node);
	return node;
}


#endif XML_HELPERS_H
