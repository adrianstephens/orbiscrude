#pragma once

#include "base.h"
#include "base/coroutine.h"
#include "Windows.System.Threading.h"

namespace iso_winrt {

//-----------------------------------------------------------------------------
//	co routines
//-----------------------------------------------------------------------------

template<typename T> auto operator co_await(const ptr<T> &p) {
	struct awaiter {
		ptr<T>		p;
		awaiter(const ptr<T> &_p) : p(_p) {}
		bool await_ready()	const { return false; }
		auto await_resume() const { return from_abi(p->GetResults()); }
		void await_suspend(std::experimental::coroutine_handle<> awaiting) const {
			p->Completed = [awaiting]() {
				awaiting.resume();
			};
		}
	};
	return awaiter(p);
}

inline auto operator co_await(const ptr<Windows::Foundation::IAsyncAction> &p) {
	struct awaiter {
		ptr<Windows::Foundation::IAsyncAction>		p;
		awaiter(const ptr<Windows::Foundation::IAsyncAction> &_p) : p(_p) {}
		bool await_ready()	const { return false; }
		void await_resume() const {}
		void await_suspend(std::experimental::coroutine_handle<> awaiting) const {
			p->Completed = [awaiting]() {
				awaiting.resume();
			};
		}
	};
	return awaiter(p);
}

struct apartment_context : ptr<IContextCallback> {
	static STDMETHODIMP callback(ComCallData* data) noexcept {
		std::experimental::coroutine_handle<>::from_address(data->pUserDefined).resume();
		return S_OK;
	}

	apartment_context() {
		hrcheck(CoGetObjectContext(__uuidof(IContextCallback), (void**)&t));
	}

//	~apartment_context() {
//		context->Release();
//	}

	bool await_ready()	const { return false; }
	void await_resume() const {}

	void await_suspend(std::experimental::coroutine_handle<> handle) const {
		ComCallData data = {0, 0, handle.address()};
		hrcheck(t->ContextCallback(callback, &data, IID_ICallbackWithNoReentrancyToApplicationSTA, 5, nullptr));
	}

