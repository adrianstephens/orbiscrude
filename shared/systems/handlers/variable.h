#ifndef VARIABLE_H
#define VARIABLE_H

#include "object.h"

template<typename T> bool CreateInterpolator0(iso::Object *obj, iso::crc32 id, const T &t, float attack, float sustain, float release);
template<typename T> bool CreateInterpolator(iso::Object *obj, iso::crc32 id, const T &t, float attack, float sustain, float release);
void FlashObject(iso::Object *obj, param(iso::colour) c, float attack, float sustain, float release);

#endif