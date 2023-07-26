#include "plist.h"

#include "codec/base64.h"
#include "hashes/SHA.h"
#include "filetypes/bin.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	binary plist
//-----------------------------------------------------------------------------

uint64 bplist::get_int(istream_ref file, int size) {
	uint64be	temp	= 0;
	file.readbuff((uint8*)(&temp + 1) - size, size);
	return temp;
}

uint64 bplist::get_int(istream_ref file) {
	int			c		= file.getc();
	ISO_ASSERT((c & 0xf0) == bplist::Int);
	return get_int(file, 1 << (c & 0xf));
}

#define ISO_ptr	ISO_ptr_machine

ISO_ptr_machine<void> bplist_reader::_get_element(streamptr pos, tag id) {
	file.seek(pos);
	int	c = file.getc();
	int	b = c & 0xf;

	switch (c & 0xf0) {
		case Null:
			switch (b) {
				case False:	return ISO_ptr<bool8>(id, false);
				case True:	return ISO_ptr<bool8>(id, true);
			}
			break;

		case Int: {
			uint8	buffer[16];
			file.readbuff(buffer, size_t(1) << b);
			switch (b) {
				case 0:		return ISO_ptr<uint8>(id,  buffer[0]);
				case 1:		return ISO_ptr<uint16>(id, *(uint16be*)buffer);
				case 2:		return ISO_ptr<uint32>(id, *(uint32be*)buffer);
				case 3:		return ISO_ptr<uint64>(id, *(uint64be*)buffer);
				default:	return ISO_NULL;
			}
		}

		case Real: {
			uint8	buffer[16];
			file.readbuff(buffer, size_t(1) << b);
			switch (b) {
				case 2:		return ISO_ptr<float32>(id, *(float32be*)buffer);
				case 3:		return ISO_ptr<float64>(id, *(float64be*)buffer);
				default:	return ISO_NULL;
			};
		}
		case Date & 0xf0:
			return ISO_ptr<DateTime>(id, DateTime(2001, 1, 1) + Duration::Secs(file.get<float64be>()));

		case Data: {
			if (b == 0xf)
				b = get_int(file);
		#if 0
			const ISO::Type *type = new ISO::TypeArray(ISO::getdef<uint8>(), b);
			ISO_ptr<void>	t = MakePtr(type, id);
			file.readbuff(t, b);
		#else
			ISO_ptr<malloc_block>	t(id, b);
			file.readbuff(*t, b);
		#endif
			return t;
		}
		case ASCIIString: {
			if (b == 0xf)
				b = get_int(file);
			ISO_ptr<string>	t(id, b);
			file.readbuff(t->begin(), b);
			return t;
		}
		case Unicode16String: {
			if (b == 0xf)
				b = get_int(file);

			ISO_ptr<string16>	t(id, b);
			copy(new_auto_init(char16be, b, file), t->begin());
			return t;
		}
		case UID: {
			if (b == 3)
				return ISO_NULL;
			auto	i = get_int(file, 1 << b);
			return get_element(objects[i], id);
/*			switch (b) {
				case 0: return MakePtr(id, file.get<xint8>());
				case 1: return MakePtr(id, file.get<xint16be>());
				case 3: return MakePtr(id, file.get<xint32be>());
				case 7: return MakePtr(id, file.get<xint64be>());
				case 15: return MakePtr(id, file.get<GUID>());
			}
			break;
			*/
		}
		case Set:
			ISO_ASSERT(0);
		case Array: {
			if (b == 0xf)
				b = get_int(file);

			uint64		*table2;
			if (id == "$objects")
				table2 = objects.resize(b);
			else
				table2 = alloc_auto(uint64, b);

			for (int i = 0; i < b; i++)
				table2[i] = get_int(file, objectRefSize);

			if (id == "$objects")
				break;

			ISO_ptr<plist_array>	t(id, int(b));
			for (int i = 0; i < b; i++)
				(*t)[i] = get_element(table2[i]);
			return t;
		}
		case Dict: {
			if (b == 0xf)
				b = get_int(file);

			uint64		*table2	= alloc_auto(uint64, b * 2);
			for (int i = 0; i < b * 2; i++)
				table2[i] = get_int(file, objectRefSize);

			ISO_ptr<plist_dictionary>	t(id, b);
			for (int i = 0; i < b; i++) {
				ISO_ptr<string>	a = get_element(table2[i]);
				if (a.IsType<string>())
					(*t)[i] = get_element(table2[i + b], *a);
				else
					(*t)[i] = ISO_ptr<pair<ISO_ptr<void>, ISO_ptr<void>>>(0, make_pair(a, get_element(table2[i + b])));
			}
			return t;
		}
	}
	return ISO_NULL;
}

