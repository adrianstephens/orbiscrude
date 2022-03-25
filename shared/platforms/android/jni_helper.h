#ifndef JNI_HELPER_H
#define JNI_HELPER_H

#include <jni.h>
#include <android/log.h>
#include "base/strings.h"

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "isopod", __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "isopod", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "isopod", __VA_ARGS__))
#define LOGV(...) ((void)__android_log_print(ANDROID_LOG_VERBOSE, "isopod", __VA_ARGS__))

namespace iso { namespace jni {

template<typename T> T		JPARAM2(const T &t, ...) 					{ return t; }
template<typename T> auto	JPARAM(const T &t)->decltype(JPARAM2(t, t)) { return JPARAM2(t, t); }

template<typename T, typename V = void> struct JSIG;

template<typename R, typename...PP> struct JSIG<R(PP...)> {
	static constexpr auto sig() { return concat_strings("(", JSIG<PP>::sig()..., ")", JSIG<R>::sig()); }
};

template<> struct JSIG<void> {
	static constexpr const char (&sig())[2] { return "V"; }
	static void		call(JNIEnv *env, jobject o, jmethodID id, va_list args)	{ return env->CallVoidMethodV(o, id, args); }
	static void		call(JNIEnv *env, jclass c, jmethodID id, va_list args)		{ return env->CallStaticVoidMethodV(c, id, args); }
};
template<> struct JSIG<jobject> {
	static constexpr const char (&sig())[3] { return "L;"; }
	static jobject	get	(JNIEnv *env, jobject o, jfieldID id)					{ return env->GetObjectField(o, id); }
	static jobject	get	(JNIEnv *env, jclass c, jfieldID id)					{ return env->GetStaticObjectField(c, id); }
	static jobject	call(JNIEnv *env, jobject o, jmethodID id, va_list args)	{ return env->CallObjectMethodV(o, id, args); }
	static jobject	call(JNIEnv *env, jclass c, jmethodID id, va_list args)		{ return env->CallStaticObjectMethodV(c, id, args); }
};
template<> struct JSIG<bool> {
	typedef jbooleanArray	A;
	static constexpr const char (&sig())[2] { return "Z"; }
	static bool		get(JNIEnv *env, jobject o, jfieldID id)					{ return env->GetBooleanField(o, id); }
	static bool		get(JNIEnv *env, jclass c, jfieldID id)						{ return env->GetStaticBooleanField(c, id); }
	static void		set(JNIEnv *env, jobject o, jfieldID id, bool val)			{ return env->SetBooleanField(o, id, val); }
	static void		set(JNIEnv *env, jclass c, jfieldID id, bool val)			{ return env->SetStaticBooleanField(c, id, val); }
	static bool		call(JNIEnv *env, jobject o, jmethodID id, va_list args)	{ return env->CallBooleanMethodV(o, id, args); }
	static bool		call(JNIEnv *env, jclass c, jmethodID id, va_list args)		{ return env->CallStaticBooleanMethodV(c, id, args); }
	static A		new_array(JNIEnv *env, jsize length)						{ return env->NewBooleanArray(length); }
	static jboolean* get_array(JNIEnv *env, A a)								{ return env->GetBooleanArrayElements(a, nullptr); }
	static void		release_array(JNIEnv *env, A a, jboolean *elems)			{ env->ReleaseBooleanArrayElements(a, elems, 0); }
	static void 	get_region(JNIEnv *env, A a, jsize start, jsize len, jboolean* buf)			{ env->GetBooleanArrayRegion(a, start, len, buf); }
	static void 	set_region(JNIEnv *env, A a, jsize start, jsize len, const jboolean* buf)	{ env->SetBooleanArrayRegion(a, start, len, buf); }
};
template<> struct JSIG<int8_t> {
	typedef jbyteArray	A;
	static constexpr const char (&sig())[2] { return "B"; }
	static int8		get(JNIEnv *env, jobject o, jfieldID id)					{ return env->GetByteField(o, id); }
	static int8		get(JNIEnv *env, jclass c, jfieldID id)						{ return env->GetStaticByteField(c, id); }
	static void		set(JNIEnv *env, jobject o, jfieldID id, int8 val)		{ return env->SetByteField(o, id, val); }
	static void		set(JNIEnv *env, jclass c, jfieldID id, int8 val)			{ return env->SetStaticByteField(c, id, val); }
	static int8		call(JNIEnv *env, jobject o, jmethodID id, va_list args)	{ return env->CallByteMethodV(o, id, args); }
	static int8		call(JNIEnv *env, jclass c, jmethodID id, va_list args)		{ return env->CallStaticByteMethodV(c, id, args); }
	static A		new_array(JNIEnv *env, jsize length)						{ return env->NewByteArray(length); }
	static int8*	get_array(JNIEnv *env, A a)									{ return env->GetByteArrayElements(a, nullptr); }
	static void		release_array(JNIEnv *env, A a, int8 *elems)				{ env->ReleaseByteArrayElements(a, elems, 0); }
	static void 	get_region(JNIEnv *env, A a, jsize start, jsize len, int8* buf)			{ env->GetByteArrayRegion(a, start, len, buf); }
	static void 	set_region(JNIEnv *env, A a, jsize start, jsize len, const int8* buf)	{ env->SetByteArrayRegion(a, start, len, buf); }
};
template<> struct JSIG<jchar> {
	typedef jcharArray	A;
	static constexpr const char (&sig())[2] { return "C"; }
	static jchar	get(JNIEnv *env, jobject o, jfieldID id)					{ return env->GetCharField(o, id); }
	static jchar	get(JNIEnv *env, jclass c, jfieldID id)						{ return env->GetStaticCharField(c, id); }
	static void		set(JNIEnv *env, jobject o, jfieldID id, jchar val)			{ return env->SetCharField(o, id, val); }
	static void		set(JNIEnv *env, jclass c, jfieldID id, jchar val)			{ return env->SetStaticCharField(c, id, val); }
	static jchar	call(JNIEnv *env, jobject o, jmethodID id, va_list args)	{ return env->CallCharMethodV(o, id, args); }
	static jchar	call(JNIEnv *env, jclass c, jmethodID id, va_list args)		{ return env->CallStaticCharMethodV(c, id, args); }
	static A		new_array(JNIEnv *env, jsize length)						{ return env->NewCharArray(length); }
	static jchar*	get_array(JNIEnv *env, A a)									{ return env->GetCharArrayElements(a, nullptr); }
	static void		release_array(JNIEnv *env, A a, jchar *elems)				{ env->ReleaseCharArrayElements(a, elems, 0); }
	static void 	get_region(JNIEnv *env, A a, jsize start, jsize len, jchar* buf)		{ env->GetCharArrayRegion(a, start, len, buf); }
	static void 	set_region(JNIEnv *env, A a, jsize start, jsize len, const jchar* buf)	{ env->SetCharArrayRegion(a, start, len, buf); }
};
template<> struct JSIG<int16> {
	typedef jshortArray	A;
	static constexpr const char (&sig())[2] { return "S"; }
	static int16	get(JNIEnv *env, jobject o, jfieldID id)					{ return env->GetShortField(o, id); }
	static int16	get(JNIEnv *env, jclass c, jfieldID id)						{ return env->GetStaticShortField(c, id); }
	static void		set(JNIEnv *env, jobject o, jfieldID id, int16 val)		{ return env->SetShortField(o, id, val); }
	static void		set(JNIEnv *env, jclass c, jfieldID id, int16 val)		{ return env->SetStaticShortField(c, id, val); }
	static int16	call(JNIEnv *env, jobject o, jmethodID id, va_list args)	{ return env->CallShortMethodV(o, id, args); }
	static int16	call(JNIEnv *env, jclass c, jmethodID id, va_list args)		{ return env->CallStaticShortMethodV(c, id, args); }
	static A		new_array(JNIEnv *env, jsize length)						{ return env->NewShortArray(length); }
	static int16*	get_array(JNIEnv *env, A a)									{ return env->GetShortArrayElements(a, nullptr); }
	static void		release_array(JNIEnv *env, A a, int16 *elems)				{ env->ReleaseShortArrayElements(a, elems, 0); }
	static void 	get_region(JNIEnv *env, A a, jsize start, jsize len, int16* buf)		{ env->GetShortArrayRegion(a, start, len, buf); }
	static void 	set_region(JNIEnv *env, A a, jsize start, jsize len, const int16* buf)	{ env->SetShortArrayRegion(a, start, len, buf); }
};
template<> struct JSIG<int32> {
	typedef jintArray	A;
	static constexpr const char (&sig())[2] { return "I"; }
	static int32	get(JNIEnv *env, jobject o, jfieldID id)					{ return env->GetIntField(o, id); }
	static int32	get(JNIEnv *env, jclass c, jfieldID id)						{ return env->GetStaticIntField(c, id); }
	static void		set(JNIEnv *env, jobject o, jfieldID id, int32 val)		{ return env->SetIntField(o, id, val); }
	static void		set(JNIEnv *env, jclass c, jfieldID id, int32 val)		{ return env->SetStaticIntField(c, id, val); }
	static int32	call(JNIEnv *env, jobject o, jmethodID id, va_list args)	{ return env->CallIntMethodV(o, id, args); }
	static int32	call(JNIEnv *env, jclass c, jmethodID id, va_list args)		{ return env->CallStaticIntMethodV(c, id, args); }
	static A		new_array(JNIEnv *env, jsize length)						{ return env->NewIntArray(length); }
	static int32*	get_array(JNIEnv *env, A a)									{ return env->GetIntArrayElements(a, nullptr); }
	static void		release_array(JNIEnv *env, A a, int32 *elems)				{ env->ReleaseIntArrayElements(a, elems, 0); }
	static void 	get_region(JNIEnv *env, A a, jsize start, jsize len, int32* buf)		{ env->GetIntArrayRegion(a, start, len, buf); }
	static void 	set_region(JNIEnv *env, A a, jsize start, jsize len, const int32* buf)	{ env->SetIntArrayRegion(a, start, len, buf); }
};
template<> struct JSIG<int64> {
	typedef jlongArray	A;
	static constexpr const char (&sig())[2] { return "J"; }
	static int64	get(JNIEnv *env, jobject o, jfieldID id)					{ return env->GetLongField(o, id); }
	static int64	get(JNIEnv *env, jclass c, jfieldID id)						{ return env->GetStaticLongField(c, id); }
	static void		set(JNIEnv *env, jobject o, jfieldID id, int64 val)		{ return env->SetLongField(o, id, val); }
	static void		set(JNIEnv *env, jclass c, jfieldID id, int64 val)		{ return env->SetStaticLongField(c, id, val); }
	static int64	call(JNIEnv *env, jobject o, jmethodID id, va_list args)	{ return env->CallLongMethodV(o, id, args); }
	static int64	call(JNIEnv *env, jclass c, jmethodID id, va_list args)		{ return env->CallStaticLongMethodV(c, id, args); }
	static A		new_array(JNIEnv *env, jsize length)						{ return env->NewLongArray(length); }
	static int64*	get_array(JNIEnv *env, A a)									{ return env->GetLongArrayElements(a, nullptr); }
	static void		release_array(JNIEnv *env, A a, int64 *elems)				{ env->ReleaseLongArrayElements(a, elems, 0); }
	static void 	get_region(JNIEnv *env, A a, jsize start, jsize len, int64* buf)		{ env->GetLongArrayRegion(a, start, len, buf); }
	static void 	set_region(JNIEnv *env, A a, jsize start, jsize len, const int64* buf)	{ env->SetLongArrayRegion(a, start, len, buf); }
};
template<> struct JSIG<float> {
	typedef jfloatArray	A;
	static constexpr const char (&sig())[2] { return "F"; }
	static float	get(JNIEnv *env, jobject o, jfieldID id)					{ return env->GetFloatField(o, id); }
	static float	get(JNIEnv *env, jclass c, jfieldID id)						{ return env->GetStaticFloatField(c, id); }
	static void		set(JNIEnv *env, jobject o, jfieldID id, float val)			{ return env->SetFloatField(o, id, val); }
	static void		set(JNIEnv *env, jclass c, jfieldID id, float val)			{ return env->SetStaticFloatField(c, id, val); }
	static float	call(JNIEnv *env, jobject o, jmethodID id, va_list args)	{ return env->CallFloatMethodV(o, id, args); }
	static float	call(JNIEnv *env, jclass c, jmethodID id, va_list args)		{ return env->CallStaticFloatMethodV(c, id, args); }
	static A		new_array(JNIEnv *env, jsize length)						{ return env->NewFloatArray(length); }
	static float*	get_array(JNIEnv *env, A a)									{ return env->GetFloatArrayElements(a, nullptr); }
	static void		release_array(JNIEnv *env, A a, float *elems)				{ env->ReleaseFloatArrayElements(a, elems, 0); }
	static void 	get_region(JNIEnv *env, A a, jsize start, jsize len, float* buf)		{ env->GetFloatArrayRegion(a, start, len, buf); }
	static void 	set_region(JNIEnv *env, A a, jsize start, jsize len, const float* buf)	{ env->SetFloatArrayRegion(a, start, len, buf); }
};
template<> struct JSIG<double> {
	typedef jdoubleArray	A;
	static constexpr const char (&sig())[2] { return "D"; }
	static double	get(JNIEnv *env, jobject o, jfieldID id)					{ return env->GetDoubleField(o, id); }
	static double	get(JNIEnv *env, jclass c, jfieldID id)						{ return env->GetStaticDoubleField(c, id); }
	static void		set(JNIEnv *env, jobject o, jfieldID id, double val)		{ return env->SetDoubleField(o, id, val); }
	static void		set(JNIEnv *env, jclass c, jfieldID id, double val)			{ return env->SetStaticDoubleField(c, id, val); }
	static double	call(JNIEnv *env, jobject o, jmethodID id, va_list args)	{ return env->CallDoubleMethodV(o, id, args); }
	static double	call(JNIEnv *env, jclass c, jmethodID id, va_list args)		{ return env->CallStaticDoubleMethodV(c, id, args); }
	static A		new_array(JNIEnv *env, jsize length)						{ return env->NewDoubleArray(length); }
	static double*	get_array(JNIEnv *env, A a)									{ return env->GetDoubleArrayElements(a, nullptr); }
	static void		release_array(JNIEnv *env, A a, double *elems)				{ env->ReleaseDoubleArrayElements(a, elems, 0); }
	static void 	get_region(JNIEnv *env, A a, jsize start, jsize len, double* buf)		{ env->GetDoubleArrayRegion(a, start, len, buf); }
	static void 	set_region(JNIEnv *env, A a, jsize start, jsize len, const double* buf)	{ env->SetDoubleArrayRegion(a, start, len, buf); }
};

template<typename T, typename U> struct JSIG_cast : JSIG<T> {
	using typename JSIG<T>::A;
	static U*		get_array(JNIEnv *env, A a)											{ return (U*)JSIG<T>::get_array(env, a); }
	static void		release_array(JNIEnv *env, A a, U *elems)							{ JSIG<T>::release_array(env, a, (T*)elems); }
	static void 	get_region(JNIEnv *env, A a, jsize start, jsize len, U* buf)		{ JSIG<T>::get_region(env, a, start, len, (T*)buf); }
	static void 	set_region(JNIEnv *env, A a, jsize start, jsize len, const U* buf)	{ JSIG<T>::set_region(env, a, start, len, (const T*)buf); }
};

template<> struct JSIG<uint8>  : JSIG_cast<int8,  uint8 > {};
//	template<> struct JSIG<uint16> : JSIG_cast<int16, uint16> {};
template<> struct JSIG<uint32> : JSIG_cast<int32, uint32> {};
template<> struct JSIG<uint64> : JSIG_cast<int64, uint64> {};

// call static method
template<typename T> T	IDCall(JNIEnv *env, jclass c, jmethodID id, ...) {
	va_list args;
	va_start(args, id);
	return JSIG<T>::call(env, c, id, args);
}
template<typename T> T	SigCall(JNIEnv *env, jclass c, const char *name, const char *sig, ...) {
	va_list args;
	va_start(args, sig);
	return JSIG<T>::call(env, c, env->GetStaticMethodID(c, name, sig), args);
}
template<typename R, typename...TT> R	Call(JNIEnv *env, jclass c, const char *name, const TT &...tt) {
	return SigCall<R>(env, c, name, JSIG<R(TT...)>::sig(), JPARAM(tt)...);
}

// call method
template<typename T> T	IDCall(JNIEnv *env, jclass c, jobject o, jmethodID id, ...) {
	va_list args;
	va_start(args, id);
	return JSIG<T>::call(env, o, id, args);
}
template<typename T> T	SigCall(JNIEnv *env, jclass c, jobject o, const char *name, const char *sig, ...) {
	va_list args;
	va_start(args, sig);
	return JSIG<T>::call(env, o, env->GetMethodID(c, name, sig), args);
}
template<typename R, typename...TT> R	Call(JNIEnv *env, jclass c, jobject o, const char *name, const TT &...tt) {
	return SigCall<R>(env, c, o, name, JSIG<R(TT...)>::sig(), JPARAM(tt)...);
}

// get/set static field
template<typename T> T		GetField(JNIEnv *env, jclass c, const char *name) {
	return JSIG<T>::get(env, c, env->GetStaticFieldID(c, name, JSIG<T>::sig()));
}
template<typename T> void	SetField(JNIEnv *env, jclass c, const char *name, const T &t) {
	JSIG<T>::set(env, c, env->GetStaticFieldID(c, name, JSIG<T>::sig()), t);
}

// get/set field
template<typename T> T		GetField(JNIEnv *env, jclass c, jobject o, const char *name) {
	return JSIG<T>::get(env, o, env->GetFieldID(c, name, JSIG<T>::sig()));
}
template<typename T> void	SetField(JNIEnv *env, jclass c, jobject o, const char *name, const T &t) {
	JSIG<T>::get(env, o, env->GetFieldID(c, name, JSIG<T>::sig()), t);
}

//ObjectHolder

struct ObjectHolder {
	static thread_local JNIEnv *env;
	jobject	obj;
	bool	own;

