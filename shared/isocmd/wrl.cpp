// wrl-consume-component.cpp
// compile with: runtimeobject.lib
#include <Windows.Foundation.h>
#include <wrl\wrappers\corewrappers.h>
#include <wrl\client.h>
#include <stdio.h>

typedef Microsoft::WRL::Wrappers::HString	string;

//#undef GetCurrentTime
//template<typename T> struct rt_name;
//#include "d:\test.rth"

using namespace ABI::Windows::Foundation;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

// Prints an error string for the provided source code line and HRESULT
// value and returns the HRESULT value as an int.
int PrintError(unsigned int line, HRESULT hr) {
	wprintf_s(L"ERROR: Line:%d HRESULT: 0x%X\n", line, hr);
	return hr;
}

struct test_wrl {
	int init() {
		// Initialize the Windows Runtime.
		RoInitializeWrapper initialize(RO_INIT_MULTITHREADED);
		if (FAILED(initialize)) {
			return PrintError(__LINE__, initialize);
		}

		// Get the activation factory for the IUriRuntimeClass interface.
		ComPtr<IUriRuntimeClassFactory> uriFactory;
		HRESULT hr = GetActivationFactory(HStringReference(RuntimeClass_Windows_Foundation_Uri).Get(), &uriFactory);
		if (FAILED(hr)) {
			return PrintError(__LINE__, hr);
		}

		// Create a string that represents a URI.
		HString uriHString;
		hr = uriHString.Set(L"http://www.microsoft.com");
		if (FAILED(hr)) {
			return PrintError(__LINE__, hr);
		}

		// Create the IUriRuntimeClass object.
		ComPtr<IUriRuntimeClass> uri;
		hr = uriFactory->CreateUri(uriHString.Get(), &uri);
		if (FAILED(hr)) {
			return PrintError(__LINE__, hr);
		}

		// Get the domain part of the URI.
		HString domainName;
		hr = uri->get_Domain(domainName.GetAddressOf());
		if (FAILED(hr)) {
			return PrintError(__LINE__, hr);
		}

		// Print the domain name and return.
		wprintf_s(L"Domain name: %s\n", domainName.GetRawBuffer(nullptr));

		// All smart pointers and RAII objects go out of scope here.
	}
	test_wrl() {
		init();
	}
};// _test_wrl;