#undef ISO_ptr

void bplist::put_int(ostream_ref file, int size, uint64 v) {
	uint64be	temp	= v;
	file.writebuff((uint8*)(&temp + 1) - size, size);
}

void bplist::put_int(ostream_ref file, uint64 v) {
	int		size = (v >> 8) == 0 ? 0 : (v >> 16) == 0 ? 1 : (v >> 32) == 0 ? 2 : 3;
	file.putc(bplist::Int | size);
	put_int(file, 1 << size, v);
}

void bplist::put_marker_length(ostream_ref file, Marker m, uint64 v) {
	if (v < 15) {
		file.putc(m | v);
	} else {
		file.putc(m | 0xf);
		put_int(file, v);
	}
}

void bplist::_put_element(ostream_ref file, const index &v) {
	uint32	size	= v.i ? highest_set_index(v.i) / 8 : 0;
	file.putc(UID | size);
	put_int(file, size + 1, v.i);
}

void bplist::_put_element(ostream_ref file, bool v) {
	file.putc(v ? True : False);
}
void bplist::_put_element(ostream_ref file, uint64 v) {
	put_int(file, v);
}
void bplist::_put_element(ostream_ref file, float v) {
	file.putc(Real | 2);
	file.write(float32be(v));
}
void bplist::_put_element(ostream_ref file, double v) {
	file.putc(Real | 3);
	file.write(float64be(v));
}
void bplist::_put_element(ostream_ref file, DateTime v) {
	file.putc(Date | 3);
	file.write(float64be((v - DateTime(2001, 1, 1)).fSecs()));
}
void bplist::_put_element(ostream_ref file, const const_memory_block &v) {
	put_marker_length(file, Data, v.length());
	file.writebuff(v, v.length());
}
void bplist::_put_element(ostream_ref file, const char *v) {
	put_marker_length(file, ASCIIString, string_len(v));
	file.writebuff(v, string_len(v));
}
void bplist::_put_element(ostream_ref file, const char16 *v) {
	put_marker_length(file, Unicode16String, string_len(v));
	file.writebuff(v, string_len(v) * 2);
}
void bplist::_put_element(ostream_ref file, const GUID &v) {
	file.putc(UID|0xf);
	file.write(v);
}

uint64 bplist_writer::put_array(void *data, const ISO::Type *type, size_t num) {
	if (num && type->GetType() == ISO::REFERENCE && ((ISO_ptr<void>*)data)->ID()) {
		_dictionary		d(*this);
		ISO_ptr_machine<void>	*a	= (ISO_ptr_machine<void>*)data;
		for (int i = 0; i < num; i++, a++)
			d.add(a->ID().get_tag(), *a);
		return d.i;

	} else {
		_array			d(*this);
		ISO_ptr_machine<void>	*a	= (ISO_ptr_machine<void>*)data;
		for (int i = 0; i < num; i++, a++)
			d.add(*a);
		return d.i;
	}
}

