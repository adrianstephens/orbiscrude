#include "pch.h"

namespace Common {
using namespace iso_winrt;
using namespace Platform;
using namespace Windows;
using namespace Collections;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Automation;
using namespace Windows::UI::Xaml::Media;

generator<ptr<DependencyObject>> GetDescendants(ptr<DependencyObject> start) {
	dynamic_array_de<ptr<DependencyObject>>	queue;

	for (int i = 0, n = VisualTreeHelper::GetChildrenCount(start); i < n; i++) {
		auto child = VisualTreeHelper::GetChild(start, i);
		co_yield child;
		queue.push_back(child);
	}

	while (!queue.empty()) {
		auto parent = queue.pop_front_value();
		for (int i = 0, n = VisualTreeHelper::GetChildrenCount(parent); i < n; i++) {
			auto child = VisualTreeHelper::GetChild(parent, i);
			co_yield child;
			queue.push_back(child);
		}
	}
}

ptr<DependencyObject> FindDescendant(ptr<DependencyObject> start, hstring_ref name) {
	for (auto i : GetDescendants(start)) {
		ptr<Windows::UI::Xaml::FrameworkElement> child = i;
		hstring	childname = child->Name();
		if (childname == name)
			return child;
	}
	return nullptr;
}

}
