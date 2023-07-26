#ifndef OODLE_H
#define OODLE_H

#include "codec.h"

namespace oodle {
using namespace iso;
enum { SAFE_SPACE = 64};

int decompress(const uint8* src, size_t src_len, uint8* dst, size_t dst_len);

} //namespace oodle

#endif //OODLE_H