uint64 bplist_writer::add(ISO_ptr_machine<void> p) {
	const ISO::Type *type = p.GetType();
	for (;;) {
		switch (type->GetType()) {
			case ISO::INT: {
				uint64	v = uint64(((ISO::TypeInt*)type)->get64(p));
				return type->flags & ISO::TypeInt::HEX
					? add(index(v))
					: add(v);
			}
			case ISO::FLOAT:
				return type->Is<float>()
					? add(*(float*)p)
					: add(*(double*)p);

			case ISO::STRING:
				return ((ISO::TypeString*)type)->char_size() == 1
					? add((const char*)type->ReadPtr(p))
					: add((const char16*)type->ReadPtr(p));

			case ISO::ARRAY:
				return type->SubType()->IsPlainData()
					? add(const_memory_block(p, type->GetSize()))
					: put_array(p, type->SubType(), ((ISO::TypeArray*)type)->Count());

			case ISO::OPENARRAY: {
				ISO_openarray<void>	*a = p;
				return type->SubType()->IsPlainData()
					? add(const_memory_block(p, type->SubType()->GetSize() * a->Count()))
					: put_array(*a, type->SubType(), a->Count());
			}

			case ISO::REFERENCE:
				p		= *(ISO_ptr<void>*)p;
				type	= p.GetType();
				continue;

			case ISO::USER:
				if (type->Is<GUID>())
					return add(*(GUID*)p);

				if (type->Is<bool>())
					return add(*(bool*)p);

				if (type->Is<DateTime>())
					return add(*(DateTime*)p);

				if (type->Is<plist_array>()) {
					_array				d(*this);
					plist_array			*a	= p;
					uint64				x	= d.put_here(a->Count());
					for (auto &i : *a)
						d.add(i);
					return x;
				}

				if (type->Is<plist_dictionary>()) {
					_dictionary			d(*this);
					plist_dictionary	*a	= p;
					uint64				x	= d.put_here(a->Count());
					for (auto &i : *a)
						d.add_id(i.ID().get_tag());
					int	i0 = 0;
					for (auto &i : *a)
						d.set(i0++, i);
					return x;
				}

				if (type->Is("Bin")) {
					ISO::Browser2	b(p);
					void	*bin	= b[0];
					size_t	length	= b.Count() * b[0].GetSize();
					return add(const_memory_block(bin, length));
				}

				type = type->SubType();
				continue;

			default:
				return 0;
		}
	}
}

uint64 bplist_writer::count_array(void *data, const ISO::Type *type, size_t num) {
	uint64	n = 1;
	if (num && type->GetType() == ISO::REFERENCE && ((ISO_ptr<void>*)data)->ID()) {
		ISO_ptr<void>	*a	= (ISO_ptr<void>*)data;
		for (int i = 0; i < num; i++, a++)
			n += count_elements(*a) + 1;

	} else {
		ISO_ptr<void>	*a	= (ISO_ptr<void>*)data;
		for (int i = 0; i < num; i++, a++)
			n += count_elements(*a);
	}
	return n;
}

uint64 bplist_writer::count_elements(ISO_ptr_machine<void> p) {
	const ISO::Type *type = p.GetType();
	for (;;) {
		switch (type->GetType()) {
			case ISO::INT:
			case ISO::FLOAT:
			case ISO::STRING:
				return 1;

			case ISO::ARRAY:
				return type->SubType()->IsPlainData() ? 1 : count_array(p, type->SubType(), ((ISO::TypeArray*)type)->Count());

			case ISO::OPENARRAY: {
				ISO_openarray<void>	*a = p;
				return type->SubType()->IsPlainData() ? 1 : count_array(*a, type->SubType(), a->Count());
			}

			case ISO::REFERENCE:
				p		= *(ISO_ptr<void>*)p;
				type	= p.GetType();
				continue;

			case ISO::USER:
				if (type->Is<GUID>()
				|| type->Is<bool>()
				|| type->Is<DateTime>()
				|| type->Is("Bin")
				)
					return 1;

				if (type->Is<plist_array>()) {
					plist_array			*a	= p;
					uint64				n	= 1;
					for (auto &i : *a)
						n += count_elements(i);
					return n;
				}

				if (type->Is<plist_dictionary>()) {
					plist_dictionary	*a	= p;
					uint64				n	= 1;
					for (auto &i : *a)
						n += count_elements(i) + 1;
					return n;
				}

				type = type->SubType();
				continue;

			default:
				return 0;
		}
	}
}

