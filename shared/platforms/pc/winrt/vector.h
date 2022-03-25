#pragma once

#include "base.h"
#include "windows.foundation.collections.h"
#include "base/algorithm.h"

//-----------------------------------------------------------------------------
//	Vector
//-----------------------------------------------------------------------------

namespace iso_winrt {
namespace Windows { namespace Foundation { namespace Collections {

	template<typename T> class Vector : public runtime<Vector<T>, IObservableVector<T>, IVectorView<T>> {
		dynamic_array<T>	imp;

		struct VectorChangedEventArgs : runtime<VectorChangedEventArgs, IVectorChangedEventArgs> {
			Collections::CollectionChange	CollectionChange;
			unsigned						Index;
			VectorChangedEventArgs(Collections::CollectionChange change, unsigned index) : CollectionChange(change), Index(index) {}
		};

		void Notify(CollectionChange change, unsigned index) {
			if (VectorChanged.any()) {
				auto	args = new VectorChangedEventArgs(change, index);
				VectorChanged(to_abi(this), args);
				args->Release();
			}
		}
		void NotifyReset()					{ Notify(CollectionChange::Reset, 0); }
		void NotifyInserted(unsigned index)	{ Notify(CollectionChange::ItemInserted, index); }
		void NotifyRemoved(unsigned index)	{ Notify(CollectionChange::ItemRemoved, index); }
		void NotifyChanged(unsigned index)	{ Notify(CollectionChange::ItemChanged, index); }

		struct iterator : runtime<IIterator<T>>	{
			T		*Current;
			T		*end;
			bool	HasCurrent()	{ return Current < end; }
			bool	MoveNext()		{ if (Current >= end) return false; ++Current; return true; }
			unsigned GetMany(const szarray<T>& items) {
				unsigned n = min(end - Current, items.size);
				copy_n(imp.begin(), Current, n);
				return n;
			}
			iterator(T *begin, T *_end) : Current(begin), end(_end) {}
		};

	public:
		event<VectorChangedEventHandler<T> >	VectorChanged;

		struct {
			operator unsigned()		{ return encloser(this, &Vector::Size)->imp.size32(); }
			unsigned operator()()	{ return operator unsigned(); }
		} Size;

		T					GetAt(unsigned index)	{ return imp[index]; }
		pptr<IVectorView<T>> GetView()				{ return this; }
		bool				IndexOf(T value, unsigned *index) {
			auto	i = find(imp, value);
			if (i == imp.end())
				return false;
			*index = imp.index_of(i);
			return true;
		}
		void				SetAt(unsigned index, T value)		{ imp[index] = value; NotifyChanged(index); }
		void				InsertAt(unsigned index, T value)	{ imp.insert(&imp[index], value); NotifyInserted(index); }
		void				RemoveAt(unsigned index)			{ imp.erase(&imp[index]); NotifyRemoved(index); }
		void				Append(T value)						{ auto index = imp.size32(); imp.push_back(value); NotifyInserted(index); }
		void				RemoveAtEnd()						{ imp.pop_back(); NotifyRemoved(imp.size32()); }
		void				Clear()								{ imp.clear(); NotifyReset(); }
		unsigned			GetMany(unsigned startIndex, const szarray<T>& items) {
			unsigned n = min(imp.size32() - startIndex, items.size);
			copy_n(imp.begin(), items.p, n);
			return n;
		}
		void				ReplaceAll(const szarray<T>& items) { imp = items; NotifyReset(); }

		ptr<IIterator<T>>	First() { return new iterator(imp.begin(), imp.end()); }

		Vector()	{}
		~Vector()	{}
	};

}}}

template<typename T> struct def_runtime<Windows::Foundation::Collections::Vector<T>> : def<Windows::Foundation::Collections::IObservableVector<T>> {};

} // namespace iso_winrt
