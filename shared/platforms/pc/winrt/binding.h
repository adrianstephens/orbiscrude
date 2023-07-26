#pragma once

#include "base.h"
#include "winrt/Windows.UI.Xaml.Data.h"
#include "winrt/Windows.UI.Xaml.Markup.h"
#include "winrt/Windows.Foundation.Collections.h"
#include "base/hash.h"

#pragma comment(lib, "windowsapp")

namespace iso_winrt {

template<typename T> inline Windows::UI::Xaml::Interop::TypeName typeof() {
	return {get_class_name<T>(), Windows::UI::Xaml::Interop::TypeKind::Custom};
}

struct member {
	typedef object	getter_t(object_ref);
	typedef void	setter_t(object_ref, object_ref);

	const char		*name;
	hstring			type;
	getter_t*		getter;
	setter_t*		setter;

	member() : name(0) {}
	member(const char *_name, hstring_ref _type, getter_t *_getter, setter_t *_setter) : name(_name), type(_type), getter(_getter), setter(_setter) {}
};

template<typename T, typename = void> struct member_type {
	typedef T	type;
};
template<typename T> struct member_type<T, enable_if_t<is_class<T>>> {
	template<typename U> static U g(void(T::*p)(U));
	static T g(void*);
	typedef decltype(g(&T::operator=))	type;
};
template<typename T> struct member_type<T, void_t<decltype(&T::operator=)>> {
	template<typename U> static U g(void(T::*p)(U));
	static T g(void*);
	typedef decltype(g(&T::operator=))	type;
};
//template<typename C, typename T> struct member_type<T C::*>		: member_type<T> {};
//template<typename C, typename T> struct member_type<T (C::*)()>	: member_type<T> {};

template<typename T> struct member_maker;

template<typename C, typename T, typename U> struct member_maker1 {
	template<T C::*p, T C::*q> static member make(const char *_name) {
		return member(
			_name, name<U>::value,
			[](object_ref obj) {
				return (object)U(ref_cast<ptr<C>>(obj)->*p);
			},
			[](object_ref obj, object_ref val) {
				ref_cast<ptr<C>>(obj)->*p = unbox<U>(val);
			}
		);
	}
};

template<typename C, typename T, typename U> struct member_maker2 {
	template<T (C::*p)(), void (C::*q)(U)> static member make(const char *_name) {
		return member(
			_name, name<T>::value,
			[](object_ref obj) {
				return (object)((ref_cast<ptr<C>>(obj)->*p)());
			},
			[](object_ref obj, object_ref val) {
				(ref_cast<ptr<C>>(obj)->*q)(unbox<T>(val)); ISO_ASSERT(0);
			}
		);
	}
};

template<typename C, typename T> struct member_maker<T C::*> {
	auto test_setter(T C::*q) {
		return *this;//member_maker1<C, T, typename member_type<T>::type>();
	}
	template<T C::*p, T C::*q> static member make(const char *_name) {
		return member(
			_name, name<T>::value,
			[](object_ref obj) {
				return (object)(ref_cast<ptr<C>>(obj)->*p);
			},
			[](object_ref obj, object_ref val) {
				ref_cast<ptr<C>>(obj)->*p = unbox<T>(val);
			}
		);
	}
	template<T C::*p, typename U> static member make(const char *_name, U*) {
		return member(
			_name, name<U>::value,
			[](object_ref obj) {
				return (object)U(ref_cast<ptr<C>>(obj)->*p);
			},
			[](object_ref obj, object_ref val) {
				ref_cast<ptr<C>>(obj)->*p = unbox<U>(val);
			}
		);
	}
	template<T C::*p, typename U> static member make(const char *_name, ptr<U>*) {
		return member(
			_name, name<U>::value,
			[](object_ref obj) {
				ptr<U>	r = ref_cast<ptr<C>>(obj)->*p;
				return (object)r;
			},
			[](object_ref obj, object_ref val) {
				ref_cast<ptr<C>>(obj)->*p = ref_cast<ptr<U>>(val);
			}
		);
	}
};

template<typename C, typename T> struct member_maker<T (C::*)()> {
	auto test_setter(...) { return *this; }
	template<typename U> static auto test_setter(void (C::*q)(U)) { 
		return member_maker2<C, T, U>();
	}
	template<T (C::*p)(), T (C::*q)()> static member make(const char *_name) {
		return member(
			_name, name<T>::value,
			[](object_ref obj) {
				return (object)((ref_cast<ptr<C>>(obj)->*p)());
			},
			0
		);
	}
};

template<typename C, typename T> struct member_maker<const T C::*> {
	template<T C::*p> static member make(const char *_name) {
		return member(
			_name, name<T>::value,
			[](object_ref obj) {
				return (object)(ref_cast<ptr<C>>(obj)->*p);
			},
			0
		);
	}
};

template<typename C, typename T> member_maker<T C::*>		get_member_maker(T C::*t)		{ return member_maker<T C::*>(); }
template<typename C, typename T> member_maker<T (C::*)()>	get_member_maker(T (C::*t)())	{ return member_maker<T (C::*)()>(); }
#define make_member(name, X)		get_member_maker(X).test_setter(X).make<X, X>(name)
#define make_member2(name, X, T)	get_member_maker(X).make<X>(name, (T*)0)

template<typename T> struct members { static member a[]; };
template<typename T> member members<T>::a[1];

struct type_id : static_list<type_id> {
	enum {
		Bindable	= 1,
	};
	hstring			name;
	uint32			flags;
	IInspectable*	(*activator)();
	hstring			base;
	member*			members;
	size_t			num_members;