//-----------------------------------------------------------------------------
//	ascii plist
//-----------------------------------------------------------------------------

ISO_ptr<void> PLISTreader::get_item(tag id) {
	int	c = skip_whitespace();

	switch (c) {
		case '(': {
			ISO_ptr<plist_array>	p(id);
			while ((c = skip_whitespace()) != ')') {
				put_back(c);
				p->Append(get_item(none));
				expect(',');
			}
			return p;
		}

		case '{': {
			ISO_ptr<plist_dictionary>	p(id);
			while ((c = skip_whitespace()) != '}') {
				string_builder	b;
				if (c == '"') {
					for (; (c = getc()) != '"';) {
						if (c == '\\')
							c = get_escape(*this);
						b << char32(c);
					}
				} else {
					if (!is_alphanum(c))
						throw_accum("Bad identifier at line " << line_number);

					{
						b << char32(c);
						while ((c = getc()) > ' ')
							b << char32(c);
					}
				}
				expect('=');

				p->Append(get_item(b.term()));
				expect(';');
			}
			return p;
		}

		case '"': {
			string_builder	b;
			while ((c = getc()) != '"') {
				if (c == '\\')
					c = get_escape(*this);
				b << char32(c);
			}
			return ISO_ptr<string>(id, move(b));
		}
#if 0
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
		case '-':
		case '+':
		{
			bool	neg = (c == '-');
			if (c == '-' || c == '+') {
				c = getc();
				if (!is_digit(c))
					throw_accum("Bad number at line " << line_number);
			}
			uint64	n = c - '0';
			for (;;) {
				c = getc();
				if (!is_digit(c))
					break;
				n = n * 10 + c - '0';
			}
			put_back(c);
			int	bits = highest_set_index(n - int(neg));
			if (neg) {
				int	bits = highest_set_index(n - 1);
				if (bits < 7)
					return ISO_ptr<int8>(id, -n);
				else if (bits < 15)
					return ISO_ptr<int16>(id, -n);
				else if (bits < 31)
					return ISO_ptr<int32>(id, -n);
				else
					return ISO_ptr<int64>(id, -n);
			} else {
				int	bits = highest_set_index(n);
				if (bits < 8)
					return ISO_ptr<uint8>(id, n);
				else if (bits < 16)
					return ISO_ptr<uint16>(id, n);
				else if (bits < 32)
					return ISO_ptr<uint32>(id, n);
				else
					return ISO_ptr<uint64>(id, n);
			}
		}
#endif
		default: {
			string_builder	b;
			do {
				b << char32(c);
				c = getc();
			} while (c > ' ' && c != ';' && c != ',');
			put_back(c);

			if (str(b) == "YES")
				return ISO_ptr<bool8>(id, true);
			if (str(b) == "NO")
				return ISO_ptr<bool8>(id, false);
			return ISO_ptr<string>(id, move(b));
		}
	}
}

void PLISTwriter::new_line() {
	putc('\n');
	for (int i = 0; i < indent; i++)
		putc('\t');
}

