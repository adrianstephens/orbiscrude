typedef unsigned int uint;

struct DCTinfo {
	uint				eob:10, q0:11, q1:11;
	uint				x:16, y:16;
	uint				coeffs;
};

struct PREDinfo {
	uint	mode:6, x:13, y:13;
};
struct PREDinfo2 {
	uint	mode:6, x:13, y:13;
	uint	key;
};

struct INTERinfo {
	uint	filter:2, ref0:2, ref1:2, split:2, mvs:24;
};

struct MotionVector {
	int		row:16, col:16;
};