	const member* find_member(const char *name) const {
		for (auto i = members; i != members + num_members; ++i) {
			if (str(i->name) == name)
				return i;
		}
		return nullptr;
	}

	template<typename M> type_id(hstring_ref _name, uint32 _flags, IInspectable* (*_activator)(), hstring_ref _base, M _members) : name(_name), flags(_flags), activator(_activator), base(_base), members(0), num_members(0) {}
	template<int N> type_id(hstring_ref _name, uint32 _flags, IInspectable* (*_activator)(), hstring_ref _base, member (&_members)[N]) : name(_name), flags(_flags), activator(_activator), base(_base), members(_members), num_members(N) {}

	static const type_id* find(const hstring &name) {
		for (auto &i : all()) {
			if (i.name == name)
				return &i;
		}
		return nullptr;
	}
};

template<typename T, typename V = void> struct activator {
	typedef IInspectable*	(*type)();
	static constexpr type value() { return nullptr; }
};
template<typename T> struct activator<T, void_t<decltype(new T)>> {
	typedef IInspectable*	(*type)();
	static constexpr type value() { return []() { return query<IInspectable>(new T); }; }
};

template<typename T, typename V = void> struct base_name {
	static constexpr auto value = nullptr;
};
template<typename T> struct base_name<T, void_t<typename T::base_type>> : name<typename T::base_type> {};

template<typename T> struct type_id_t {
	static	type_id	_type_id;
};

template<typename T> type_id type_id_t<T>::_type_id(
	name<T>::value,
	type_id::Bindable,
	activator<T>::value(),
	base_name<T>::value,
	members<T>::a
);

}

namespace XamlBindingInfo {
	using namespace iso_winrt;
	using namespace Platform;
	using namespace Collections;
	using namespace Windows::UI::Xaml;

	class XamlBindings;

	class IXamlBindings {
	public:
		virtual ~IXamlBindings() {};
		virtual bool	IsInitialized() = 0;
		virtual void	Update() = 0;
		virtual bool	SetDataRoot(ptr<Object> data) = 0;
		virtual void	StopTracking() = 0;
		virtual void	Connect(int connectionId, Object* target) = 0;
		virtual void	ResetTemplate() = 0;
		virtual int		ProcessBindings(Controls::ContainerContentChangingEventArgs* args) = 0;
		virtual void	SubscribeForDataContextChanged(FrameworkElement* object, XamlBindings* handler) = 0;
	};

	class IXamlBindingTracking {
	public:
		virtual void PropertyChanged(ptr<Object> sender, ptr<Data::PropertyChangedEventArgs> e) = 0;
		virtual void CollectionChanged(ptr<Object> sender, ptr<Interop::NotifyCollectionChangedEventArgs> e) = 0;
		virtual void DependencyPropertyChanged(ptr<DependencyObject> sender, ptr<Windows::UI::Xaml::DependencyProperty> prop) = 0;
		virtual void VectorChanged(ptr<Object> sender, ptr<IVectorChangedEventArgs> e) = 0;
		virtual void MapChanged(ptr<Object> sender, ptr<IMapChangedEventArgs<ptr<String>>> e) = 0;
	};

