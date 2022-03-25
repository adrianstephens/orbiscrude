enum KeyType				{ key_unknown, key_rsa, key_dsa, key_dh, key_ec,};

template<typename T> struct KeyTypeT : T_constant0<KeyType, key_unknown> {};

struct Key {
	KeyType		type;
	const void	*p;
	Key() : type(key_unknown), p(0) {}
	template<typename T> Key(const T *t) : type(KeyTypeT<T>::value), p(t) {}
	void	set(KeyType _type, const void *_p) {
		type	= _type;
		p		= _p;
	}
	template<typename T> operator const T*() {
		ISO_ASSERT(KeyTypeT<T>::value == type);
		return (const T*)p;
	}
	template<typename T> operator T*() {
		ISO_ASSERT(KeyTypeT<T>::value == type);
		return (T*)p;
	}
};

//-----------------------------------------------------------------------------
//	Keys
//-----------------------------------------------------------------------------

template<> struct KeyTypeT<iso::DH> : T_constant0<KeyType, key_dh> {};
template<> struct KeyTypeT<iso::DSA> : T_constant0<KeyType, key_dsa> {};
template<> struct KeyTypeT<iso::RSA> : T_constant0<KeyType, key_rsa> {};
template<> struct KeyTypeT<iso::EC> : T_constant0<KeyType, key_ec> {};


KeyType get_key_type(const char *id) {
	static struct { const char *name; KeyType type; } ids[] = {
		{"rsaEncryption",		key_rsa	},
		{"rsa",					key_rsa	},
		{"dsaEncryption",		key_dsa	},
		{"dsaEncryption-old",	key_dsa	},
		{"dsaWithSHA",			key_dsa	},
		{"dsaWithSHA1",			key_dsa	},
		{"dsaWithSHA1-old",		key_dsa	},
		{"dhKeyAgreement",		key_dh	},
//		{"X9.42 DH",			key_dhx	},
		{"id-ecPublicKey",		key_ec	},
	};
	for (auto &i : ids) {
		if (i.name == id)
			return i.type;
	}
	return key_unknown;
}

KeyType get_key_type(const ASN1::ResolvedObjectID &id) {
	return get_key_type(id.name);
}

KeyType get_key_type(const ASN1::ObjectID &id) {
	if (const ASN1::OID *oid = lookup(id))
		return get_key_type(oid->name);
	return key_unknown;
}

struct KeyContext {
	void	*ctx;
	Key		key;		// may be NULL
	Key		peer_key;	// Peer key for key agreement, may be NULL

	void	(*_delete)(void*);
	bool	(*_sign_init)(void*);
	bool	(*_sign)(void*, const memory_block &sig, const const_memory_block &tbs);
	bool	(*_verify_init)(void*);
	bool	(*_verify)(void*, const const_memory_block &sig, const const_memory_block &tbs);
	bool	(*_verify_recover_init)(void*);
	bool	(*_verify_recover)(void*, const memory_block &rout, const const_memory_block &sig);
	bool	(*_encrypt_init)(void*);
	bool	(*_encrypt)(void*, const memory_block &out, const const_memory_block &in);
	bool	(*_decrypt_init)(void*);
	bool	(*_decrypt)(void*, const memory_block &out, const const_memory_block &in);
	bool	(*_derive_init)(void*);
	uint32	(*_derive)(void*, uint8 *result, Key key, Key peer_key);
	bool	(*_paramgen_init)(void*);
	Key		(*_paramgen)(void*);
	bool	(*_keygen_init)(void*);
	Key		(*_keygen)(void*, Key key);

	template<typename T> void set_ctx(T *t) {
		ctx	= t;
		_delete					= (void(*)(void*))&deleter<T>;
		_sign_init				= make_function(&T::sign_init);
		_sign					= make_function(&T::sign);
		_verify_init			= make_function(&T::verify_init);
		_verify					= make_function(&T::verify);
		_verify_recover_init	= make_function(&T::verify_recover_init);
		_verify_recover			= make_function(&T::verify_recover);
		_encrypt_init			= make_function(&T::encrypt_init);
		_encrypt				= make_function(&T::encrypt);
		_decrypt_init			= make_function(&T::decrypt_init);
		_decrypt				= make_function(&T::decrypt);
		_derive_init			= make_function(&T::derive_init);
		_derive					= make_function(&T::derive);
		_paramgen_init			= make_function(&T::paramgen_init);
		_paramgen				= make_function(&T::paramgen);
		_keygen_init			= make_function(&T::keygen_init);
		_keygen					= make_function(&T::keygen);
	}
	void	set_ctx(KeyType type);

