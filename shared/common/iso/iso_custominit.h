#ifndef ISO_CUSTOMINIT_H
#define ISO_CUSTOMINIT_H

#if 1//def PLAT_WII
#define ISO_INIT(T)		void Init(T *p, void *physram)
#define ISO_DEINIT(T)	void DeInit(T *p)
#else
namespace ISO {
template<typename T>	void Init(T *p, void *physram);
template<typename T>	void DeInit(T *p);
}
#define ISO_INIT(T)		template<> void Init<T>(T *p, void *physram)
#define ISO_DEINIT(T)	template<> void DeInit<T>(T *p)
#endif

#endif