	class XamlBindings : public runtime<XamlBindings, IDataTemplateExtension, Markup::IComponentConnector> {
		IXamlBindings* _pBindings;
	internal:
		XamlBindings(IXamlBindings* pBindings) : _pBindings(pBindings) {}
		~XamlBindings()			{ delete _pBindings; }

		void Initialize()		{ if (_pBindings->IsInitialized()) _pBindings->Update(); }
		void Update()			{ _pBindings->Update(); }
		void StopTracking()		{ _pBindings->StopTracking(); }
		
		void Loading(ptr<FrameworkElement> src, ptr<Object> data) {
			Initialize();
		}
		void DataContextChanged(ptr<FrameworkElement> sender, ptr<DataContextChangedEventArgs> args) {
			if (_pBindings->SetDataRoot(args->NewValue))
				Update();
		}
		void SubscribeForDataContextChanged(ptr<FrameworkElement> object) {
			_pBindings->SubscribeForDataContextChanged(object, this);
		}

	public:
		// IComponentConnector
		virtual void Connect(int connectionId, ptr<Object> target) {
			_pBindings->Connect(connectionId, target);
		}

		// IDataTemplateExtension
		virtual bool ProcessBinding(unsigned int) { throw NotImplementedException(); }
		virtual int ProcessBindings(ptr<Controls::ContainerContentChangingEventArgs> args) {
			return _pBindings->ProcessBindings(args);
		}
		virtual void ResetTemplate() {
			_pBindings->ResetTemplate();
		}
		virtual void DisconnectUnloadedObject(int connectionId) {
			//_pBindings->DisconnectUnloadedObject(connectionId);
		}
	};

	template<class T> class XamlBindingsBase : public IXamlBindings {
	protected:
		static const int NOT_PHASED = (1 << 31), DATA_CHANGED = (1 << 30);
		bool	_isInitialized;
		ptr<T>	_bindingsTracking;
		Windows::Foundation::EventRegistrationToken _dataContextChangedToken;

		XamlBindingsBase() : _isInitialized(false) { _dataContextChangedToken.Value = 0; }
		virtual ~XamlBindingsBase() {
			if (_bindingsTracking != nullptr) {
				_bindingsTracking->SetListener(nullptr);
				_bindingsTracking = nullptr;
			}
		}
		virtual void ReleaseAllListeners() {
			// Overridden in the binding class as needed.
		}

	public:
		virtual void Update() = 0;
		virtual void Connect(int connectionId, ptr<Object> target) = 0;

		void InitializeTracking(IXamlBindingTracking* pBindingsTracking) {
			_bindingsTracking = ref_new<T>();
			_bindingsTracking->SetListener(pBindingsTracking);
		}
		virtual void StopTracking() override {
			ReleaseAllListeners();
			_isInitialized = false;
		}
		virtual bool IsInitialized() override {
			return _isInitialized;
		}
		void SubscribeForDataContextChanged(ptr<Windows::UI::Xaml::FrameworkElement> object, ptr<XamlBindings> handler) {
			dataContextChangedToken = object->DataContextChanged += {this, DataContextChanged};
		}
		virtual void Recycle() {
			// Overridden in the binding class as needed.
		}
		virtual void ProcessBindings(ptr<Object>, int, int, int* nextPhase) {
			// Overridden in the binding class as needed.
			*nextPhase = -1;
		}
	};

	class XamlBindingTrackingBase {
	internal:
		XamlBindingTrackingBase();
		void SetListener(IXamlBindingTracking* pBindings);

		// Event handlers
		void PropertyChanged(ptr<Object> sender, ptr<Data::PropertyChangedEventArgs> e);
		void CollectionChanged(ptr<Object> sender, ptr<Interop::NotifyCollectionChangedEventArgs> e);
		void DependencyPropertyChanged(ptr<DependencyObject> sender, ptr<Windows::UI::Xaml::DependencyProperty> prop);
		void VectorChanged(ptr<Object> sender, ptr<IVectorChangedEventArgs> e);
		void MapChanged(ptr<Object> sender, ptr<IMapChangedEventArgs<ptr<String>>> e);