	ObjectHolder(jobject o = 0, bool own = false) : obj(o), own(own) {}
	~ObjectHolder() 			{ if (own) env->DeleteLocalRef(obj); }
	operator jobject() const	{ return obj; }
	jclass	GetClass() const 	{ return env->GetObjectClass(obj); }
	jobject	Detach() 			{ own = false; return obj; }
};
template<typename T> jobject	JPARAM2(const T &t, const ObjectHolder &t2)	{ return t.obj; }

template<typename T> struct JSIG_object {
	static T		make(JNIEnv *env, jobject o)								{ return static_cast<T&&>(ObjectHolder(o, false)); }
	static T		get	(JNIEnv *env, jobject o, jfieldID id)					{ return make(env, env->GetObjectField(o, id)); }
	static T		get	(JNIEnv *env, jclass c, jfieldID id)					{ return make(env, env->GetStaticObjectField(c, id)); }
	static void		set(JNIEnv *env, jobject o, jfieldID id, const T &val)		{ return env->SetObjectField(o, id, val.o); }
	static void		set(JNIEnv *env, jclass c, jfieldID id, const T &val)		{ return env->SetStaticObjectField(c, id, val.o); }
	static T		call(JNIEnv *env, jobject o, jmethodID id, va_list args)	{ return make(env, env->CallObjectMethodV(o, id, args)); }
	static T		call(JNIEnv *env, jclass c, jmethodID id, va_list args)		{ return make(env, env->CallStaticObjectMethodV(c, id, args)); }
};

// Arrays

template<typename T, typename V = void> struct Array : ObjectHolder {
	typedef typename JSIG<T>::A	A;
	struct Elements {
		A			a;
		T 			*elements;
		Elements(A a) : a(a), elements(JSIG<T>::get_array(env, a)) {}
		~Elements() 		{ JSIG<T>::release_array(env, a, elements); }
		operator T*() const	{ return elements; }
	};
	Array(jsize len) : ObjectHolder(JSIG<T>::new_array(env, len), true) {}
	Array(jsize len, const T *buffer) : ObjectHolder(JSIG<T>::new_array(env, len), true) { set_region(0, len, buffer); }
	jsize 		length()											const { return env->GetArrayLength((jarray)obj); }
	T			operator[](jsize index)								const { return Elements((A)obj)[index]; }
	Elements 	elements()											const { return Elements((A)obj); }
	void		get_region(jsize start, jsize len, T *buffer) 		const { return JSIG<T>::get_region(env, (A)obj, start, len, buffer); }
	void		set_region(jsize start, jsize len, const T *buffer) const { return JSIG<T>::set_region(env, (A)obj, start, len, buffer); }
	void		output(T *out_array, size_t out_max) 				const { get_region(0, out_max, out_array); }
	template<size_t N> void		output(T (&out)[N]) 				const { output(out, N); }
};

template<> struct Array<jobject> : ObjectHolder {
	struct Ref {
		jobjectArray	a;
		jsize 			i;
		Ref(jobjectArray a, jsize i) : a(a), i(i) {}
		operator jobject() const			{ return env->GetObjectArrayElement(a, i); }
		void operator=(const jobject &t) 	{ env->SetObjectArrayElement(a, i, t); }
	};
	Array(jsize length, jclass c, jobject init = nullptr) : ObjectHolder(env->NewObjectArray(length, c, init), true) {}
	jsize 	length()			const 	{ return env->GetArrayLength((jarray)obj); }
	Ref		operator[](jsize i)	const 	{ return Ref((jobjectArray)obj, i); }
};

template<typename T> struct JSIG<Array<T>> : JSIG_object<Array<T>> {
	static constexpr auto sig()->decltype(concat_strings("[", JSIG<T>::sig())) { return concat_strings("[", JSIG<T>::sig()); }
};

template<typename T> Array<T> MakeArray(const T *buffer, jsize len) 	{ return Array<T>(len, buffer); }
template<typename T, int N> Array<T> MakeArray(const T (&buffer)[N]) 	{ return Array<T>(N, buffer); }

// Class

struct Object : ObjectHolder {
	struct Field {
		jobject		o;
		jclass		c;
		const char	*name;
		Field(jobject o, jclass c, const char *name) : o(o), c(c), name(name) {}
		template<typename F> operator F() 				const { return jni::GetField<F>(env, c, o, name); }
		template<typename F> void operator=(const F &f)	const { jni::SetField<F>(env, c, o, name, f); }
		template<typename F> void operator=(F &&f) 		const { jni::SetField<F>(env, c, o, name, f); }
	};

