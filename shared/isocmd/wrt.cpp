#pragma comment(lib, "windowsapp")
#include "winrt/Windows.Foundation.h"
#include "winrt/Windows.Web.Syndication.h"
using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Web::Syndication;


struct test_wrt {
	int init() {
		init_apartment();
		Uri uri(L"http://kennykerr.ca/feed");
		SyndicationClient client;
		SyndicationFeed feed = client.RetrieveFeedAsync(uri).get();
		for (SyndicationItem item : feed.Items()) {
			hstring title = item.Title().Text();
			printf("%ls\n", title.c_str());
		}
		return 0;
	}
	test_wrt() {
		init();
	}
};// _test_wrt;

void testy() {
	test_wrt();
}