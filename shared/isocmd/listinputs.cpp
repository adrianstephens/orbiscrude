#include "main.h"
#include "iso/iso_binary.h"
#include "scenegraph.h"
#include "extra/xml.h"
#include "filetypes/sound/sample.h"
#include "systems/mesh/model_iso.h"
#include "systems/conversion/channeluse.h"

using namespace isocmd;

void FindModels(dynamic_array<Model3*> &models, Node *node) {
	for (int i = 0, n = node->children.Count(); i < n; i++) {
		if (node->children[i].GetType() == ISO::getdef<Node>())
			FindModels(models, node->children[i]);
		else if (node->children[i].GetType() == ISO::getdef<Model3>())
			models.push_back(node->children[i]);
	}
}

static char get_alpha(const ChannelUse &cu) {
	static char	alpha[] = "RGB80-";
	return (cu.rc.a < 4 && !cu.analog[cu.rc.a] ? '1' : alpha[cu.rc.a < 4 ? cu.ch[cu.rc.a] : cu.rc.a]);
}

void ListXML(const Cache &cache, ostream_ref out) {
	XMLwriter	xml(out, true);
	if (const char *stylesheet = ISO::root("variables")["stylesheet"].GetString())
		out.write(format_string("<?xml-stylesheet type=\"text/xsl\" href=\"%s\"?>\n", stylesheet));
//	out.write("<?xml-stylesheet type=\"text/xsl\" href=\"file:///C:\\dev\\xracing\\tools\\report.xsl\"?>\n");

	int	curr_depth = 0;
	for (int i = 0, n = cache.count(); i < n; i++) {
		int			depth;
		const char *filename = cache.get(i, &depth);
		while (depth < curr_depth) {
			xml.ElementEnd("file");
			curr_depth--;
		}

		ISO_ptr<void>	p = FileHandler::CachedRead(filename, false);
		if (p.GetType() == ISO::getdef<bitmap>()) {
			bitmap	*bm = p;
			ChannelUse	cu(bm);
			xml.Element("texture")
				.Attribute("name",		filename)
				.Attribute("width",		bm->Width())
				.Attribute("height",	bm->Height())
				.Attribute("channels",	cu.nc)
				.Attribute("alpha",		get_alpha(cu))
				.Attribute("size",		p.UserInt());
		} else if (p.GetType() == ISO::getdef<sample>()) {
			sample	*sm = p;
			xml.Element("sample")
				.Attribute("name",		filename)
				.Attribute("channels",	sm->Channels())
				.Attribute("bits",		sm->Bits())
				.Attribute("samplerate",sm->Frequency())
				.Attribute("length",	format_string("%i (%fs", sm->Length(), sm->Length() / sm->Frequency()))
				.Attribute("looping",	(sm->Flags() & sample::LOOP)  ? "true" : "false")
				.Attribute("music",		(sm->Flags() & sample::MUSIC) ? "true" : "false");
		} else {
			xml.ElementBegin("file");
			xml.Attribute("name",		filename);
			if (p.GetType() == ISO::getdef<Scene>()) {
				dynamic_array<Model3*>	models;
				FindModels(models, ((Scene*)p)->root);
				if (int n = models.size32()) {
					for (int i = 0; i < n; i++) {
						Model3	*m = models[i];
						int		nt	= 0;
						int		nv	= 0;
						int		ns	= m->submeshes.Count();
						for (int s = 0; s < ns; s++) {
							SubMesh	*sm = (SubMesh*)(SubMeshBase*)m->submeshes[s];
							nt += sm->indices.Count();
							nv += ((ISO_openarray<char>*)sm->verts)->Count();
						}
						xml.Element("model")
							.Attribute("name",		((ISO::Value*)m - 1)->ID().get_tag())
							.Attribute("submeshes",	ns)
							.Attribute("triangles",	nt)
							.Attribute("vertices",	nv);
					}
				}
			}
			curr_depth = depth + 1;
		}
	}
	while (curr_depth) {
		xml.ElementEnd("file");
		curr_depth--;
	}
}

void ListCSV(const Cache &cache, text_writer<writer_intf> out) {
	for (int i = 0, n = cache.count(); i < n; i++) {
		int			depth;
		const char *filename = cache.get(i, &depth);
		ISO_ptr<void>	p = FileHandler::CachedRead(filename, false);
		if (p.GetType() == ISO::getdef<bitmap>()) {
			bitmap	*bm = p;
			ChannelUse	cu(bm);
			out << filename
				<< ", " << bm->Width()
				<< ", " << bm->Height()
				<< ", channels:" << cu.nc
				<< ", alpha:" << get_alpha(cu)
				<< ", " << p.UserInt()
				<< "\n";
		}
	}
	out << "total vram: " << vram_offset() << "\n";
}

void ListDEPS(const Cache &cache, const filename &fn1, const filename &fn2, text_writer<writer_intf> out) {
	const char *platform = ISO::root("variables")["exportfor"].GetString();
#if 0
	out << fn1 << ":\\\r\n  " << fn2;
#else
	out << fn2 << ':';
#endif
	for (int i = 0, n = cache.count(); i < n; i++) {
		int			depth;
		filename	fn = cache.get(i, &depth);
		if (platform)
			FileHandler::ModifiedFilenameExists(fn, platform);
		if (fn.exists()) {
			out << "\\\r\n  " << fn;
		}
	}
	out << "\r\n";
}

void ListTEXT(const Cache &cache, text_writer<writer_intf> out) {
	for (int i = 0, n = cache.count(); i < n; i++) {
		int			depth;
		const char *filename = cache.get(i, &depth);
		out << repeat("  ", depth) << filename << "\r\n";
	}
}


void ListExternals(const Cache &cache, const filename &fn, const filename &outfn) {

	create_dir(outfn.dir());
	FileOutput		out(outfn);
	if (!out.exists())
		throw_accum("Cannot create " << outfn);

	filename::ext_t	ext = outfn.ext();
	if (ext == ".d")
		ListDEPS(cache, outfn, fn, out);
	else if (ext == ".csv")
		ListCSV(cache, out);
	else if (ext == ".xml")
		ListXML(cache, out);
	else
		ListTEXT(cache, out);
}
