#ifndef MOVIE_H
#define MOVIE_H

#include "filetypes/bitmap/bitmap.h"

namespace iso {

class movie {
public:
	ISO_ptr<void>	frames;
	int				width, height;
	float			fps;
	movie()	{}
	movie(ISO_ptr<void> frames, int width, int height, float fps) : frames(frames), width(width), height(height), fps(fps) {}
	template<typename T> movie(ISO_ptr<T> frames) : frames(frames), width(frames->Width()), height(frames->Height()), fps(frames->FrameRate()) {}
};

}//namespace iso

ISO_DEFUSERCOMPV(iso::movie, width, height, fps, frames);

#endif	//MOVIE_H