		// Listener update functions
		void UpdatePropertyChangedListener(ptr<Data::INotifyPropertyChanged> obj, ptr<Data::INotifyPropertyChanged>* pCache, Windows::Foundation::EventRegistrationToken* pToken);
		void UpdatePropertyChangedListener(ptr<Data::INotifyPropertyChanged> obj, WeakReference& cacheRef, Windows::Foundation::EventRegistrationToken* pToken);
		void UpdateCollectionChangedListener(ptr<Interop::INotifyCollectionChanged> obj, ptr<Interop::INotifyCollectionChanged>* pCache, Windows::Foundation::EventRegistrationToken* pToken);
		void UpdateDependencyPropertyChangedListener(ptr<DependencyObject> obj, ptr<Windows::UI::Xaml::DependencyProperty> property, ptr<DependencyObject>* pCache, __int64* pToken);
		void UpdateDependencyPropertyChangedListener(ptr<DependencyObject> obj, ptr<Windows::UI::Xaml::DependencyProperty> property, WeakReference& cacheRef, __int64* pToken);

	private:
		IXamlBindingTracking* _pBindingsTrackingWeakRef = nullptr;
	};
}

namespace XamlTypeInfo {
	using namespace iso_winrt;
	using namespace Platform;
	using namespace Windows::UI::Xaml;
	using namespace Markup;

	class Provider;

	class XamlUserMember : public runtime<XamlUserMember, IXamlMember> {
		Provider				*provider;
		const member			*info;
	public:
		XamlUserMember(Provider* provider, const member *info) : provider(provider), info(info), IsAttachable(false), IsDependencyProperty(false) {}

		bool					IsAttachable, IsDependencyProperty;
		ptr<IXamlType>			TargetType;

		bool					IsReadOnly()	{ return !info->setter; }
		hstring					Name()			{ return str(info->name); }
		ptr<IXamlType>			Type();

		ptr<Object> GetValue(object_ref instance) {
			if (info->getter)
				return info->getter(instance);
			throw NullReferenceException();
		}

		void SetValue(object_ref instance, object_ref value) {
			if (info->setter)
				info->setter(instance, value);
			else
				throw NullReferenceException();
		}
	};

	class XamlUserType : public runtime<XamlUserType, IXamlType, IWeakReferenceSource> {
		ptr<weak_reference_imp>	weak;
		STDMETHODIMP GetWeakReference(IWeakReference **weakReference) {
			*weakReference	= weak.make(query<IInspectable>(this));
			return S_OK;
		}

		hash_map<hstring, hstring>	memberNames;
		hash_map<string16, object>	enumValues;
		hstring						contentPropertyName, itemTypeName, keyTypeName;
	public:
		XamlUserType(Provider* provider, hstring_ref fullName, pptr<IXamlType> baseType) :
			IsArray(false), IsMarkupExtension(false), IsBindable(false),
			FullName(fullName),
			BaseType(baseType),
			IsLocalType(false), IsEnum(false), IsReturnTypeStub(false),
			provider(provider)
		{}

		// --- Interface methods ----
		bool				IsArray, IsMarkupExtension, IsBindable;
		hstring				FullName;

		Interop::TypeName	UnderlyingType()	{ return {FullName, KindOfType}; }
		bool				IsCollection()		{ return CollectionAdd != nullptr; }
		bool				IsConstructible()	{ return Activator != nullptr; }
		bool				IsDictionary()		{ return DictionaryAdd != nullptr; }

		ptr<IXamlType>		BaseType;
		ptr<IXamlMember>	ContentProperty();
		ptr<IXamlType>		ItemType();
		ptr<IXamlType>		KeyType();
		ptr<IXamlMember>	GetMember(hstring_ref name);

		IInspectable*		ActivateInstance()									{ return Activator(); }
		object				CreateFromString(hstring_ref value)					{ return FromStringConverter(this, value); }
		void				AddToVector(object instance, object value)			{ CollectionAdd(instance, value); }
		void				AddToMap(object instance, object key, object value) { DictionaryAdd(instance, key, value); }
		void				RunInitializer()									{}

		// --- End of Interface methods