	Object(jobject o = 0, bool own = false) : ObjectHolder(o, own) {}

	template<typename T> T		GetField(const char *name) 						const 	{ return jni::GetField<T>(env, GetClass(), obj, name); }
	template<typename T> void	SetField(const char *name, const T &t) 			const 	{ jni::SetField(env, GetClass(), obj, name, t); }
	template<typename T> T		GetStaticField(const char *name) 				const 	{ return jni::GetField<T>(env, GetClass(), name); }
	template<typename T> void	SetStaticField(const char *name, const T &t) 	const 	{ jni::SetField(env, GetClass(), name, t); }

	template<typename R, typename...TT> R	Call(const char *name, const TT &...tt) const {
		return SigCall<R>(env, GetClass(), obj, name, JSIG<R(TT...)>::sig(), JPARAM(tt)...);
	}
	template<typename R, typename...TT> R	CallStatic(const char *name, const TT &...tt) const {
		return SigCall<R>(env, GetClass(), name, JSIG<R(TT...)>::sig(), JPARAM(tt)...);
	}

	Field	operator[](const char *name) const { return Field(obj, GetClass(), name); }

	jobject	Detach() { own = false; return obj; }
};

template<typename E> struct Enum : Object {
	E		get()	const { return (E)Call<int>("ordinal"); }
	operator E() 	const { return get(); }
};

template<typename U, size_t A = 0x1000, typename V = value_list<char> >	struct to_java_name;
template<char u, char... uu, size_t A, char... vv>	struct to_java_name<value_list<char, u, uu...>,			A, value_list<char, vv...> > : to_java_name<value_list<char, uu...>, A, value_list<char, vv..., u> >	{};
template<char... uu, size_t A, char... vv>			struct to_java_name<value_list<char, ':', ':', uu...>,	A, value_list<char, vv...> > : to_java_name<value_list<char, uu...>, A, value_list<char, vv..., (sizeof...(vv) < A ? '/' : '$')> >	{};
template<char... uu, size_t A, char... vv>			struct to_java_name<value_list<char, '>', uu...>,		A, value_list<char, vv...> > : value_list<char, vv..., 0> {};

template<typename T, typename V = void>		struct to_java_name2 : to_java_name<explode<char, meta::name_s<T>>::type> {};
template<typename T>						struct to_java_name2<T, typename void_t<typename T::Parent>::type> :
	to_java_name<explode<char, meta::name_s<T>>::type, to_java_name<explode<char, meta::name_s<typename T::Parent>>::type>::size - 1>
{};

template<typename B> void _is_base(const B*);

template<typename T> struct JSIG<T, decltype(_is_base<Object>((T*)0))> : JSIG_object<T> {
	static constexpr auto name()			{ return meta::make_array(to_java_name2<T>()); }
	static constexpr auto sig()				{ return "L" + name() + ";"; }
	static jclass 	get_class(JNIEnv *env)	{ static jclass c = env->FindClass(name()); return c; }
};

template<typename T> struct Class : Object {
	struct SubClass : Object {
		typedef T Parent;
	};
	template<typename E> struct Enum : jni::Enum<E> {
		typedef T Parent;
	};

	Class() {}
	Class(JNIEnv *env, jobject o = 0, bool own = false) : Object(o, own) {}

	jclass 	GetClass() const { if (jclass c = JSIG<T>::get_class(env)) return c; return env->GetObjectClass(obj); }

	// lookup method & field IDs
	template<typename F> F		GetField(const char *name) 						const { return jni::GetField<F>(env, GetClass(), obj, name); }
	template<typename F> void	SetField(const char *name, const F &t) 			const { jni::SetField(env, GetClass(), obj, name, t); }
	template<typename F> F		GetStaticField(const char *name) 				const { return jni::GetField<F>(env, GetClass(), name); }
	template<typename F> void	SetStaticField(const char *name, const F &t) 	const { jni::SetField(env, GetClass(), name, t); }

	template<typename R, typename...TT> R		Call(const char *name, const TT &...tt) const {
		return SigCall<R>(env, GetClass(), obj, name, JSIG<R(TT...)>::sig(), JPARAM(tt)...);
	}
	template<typename R, typename...TT> R		CallStatic(const char *name, const TT &...tt) const {
		return SigCall<R>(env, GetClass(), name, JSIG<R(TT...)>::sig(), JPARAM(tt)...);
	}

	// with static method & field IDs
	template<typename F, typename N> jfieldID	GetFieldID(N name) 					const { static jfieldID id = env->GetStaticFieldID(GetClass(), meta::make_array(name), JSIG<T>::sig()); return id; }
	template<typename F, typename N> jfieldID	GetStaticFieldID(N name) 			const { static jfieldID id = env->GetStaticFieldID(GetClass(), meta::make_array(name), JSIG<T>::sig()); return id; }

	template<typename F, typename N> F			GetField(N name) 					const { JSIG<F>::get(env, obj, GetFieldID<F>(name)); }
	template<typename F, typename N> void		SetField(N name, const F &t) 		const { JSIG<F>::set(env, obj, GetFieldID<F>(name), t); }
	template<typename F, typename N> F			GetStaticField(N name) 				const { JSIG<F>::get(env, GetStaticFieldID<F>(name)); }
	template<typename F, typename N> void		SetStaticField(N name, const F &t) 	const { JSIG<F>::set(env, GetStaticFieldID<F>(name), t); }

	template<typename R, typename N, typename...TT> R	Call(N name, const TT &...tt) const {
		static jmethodID id = env->GetMethodID(GetClass(), meta::make_array(name), JSIG<R(TT...)>::sig());
		return IDCall<R>(env, GetClass(), obj, id, JPARAM(tt)...);
	}
	template<typename R, typename N, typename...TT> R	CallStatic(N name, const TT &...tt) const {
		static jmethodID id = env->GetStaticMethodID(GetClass(), meta::make_array(name), JSIG<R(TT...)>::sig());
		return IDCall<R>(env, GetClass(), id, JPARAM(tt)...);
	}
	Field	operator[](const char *name) const { return Field(env, obj, GetClass(), name); }
};


template<typename T> struct Array<T, decltype(_is_base<Object>((T*)0))> : ObjectHolder {
	struct Ref {
		JNIEnv 			*env;
		jobjectArray	a;
		jsize 			i;
		Ref(JNIEnv *env, jobjectArray a, jsize i) : env(env), a(a), i(i) {}
		operator T() const				{ return JSIG<T>::make(env->GetObjectArrayElement(a, i)); };
		void operator=(T &&t) 			{ env->SetObjectArrayElement(a, i, t.obj); t.own = false; }
		void operator=(const T &t) 		{ env->SetObjectArrayElement(a, i, t.obj); }
	};
	Array(JNIEnv *env, jsize len, const T &init) : ObjectHolder(env, env->NewObjectArray(len, JSIG<T>::get_class(env), init.obj), true) {}
	Array(JNIEnv *env, jsize len) : ObjectHolder(env, env->NewObjectArray(len, JSIG<T>::get_class(env), nullptr), true) {}
	template<typename U> Array(JNIEnv *env, jsize len, const U *buffer) : ObjectHolder(env, env->NewObjectArray(len, JSIG<T>::get_class(env), nullptr), true) {
		for (int i = 0; i < len; i++)
			env->SetObjectArrayElement((jobjectArray)obj, i, T(env, buffer[i]).Detach());
	}

	jsize 	length()			const 	{ return env->GetArrayLength((jarray)obj); }
	Ref		operator[](jsize i)	const 	{ return Ref(env, (jobjectArray)obj, i); }
};

template<typename T> struct Global : T {
	using T::obj; using T::env;
	Global()											{}
	Global(jobject _o) : Object(T::env->NewGlobalRef(_o))	{}
	Global(Global &&b) : T(b)		{ b.o = 0; }
	Global& operator=(jobject _obj)	{ if (obj) env->DeleteGlobalRef(obj); obj = env->NewGlobalRef(_obj); return *this; }
	Global& operator=(Global &&b)	{ swap(obj, b.obj); return *this; }
};

void		ExceptionCheck();
extern Global<Object>	activity;

} // namespace jni

namespace java { namespace lang {
struct String : iso::jni::Object {
	struct UTF8Chars {
		jstring		s;
		const char 	*chars;
		jboolean	copy;
		UTF8Chars(jstring s) : s(s), chars(env->GetStringUTFChars(s, &copy)) {}
		~UTF8Chars() 					{ env->ReleaseStringUTFChars(s, chars); }
		operator const char*() 	const 	{ return chars; }
		jsize		length()	const 	{ return env->GetStringUTFLength(s); }
	};
	struct JavaChars {
		jstring		s;
		const jchar *chars;
		jboolean	copy;
		JavaChars(jstring s) : s(s), chars(env->GetStringChars(s, &copy)) {}
		~JavaChars() 					{ env->ReleaseStringChars(s, chars); }
		operator const jchar*() const 	{ return chars; }
		jsize		length()	const 	{ return env->GetStringLength(s); }
	};
	String(jstring s) 						: jni::Object(s, false) {}
	String(const jchar *chars, jsize len) 	: jni::Object(env->NewString(chars, len), true) {}
	String(const char *chars)    			: jni::Object(env->NewStringUTF(chars), true) {}

	jsize		length_java()	const	{ return env->GetStringLength((jstring)obj); }
	jsize 		length_utf()	const	{ return env->GetStringUTFLength((jstring)obj); }
	JavaChars	chars_java()	const	{ return JavaChars((jstring)obj); }
	UTF8Chars	chars_utf8()	const	{ return UTF8Chars((jstring)obj); }
	void		output(char *out_str, size_t out_max) 		const { strlcpy(out_str, chars_utf8(), out_max); }
	void		output_pad(char *out_str, size_t out_max) 	const { strncpy(out_str, chars_utf8(), out_max); }
	template<size_t N> void		output(char (&out)[N]) 		const { output(out, N); }
	template<size_t N> void		output_pad(char (&out)[N]) 	const { output_pad(out, N); }
};

struct Class : jni::Class<Class> {
	operator jclass() const { return static_cast<jclass>(obj); }
};
struct ClassLoader : jni::Class<ClassLoader> {};
} }

} // namespace iso

template<typename T, T... chars> constexpr iso::value_list<T, chars..., 0> operator""_static() { return {}; }

#endif //JNI_HELPER_H