void PLISTwriter::put_item(ISO::Browser b) {
	if (b.GetType() == ISO::REFERENCE) {
		if (tag id = b.GetName()) {
			puts(id);
			return;
		}
		b = *b;
	}

	switch (b.GetType()) {
		case ISO::INT:
			puts(to_string(b.GetInt()));
			break;

		case ISO::FLOAT:
			puts(to_string(b.GetFloat()));
			break;

		case ISO::STRING: {
			static char_set legal = ~(char_set::alphanum + '_' + '.');
			const char *s = b.GetString();
			bool	quotes = !!string_find(s, legal);
			if (quotes)
				putc('"');
			puts(b.GetString());
			if (quotes)
				putc('"');
			break;
		}

		case ISO::OPENARRAY:
			putc('{');
			++indent;
			for (ISO::Browser::iterator i = b.begin(), e = b.end(); i != e; ++i) {
				new_line();
				puts(i.GetName().get_tag());
				puts(" = ");
				put_item(**i);
				putc(';');
			}
			--indent;
			new_line();
			putc('}');
			break;

		case ISO::USER:
			if (b.Is("array")) {
				putc('(');
				++indent;
				for (ISO::Browser::iterator i = b.begin(), e = b.end(); i != e; ++i) {
					new_line();
					put_item(*i);
					putc(',');
				}
				--indent;
				new_line();
				putc(')');
			} else {
				put_item(b.SkipUser());
			}
			break;

		default:
			ISO_ASSERT(0);
			break;
	}
}

//-----------------------------------------------------------------------------
//	xml plist
//-----------------------------------------------------------------------------

template<typename T> constexpr typename T_noref<T>::type& move2(T &&a)	{ return static_cast<typename T_noref<T>::type&>(a); }

ISO_ptr<void> XPLISTreader::get_value(XMLiterator &i, tag id) {
	if (data.Is("dict")) {
		ISO_ptr<anything>	p(id);
		for (i.Enter(); i.Next();) {
			if (!data.Is("key"))
				return ISO_NULL;
			tag	key = i.Content();
			i.Next();
			p->Append(get_value(i, key));
		}
		return p;

	} else if (data.Is("array")) {
		ISO_ptr<anything>	p(id);
		for (i.Enter(); i.Next();)
			p->Append(get_value(i, none));
		return p;

	} else if (data.Is("false")) {
		return ISO_ptr<bool8>(id, false);

	} else if (data.Is("true")) {
		return ISO_ptr<bool8>(id, true);

	} else if (data.Is("integer")) {
		return ISO_ptr<int>(id, from_string<int>(i.Content()));

	} else if (data.Is("string")) {
		return ISO_ptr<string>(id, i.Content());

	} else if (data.Is("data")) {
		if (auto content = i.Content())
			return ISO_ptr<ISO_openarray<uint8> >(id, make_range<uint8>(transcode(base64_decoder(), content.data())));

	} else if (data.Is("date")) {
		return ISO_ptr<DateTime>(id, ISO_8601(move2(string_scan(i.Content()))));

	}

	return ISO_NULL;
}

void XPLISTwriter::put_item(ISO::Browser b) {
	switch (b.GetType()) {
		case ISO::INT: {
			XMLelement	e(*this, "integer");
			ElementContent(to_string(b.GetInt()));
			break;
		}

		case ISO::FLOAT: {
			XMLelement	e(*this, "float");
			ElementContent(to_string(b.GetFloat()));
			break;
		}

		case ISO::STRING: {
			XMLelement	e(*this, "string");
			ElementContent(b.GetString());
			break;
		}

		case ISO::OPENARRAY:
			if (IsRawData(b.GetTypeDef())) {
				XMLelement	e(*this, "data");

				memory_block data = GetRawData(b);

				buffer_accum<256>	lf;
				lf << '\n' << repeat('\t', GetIndentation());
				const char *lf2 = lf.term();
				ElementContentVerbatim(lf2);
				ElementContentVerbatim(transcode(base64_encoder(76, lf2), data));

			} else if (b.GetName(0)) {
				XMLelement	e(*this, "dict");
				for (auto i : b) {
					tag id = i.GetName();
					{
						XMLelement	key(*this, "key");
						ElementContent(id);
					}
					put_item(i);
				}
			} else {
				XMLelement	e(*this, "array");
				for (auto i : b)
					put_item(i);
			}
			break;

		default:
			ISO_ASSERT(0);
			break;
	}
}