	bool	sign_init()																{ return _sign_init(ctx); }
	bool	sign(const memory_block &sig, const const_memory_block &tbs)			{ return _sign(ctx, sig, tbs); }
	bool	verify_init()															{ return _verify_init(ctx); }
	bool	verify(const const_memory_block &sig, const const_memory_block &tbs)	{ return _verify(ctx, sig, tbs); }
	bool	verify_recover_init()													{ return _verify_recover_init(ctx); }
	bool	verify_recover(const memory_block &rout, const const_memory_block &sig)	{ return _verify_recover(ctx, rout, sig); }
	bool	encrypt_init()															{ return _encrypt_init(ctx); }
	bool	encrypt(const memory_block &out, const const_memory_block &in)			{ return _encrypt(ctx, out, in); }
	bool	decrypt_init()															{ return _decrypt_init(ctx); }
	bool	decrypt(const memory_block &out, const const_memory_block &in)			{ return _decrypt(ctx, out, in); }
	bool	derive_init()															{ return _derive_init(ctx); }
	uint32	derive(uint8 *result)													{ return _derive(ctx, result, key, peer_key); }
	bool	paramgen_init()															{ return _paramgen_init(ctx); }
	Key		paramgen()																{ return _paramgen(ctx); }
	bool	keygen_init()															{ return _keygen_init(ctx); };
	Key		keygen()																{ return _keygen(ctx, key); }

	malloc_block	derive() {
		uint32	len = derive(0);
		malloc_block	m(len);
		derive(m);
		return m;
	}

	KeyContext() : ctx(0) {}
	KeyContext(const Key &k) : key(k)	{ set_ctx(k.type); }
	KeyContext(KeyType type)			{ set_ctx(type); }
	KeyContext(const X509::Key *k)	{
		KeyType		type	= get_key_type(k->Type);
		set_ctx(type);
		key.set(type, k->Value.get_ptr());
	}

	~KeyContext() { _delete(ctx); }

	bool	set_peer(Key peer) {
		peer_key = peer;
		return true;
	}
	bool	set_peer(const X509::Key *peer) {
		peer_key.set(get_key_type(peer->Type), peer->Value.get_ptr());
		return true;
	}

	Key	new_mac_key(KeyType type, const const_memory_block &key) {
		KeyContext	mac_ctx(type);
		mac_ctx.keygen_init();
//		mac_ctx->set_mac_key(key);
		return mac_ctx.keygen();
	}

};

struct KeyContextDefaults {
	bool	sign_init()																{ return false; }
	bool	sign(const memory_block &sig, const const_memory_block &tbs)			{ return false; }
	bool	verify_init()															{ return false; }
	bool	verify(const const_memory_block &sig, const const_memory_block &tbs)	{ return false; }
	bool	verify_recover_init()													{ return false; }
	bool	verify_recover(const memory_block &rout, const const_memory_block &sig)	{ return false; }
	bool	encrypt_init()															{ return false; }
	bool	encrypt(const memory_block &out, const const_memory_block &in)			{ return false; }
	bool	decrypt_init()															{ return false; }
	bool	decrypt(const memory_block &out, const const_memory_block &in)			{ return false; }
	bool	derive_init()															{ return false; }
	bool	paramgen_init()															{ return false; }
	bool	keygen_init()															{ return false; }
};

struct KeyContextRSA	: RSAContext, KeyContextDefaults {
};
struct KeyContextDSA	: DSAContext, KeyContextDefaults {
	uint32	derive(uint8 *result, Key key, Key peer)								{ return 0; }
	Key		paramgen()																{ return Key((void*)0); }
	Key		keygen(Key key)															{ return Key((void*)0); }
};
struct KeyContextDH		: DHContext, KeyContextDefaults {
};
struct KeyContextEC		: ECContext, KeyContextDefaults {
	uint32	derive(uint8 *result, Key key, Key peer)								{ return 0; }
	Key		paramgen()																{ return Key((void*)0); }
	Key		keygen(Key key)															{ return Key((void*)0); }
};

void KeyContext::set_ctx(KeyType type) {
	switch (type) {
		case key_rsa:	set_ctx(new KeyContextRSA); break;
		case key_dsa:	set_ctx(new KeyContextDSA); break;
		case key_dh:	set_ctx(new KeyContextDH); break;
		case key_ec:	set_ctx(new KeyContextEC); break;
	};
}
