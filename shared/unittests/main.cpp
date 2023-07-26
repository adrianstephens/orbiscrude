#include "events.h"

using namespace iso;

int main() {
	AppEvent(AppEvent::BEGIN).send();
	return 0;
}