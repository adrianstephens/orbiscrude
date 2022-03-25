#include "bencode.h"

using namespace iso;

void writenum(ostream_ref file, uint32 i) {
	int	p = 1;
	while (i >= p * 10)
		p *= 10;
	while (p) {
		file.putc(i / p + '0');
		i %= p;
		p /= 10;
	}
}

char *readstring(istream_ref file, int c) {
	if (c == 0)
		c = file.getc();

	if (c < '0' || c > '9')
		return NULL;

	int	i = c - '0';
	while ((c = file.getc()) != ':') {
		if ((c < '0' || c > '9') || (c == '0' && i == 0))
			return NULL;
		i = i * 10 + c - '0';
	}
	char	*bytes = (char*)iso::malloc(i + 1);
	if (file.readbuff(bytes, i) != i) {
		iso::free(bytes);
		return NULL;
	}
	bytes[i] = 0;
	return bytes;
}

class bencode_int : public bencode_item {
	int	i;
public:
	bencode_type	type()						{ return BENCODE_INT;	}

	bencode_int()		: i(0)	{}
	bencode_int(int _i) : i(_i)	{}

	bool			read(istream_ref file, int c) {
		i	= 0;
		c	= file.getc();

		bool	neg = c == '-';
		if (neg)
			c = file.getc();

		if (c == '0') {
			if (file.getc() != 'e')
				return false;
		}

		do {
			if (c < '0' || c > '9')
				return false;
			i = i * 10 + c - '0';
		} while ((c = file.getc()) != 'e');

		if (neg)
			i = -i;
		return true;
	}
	bool			write(ostream_ref file) {
		file.putc('i');
		int	i2 = i;
		if (i2 < 0) {
			file.putc('-');
			i2 = -i2;
		}
		writenum(file, i2);
		file.putc('e');
		return true;
	}
};


class bencode_bytes : public bencode_item {
	int		len;
	void	*data;
public:
	bencode_type	type()						{ return BENCODE_BYTES;	}
	int				count()						{ return len;			}

	bencode_bytes() : len(0), data(0)			{}
	bencode_bytes(int _len, void *_data) : len(_len), data(_data)	{}
	~bencode_bytes()							{ iso::free(data); }

	bool			read(istream_ref file, int c) {
		len = c - '0';
		while ((c = file.getc()) != ':') {
			if ((c < '0' || c > '9') || (c == '0' && len == 0))
				return false;
			len = len * 10 + c - '0';
		}
		data = iso::malloc(len);
		return file.readbuff(data, len) == len;
	}
	bool			write(ostream_ref file) {
		writenum(file, len);
		file.putc(':');
		file.writebuff(data, len);
		return true;
	}
};

class bencode_list : public bencode_item {
protected:
	int				len, maxlen;
	bencode_item	**list;

public:
	bencode_type	type()						{ return BENCODE_LIST;	}
	int				count()						{ return len;			}
	bencode_item*	operator[](int i)			{ return list[i];		}

	bencode_list() : len(0), maxlen(0), list(0)	{}
	~bencode_list() {
		for (int i = 0; i < len; i++)
			delete list[i];
		iso::free(list);
	}

	void	append(bencode_item *item) {
		if (len == maxlen) {
			maxlen	= max(16, maxlen * 2);
			list	= (bencode_item**)iso::realloc(list, sizeof(bencode_item*) * maxlen);
		}
		list[len++] = item;
	}

	bool			read(istream_ref file, int c) {
		while ((c = file.getc()) != 'e') {
			if (bencode_item *item = parse(file, c)) {
				append(item);
			} else {
				return false;
			}
		}
		return true;
	}
	bool			write(ostream_ref file) {
		file.putc('l');
		for (int i = 0; i < len; i++) {
			if (!list[i]->write(file))
				return false;
		}
		file.putc('e');
		return true;
	}
};

class bencode_dict : public bencode_item {
	int				len, maxlen;
	struct entry {
		char			*key;
		bencode_item	*value;
	} *entries;

public:
	bencode_type	type()						{ return BENCODE_DICT;	}
	int				count()						{ return len;			}
	bencode_item*	operator[](int i)			{ return entries[i].value;	}
	bencode_item*	operator[](const char *s)	{
		for (entry *first = entries, *last = first + len; first < last;) {
			entry *middle = first + ((last - first) >> 1);
			int	c = strcmp(s, middle->key);
			if (c < 0)
				last = middle;
			else if (c > 0)
				first = middle + 1;
			else
				return middle->value;
		}
		return 0;
	}

	bencode_dict() : len(0), maxlen(0), entries(0)	{}
	~bencode_dict() {
		for (int i = 0; i < len; i++) {
			iso::free(entries[i].key);
			delete entries[i].value;
		}
		iso::free(entries);
	}

	void	append(char *key, bencode_item *value) {
		if (len == maxlen) {
			maxlen	= max(16, maxlen * 2);
			entries	= (entry*)iso::realloc(entries, sizeof(entry) * maxlen);
		}
		entries[len].key	= key;
		entries[len].value	= value;
		len++;
	}

	bool			read(istream_ref file, int c) {
		while ((c = file.getc()) != 'e') {
			if (char *key = readstring(file, c)) {
				if (bencode_item *value = parse(file)) {
					append(key, value);
					continue;
				}
				iso::free(key);
			}
			return false;
		}
		return true;
	}
	bool			write(ostream_ref file) {
		file.putc('d');
		for (int i = 0; i < len; i++) {
			int	keylen = strlen(entries[i].key);
			writenum(file, keylen);
			file.putc(':');
			file.writebuff(entries[i].key, keylen);
			if (!entries[i].value->write(file))
				return false;
		}
		file.putc('e');
		return true;
	}
};

bencode_item *bencode_item::parse(istream_ref file, int c) {
	if (c == 0)
		c = file.getc();

	bencode_item	*i;
	switch (c) {
		case 'i':		i = new bencode_int;	break;
		case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
						i = new bencode_bytes;	break;
		case 'l':		i = new bencode_list;	break;
		case 'd':		i = new bencode_dict;	break;
		default:
			return NULL;
	}
	streamptr	t = file.tell();
	if (i->read(file, c))
		return i;
	delete i;
	return NULL;
}

#if 0
void TestBencode(const char *filename) {
	FileInput	file(filename);
	bencode_browser	b(bencode_item::parse(file));
	b.write(FileOutput("test.txt"));
	streamptr	end = file.tell();
	end = end;
}
#endif
