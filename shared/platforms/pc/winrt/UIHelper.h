#include "pch.h"

namespace Common {
	using namespace iso_winrt;
	using namespace Platform;
	using namespace Windows;
	using namespace Collections;
	using namespace Windows::UI::Xaml;
	using namespace Windows::UI::Xaml::Automation;
	using namespace Windows::UI::Xaml::Media;

	generator<ptr<DependencyObject>> GetDescendants(ptr<DependencyObject> start);

	template<typename T> generator<ptr<T>> GetDescendantsOfType(ptr<DependencyObject> start) {
		for (ptr<DependencyObject> i : GetDescendants(start)) {
			if (auto j = (ptr<T>)i)
				co_yield j;
		}
	}

	ptr<DependencyObject> FindDescendant(ptr<DependencyObject> start, hstring_ref name);

}