	const apartment_context& operator co_await() const {
		return *this;
	};
};

template<typename P> struct cancellation_token {
	P	*promise;
public:
	cancellation_token(P *_promise) : promise(_promise) {}
	bool await_ready()					const	{ return true; }
	void await_suspend(std::experimental::coroutine_handle<>) const {}
	cancellation_token await_resume()	const	{ return *this; }
	bool operator()()					const	{ return promise->Status == Windows::Foundation::AsyncStatus::Canceled; }
};

struct get_cancellation_token_t {};
constexpr get_cancellation_token_t	get_cancellation_token {};

template<typename P, typename R> struct progress_token {
	P	*promise;
public:
	progress_token(P* _promise) : promise(_promise) {}
	bool await_ready()					const	{ return true; }
	void await_suspend(std::experimental::coroutine_handle<>) const {}
	progress_token await_resume()		const	{ return *this; }
	void operator()(R const& result)	const	{ promise->set_progress(result); }
};

template<typename R, typename P> progress_token<R, P> make_progress_token(P *p) { return p; }
struct get_progress_token_t {};
constexpr get_progress_token_t		get_progress_token {};

namespace Windows {namespace Foundation {

template<typename I> struct promise : future<I>::promise_type {
};

template<typename I, typename C> struct promise_base : runtime<promise<I>, I> {
	typedef	promise<I>	P;
	Mutex				lock;
	ptr<C>				completed;

	ULONG STDMETHODCALLTYPE	Release() {
		ULONG	n = this->release();
		if (n == 0)
			std::experimental::coroutine_handle<P>(static_cast<P*>(this)).destroy();
		return n;
	}
protected:
	static promise_base	get_base();

	void	CheckErrors() {
		if (Status != AsyncStatus::Completed)
			throw COMException(ErrorCode.Value);
	}

public:
	promise_base() : Status(AsyncStatus::Started) { ErrorCode.Value = S_OK; }

	HResult				ErrorCode;
	AsyncStatus			Status;
	static const uint32	Id = 1;


	void	Completed(pptr<C> handler) {
		AsyncStatus status;
		{
			auto	guard = with(lock);
			if (Status == AsyncStatus::Started) {
				completed = handler;
				return;
			}
			status = Status;
		}
		if (handler)
			handler->_Invoke(this, status);
	}
	ptr<C>	Completed() {
		return completed;
	}
	void	Cancel() {
		if (Status == AsyncStatus::Started) {
			Status = AsyncStatus::Canceled;
			ErrorCode.Value = E_ABORT;
		}
	}
	void	Close() {
		if (Status == AsyncStatus::Started)
			throw DisconnectedException();
	}

	struct final_suspend_type {
		promise_base* promise;
		bool await_ready()	const { return false; }
		void await_resume() const {}
		bool await_suspend(std::experimental::coroutine_handle<>) const {
			return promise->release() > 0;
		}
		final_suspend_type(promise_base *_promise) : promise(_promise) {}
	};

	pptr<I>		get_return_object()			{ return query<I>(this); }
	bool		initial_suspend()	const	{ return false; }
	final_suspend_type final_suspend()		{ return this; }
//	bool		final_suspend()				{ return this->release() > 0; }
	
	template<typename E> auto await_transform(E&& expression) {
		if (this->Status == AsyncStatus::Canceled)
			throw OperationCanceledException();
		return forward<E>(expression);
	}
	cancellation_token<P> await_transform(get_cancellation_token_t) {
		return (P*)this;
	}
};

template<> struct promise<IAsyncAction> : promise_base<IAsyncAction, AsyncActionCompletedHandler> {
	void GetResults() {
		CheckErrors();
	}

	void return_void() {
		ptr<AsyncActionCompletedHandler> handler;
		AsyncStatus status;
		{
			auto	guard = with(this->lock);
			if (this->Status == AsyncStatus::Started)
				this->Status = AsyncStatus::Completed;

			handler	= move(this->completed);
			status	= this->Status;
		}
		if (handler)
			handler->_Invoke(this, status);
	}
};

template<typename TProgress> struct promise<IAsyncActionWithProgress<TProgress>> : promise_base<IAsyncActionWithProgress<TProgress>, AsyncActionWithProgressCompletedHandler<TProgress>> {
	typedef decltype(promise::get_base())	base;
	using ProgressHandler	= AsyncActionProgressHandler<TProgress>;
	using base::await_transform;
public:
	ProgressHandler Progress;

	void GetResults() {
		CheckErrors();
	}

	void return_void() {
		AsyncActionWithProgressCompletedHandler<TProgress> handler;
		AsyncStatus status;
		{
			auto	guard = with(this->lock);
			if (this->Status == AsyncStatus::Started)
				this->Status = AsyncStatus::Completed;

			handler	= move(this->completed);
			status	= this->Status;
		}
		if (handler)
			handler->_Invoke(this, status);
	}

	void set_progress(const TProgress &result) {
		if (auto handler = Progress)
			handler(*this, result);
	}
	auto await_transform(get_progress_token_t) {
		return make_progress_token<TProgress>(this);
	}
};

template<typename TResult> struct promise<IAsyncOperation<TResult>> : promise_base<IAsyncOperation<TResult>, AsyncOperationCompletedHandler<TResult>> {
	TResult result;
public:
	TResult GetResults() {
		auto	guard = with(this->lock);
		CheckErrors();
		return result;
	}

	void return_value(const TResult &_result) {
		AsyncOperationCompletedHandler<TResult> handler;
		AsyncStatus status;
		{
			auto	guard = with(this->lock);
			if (this->Status == AsyncStatus::Started) {
				this->Status = AsyncStatus::Completed;
				result = _result;
			}
			handler = move(this->completed);
			status	= this->Status;
		}
		if (handler)
			handler->_Invoke(this, status);
	}
};

template<typename TResult, typename TProgress> struct promise<IAsyncOperationWithProgress<TResult, TProgress>> : promise_base<IAsyncOperationWithProgress<TResult, TProgress>, AsyncOperationWithProgressCompletedHandler<TResult, TProgress>> {
	typedef decltype(promise::get_base())	base;
	using ProgressHandler	= AsyncOperationProgressHandler<TResult, TProgress>;
	using base::await_transform;

	TResult			result;
public:
	ProgressHandler Progress;

	TResult GetResults() {
		auto	guard = with(this->lock);
		CheckErrors();
		return result;
	}

	void return_value(const TResult &_result) {
		AsyncOperationWithProgressCompletedHandler<TResult, TProgress> handler;
		AsyncStatus status;
		{
			auto	guard = with(this->lock);
			if (this->Status == AsyncStatus::Started) {
				this->Status = AsyncStatus::Completed;
				result	= _result;
			}
			handler = move(this->completed);
			status = this->status;
		}
		if (handler)
			handler->_Invoke(this, status);
	}

	void set_progress(TProgress const& result) {
		if (auto handler = Progress)
			handler(*this, result);
	}
	auto await_transform(get_progress_token_t) {
		return make_progress_token<TProgress>(this);
	}
};
} } // namespace Windows::Foundation

} // namespace iso_winrt

template<typename I, typename... TT> struct std::experimental::coroutine_traits<iso_winrt::ptr<I>, TT...> {
	typedef iso_winrt::Windows::Foundation::promise<I>	promise_type;
};