		bool				IsLocalType, IsEnum, IsReturnTypeStub;
		Provider			*provider;
		Interop::TypeKind	KindOfType;
		IInspectable*		(*Activator)();
		void				(*CollectionAdd)(Object* instance, Object* item);
		void				(*DictionaryAdd)(Object* instance, Object* key, Object* item);
		IInspectable*		(*FromStringConverter)(XamlUserType* userType, hstring_ref input);

		void				AddMemberName(hstring shortName)					{ memberNames[shortName] = FullName + L"." + shortName; }
		void				AddEnumValue(hstring_ref name, Object* value)		{ enumValues[name.raw()] = value; }
	};

	class XamlSystemMember : public runtime<XamlSystemMember, IXamlMember> {
		hstring						name;
		ptr<Data::ICustomProperty>	prop;
		bool	fix(object_ref instance) {
			if (!prop) {
				ptr<Data::ICustomPropertyProvider>	prov;
				GUID		guid	= {0xa8d489fb, 0xe3dd, 0x4a1f, {0x93, 0x75, 0xf2, 0x4f, 0x47, 0xc6, 0x16, 0x42}};
				HRESULT		hr		= instance->QueryInterface(guid, (void**)&prov);
				prop = prov->GetCustomProperty(name);
			}
			return !!prop;
		}
	public:
		XamlSystemMember(hstring_ref name) : name(name), IsAttachable(false), IsDependencyProperty(false) {}

		bool					IsAttachable, IsDependencyProperty, IsReadOnly;
		hstring					Name()			{ return name; }
		IXamlType*				Type()			{ return 0; }
		IXamlType*				TargetType()	{ return 0; }

		ptr<Object> GetValue(object_ref instance) {
			if (!fix(instance))
				throw NullReferenceException();
			return prop->GetValue(instance);
		}
		void SetValue(object_ref instance, object_ref value) {
			if (!fix(instance))
				throw NullReferenceException();
			prop->SetValue(instance, value);
		}
	};

	class XamlSystemType : public runtime<XamlSystemType, IXamlType, IWeakReferenceSource> {
		ptr<weak_reference_imp>	weak;
		STDMETHODIMP GetWeakReference(IWeakReference **weakReference) {
			*weakReference	= weak.make(query<IInspectable>(this));
			return S_OK;
		}
	public:
		XamlSystemType(Interop::TypeName name) : UnderlyingType(name) {}

		Interop::TypeName	UnderlyingType;
		hstring				FullName()								{ return UnderlyingType.Name; }
		IXamlType*			BaseType()								{ return 0; }//throw NotImplementedException(); }
		IXamlMember*		ContentProperty()						{ return new XamlSystemMember(L"Content"); }
		bool				IsArray()								{ return false; }
		bool				IsCollection()							{ return false; }
		bool				IsConstructible()						{ return false; }
		bool				IsDictionary()							{ return false; }
		bool				IsMarkupExtension()						{ return false; }
		bool				IsBindable()							{ return false; }
		IXamlType*			ItemType()								{ throw NotImplementedException(); }
		IXamlType*			KeyType()								{ throw NotImplementedException(); }
		Object*				ActivateInstance()						{ throw NotImplementedException(); }
		IXamlMember*		GetMember(hstring_ref name)				{ return new XamlSystemMember(name); }
		HRESULT				AddToVector(Object*, Object*)			{ throw NotImplementedException(); }
		HRESULT				AddToMap(Object*, Object* key, Object*)	{ throw NotImplementedException(); }
		HRESULT				RunInitializer()						{ return S_OK; }//throw NotImplementedException(); }
		Object*				CreateFromString(hstring_ref value)		{ throw NotImplementedException(); }
	};

