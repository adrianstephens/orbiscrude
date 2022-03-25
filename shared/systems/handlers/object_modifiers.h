#ifndef OBJECT_MODIFIERS_H
#define OBJECT_MODIFIERS_H

#include "object.h"

void FadeObject(iso::Object *obj, float value, float rate, bool hierarchy = true);
void FollowBone(iso::Object *obj, iso::Object *skin_obj, iso::crc32 bone);
void UnfollowBone(iso::Object *obj);

#endif // OBJECT_MODIFIERS_H