	class Provider : public runtime<Provider, IXamlMetadataProvider> {
	public:
		ptr<IXamlType> get_type_internal(hstring_ref typeName, bool sys) {
			if (!typeName)
				return nullptr;

			if (auto val = types.check(typeName)) {
				if (auto xaml = val->get<IXamlType>())
					return xaml;
			}

			if (auto *info = type_id::find(typeName)) {
				XamlUserType	*type = new XamlUserType(this, typeName, get_type_internal(info->base, true));
				type->KindOfType	= Interop::TypeKind::Custom;
				type->Activator		= info->activator;
				type->IsLocalType	= true;
				type->IsBindable	= info->flags & info->Bindable;

				for (auto j = info->members; j != info->members + info->num_members; ++j)
					type->AddMemberName(str(j->name));

				types[typeName]		= type;
				return type;
			}
			if (sys) {
				static const wchar_t* system_types[] = {
					L"Boolean", L"Int16", L"Int32", L"Int64", L"UInt8", L"UInt16", L"UInt32", L"UInt64", L"Single", L"Double", L"Char16", L"Guid", L"String", L"Object"
				};
				for (auto &i : system_types) {
					if (typeName == str(i)) {
						XamlSystemType	*type	= new XamlSystemType({typeName, Interop::TypeKind::Primitive});
						types[typeName]			= type;
						return type;
					}
				}

				if (to_string(typeName).begins("Windows.")) {
					XamlSystemType	*type	= new XamlSystemType({typeName, Interop::TypeKind::Custom});
					types[typeName]			= type;
					return type;
				}
			}
			return nullptr;
		}
	public:
		ptr<IXamlType> GetXamlType(hstring_ref typeName) {
			return get_type_internal(typeName, false);
		}

		ptr<IXamlType> GetXamlType(Interop::TypeName type) {
			auto	xaml	= get_type_internal(type.Name, false);
			auto	user	= (ptr<XamlUserType>)xaml;

			if (xaml == nullptr || (user && user->IsReturnTypeStub && !user->IsLocalType)) {
				if (auto libXamlType = CheckOtherMetadataProvidersForType(type)) {
					if (libXamlType->IsConstructible() || xaml == nullptr)
						xaml = libXamlType;
				}
			}
			return xaml;
		}

		ptr<IXamlMember> GetMemberByLongName(hstring_ref longMemberName) {
			if (!longMemberName)
				return nullptr;

			if (auto val = members.check(longMemberName))
				return *val;

			auto member = CreateXamlMember(longMemberName);
			if (member)
				members[longMemberName] = member;

			return member;
		}
		void AddOtherProvider(IXamlMetadataProvider* otherProvider) {
			OtherProviders.push_back(otherProvider);
		}

	private:
		hash_map<hstring, WeakReference>		types;
		hash_map<hstring, ptr<IXamlMember>>		members;
		dynamic_array<IXamlMetadataProvider*>	OtherProviders;

		static const member* GetMemberInfo(hstring_ref longMemberName) {
			unsigned		length;
			const wchar_t	*raw = longMemberName.raw(&length);

			if (auto last_dot = string_rfind(raw, '.', raw + length)) {
				if (auto *info = type_id::find(str(raw, last_dot)))
					return info->find_member(string(last_dot + 1, raw + length));
			}
			return nullptr;
		}

		IXamlMember* CreateXamlMember(hstring_ref longMemberName) {
			if (auto *info = GetMemberInfo(longMemberName))
				return new XamlUserMember(this, info);
			return nullptr;
		}

		IXamlType* CheckOtherMetadataProvidersForName(hstring_ref typeName) {
			IXamlType* found = nullptr;
			for (auto &i : OtherProviders) {
				if (auto xaml = i->GetXamlType(typeName)) {
					if (xaml->IsConstructible())    // not Constructible means it might be a Return Type Stub
						return xaml;
					found = xaml;
				}
			}
			return found;
		}

		IXamlType* CheckOtherMetadataProvidersForType(Interop::TypeName t) {
			IXamlType* found = nullptr;
			for (auto &i : OtherProviders) {
				if (auto xaml = i->GetXamlType(t)) {
					if (xaml->IsConstructible())    // not Constructible means it might be a Return Type Stub
						return xaml;

					found = xaml;
				}
			}
			return found;
		}

	};

	inline ptr<IXamlMember>	XamlUserType::ContentProperty() { return provider->GetMemberByLongName(contentPropertyName); }
	inline ptr<IXamlType>	XamlUserType::ItemType()		{ return provider->get_type_internal(itemTypeName, true); }
	inline ptr<IXamlType>	XamlUserType::KeyType()			{ return provider->get_type_internal(keyTypeName, true); }

	inline ptr<IXamlMember>	XamlUserType::GetMember(hstring_ref name) {
		if (auto val = memberNames.check(name))
			return provider->GetMemberByLongName(*val);
		return nullptr;
	}

	inline ptr<IXamlType>	XamlUserMember::Type()				{ return provider->get_type_internal(info->type, true); }
}
