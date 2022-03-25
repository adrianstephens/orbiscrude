#ifndef EKF_H
#define EKF_H

#include "base/defs.h"
#include "base/vector.h"

namespace iso { namespace SO3 {

struct Vector3d {
	double x, y, z;
	
	Vector3d()	: x(0), y(0), z(0) {}
	Vector3d(double _x, double _y, double _z)		: x(_x), y(_y), z(_z) {}
	Vector3d(param(float3) v)						: x(v.x), y(v.y), z(v.z) {}
	Vector3d&	set(double _x, double _y, double _z)	{ x = _x; y = _y; z = _z; return *this; }
	Vector3d&	setComponent(int i, double val)			{ (&x)[i] = val; return *this; }
	Vector3d&	setZero()								{ x = y = z = 0; return *this; }
	double		length()						const	{ return sqrt(x * x + y * y + z * z); }
	Vector3d&	scale(double s)							{ x *= s; y *= s; z *= s; return *this; }
	Vector3d&	normalize()								{ if (double d = length()) scale(1.0 / d); return *this; }
	Vector3d&	operator*=(double s)					{ x *= s; y *= s; z *= s; return *this; }
	
	friend double	Dot(const Vector3d &a, const Vector3d &b)		{ return a.x * b.x + a.y * b.y + a.z * b.z; }
	friend Vector3d	operator-(const Vector3d &a, const Vector3d &b)	{ return Vector3d(a.x - b.x, a.y - b.y, a.z - b.z); }
	friend bool		operator==(const Vector3d &a, const Vector3d &b){ return a.x == b.x && a.y == b.y && a.z == b.z; }
	friend Vector3d	operator*(const Vector3d &a, float s)			{ return Vector3d(a) *= s; }
	friend Vector3d	normalize(const Vector3d &a)					{ return Vector3d(a).normalize(); }
	friend Vector3d	cross(const Vector3d &a, const Vector3d &b)		{ return Vector3d(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x); }
	friend Vector3d	ortho(const Vector3d &v);
	friend int		largestAbsComponent(const Vector3d &v);
};

struct Matrix3x3d {
	double m[9];
	
	Matrix3x3d()					{ setZero(); }
	Matrix3x3d(const _identity&)	{ setIdentity(); }
	Matrix3x3d(double m00, double m01, double m02,  double m10, double m11, double m12,  double m20, double m21, double m22) {
		m[0] = m00; m[1] = m01; m[2] = m02;
		m[3] = m10; m[4] = m11; m[5] = m12;
		m[6] = m20; m[7] = m21; m[8] = m22;
	}
	
	void set(double m00, double m01, double m02,  double m10, double m11, double m12,  double m20, double m21, double m22) {
		m[0] = m00; m[1] = m01; m[2] = m02;
		m[3] = m10;	m[4] = m11; m[5] = m12;
		m[6] = m20;	m[7] = m21; m[8] = m22;
	}
	void setZero() {
		for (int i = 0; i < 9; i++)
			m[i] = 0;
	}
	void setIdentity()				{ set(1,0,0, 0,1,0, 0,0,1); }
	void setSameDiagonal(double d)	{ m[0] = m[4] = m[8] = d; }
	
	double		get(int row, int col)	const			{ return m[(3 * row + col)]; }
	void		set(int row, int col, double value)		{ m[(3 * row + col)] = value; }
	
	Vector3d	getColumn(int col)		const			{ return Vector3d(m[col], m[col + 3], m[col + 6]); }
	void		setColumn(int col, const Vector3d &v)	{ m[col] = v.x; m[col + 3] = v.y; m[col + 6] = v.z; }
	
	void		scale(double s)							{ for (int i = 0; i < 9; i++) m[i] *= s; }
	void		operator+=(const Matrix3x3d &b)			{ for (int i = 0; i < 9; i++) m[i] += b.m[i]; }
	void		operator-=(const Matrix3x3d &b)			{ for (int i = 0; i < 9; i++) m[i] -= b.m[i]; }
	
	void	transpose()									{ swap(m[1], m[3]); swap(m[2], m[6]); swap(m[5], m[7]); }
	void	transpose(Matrix3x3d &result);
	double	determinant() const {
		return get(0, 0) * (get(1, 1) * get(2, 2) - get(2, 1) * get(1, 2))
		- get(0, 1) * (get(1, 0) * get(2, 2) - get(1, 2) * get(2, 0))
		+ get(0, 2) * (get(1, 0) * get(2, 1) - get(1, 1) * get(2, 0));
	}
	
	bool	invert(Matrix3x3d &result) const;
	
	operator float3x3() const {
		return float3x3(
			float3{(float)m[0], (float)m[3], (float)m[6]},
			float3{(float)m[1], (float)m[4], (float)m[7]},
			float3{(float)m[2], (float)m[5], (float)m[8]}
		);
	}
	
	friend Matrix3x3d operator+(const Matrix3x3d &a, const Matrix3x3d &b) {
		Matrix3x3d	r;
		for (int i = 0; i < 9; i++)
			r.m[i] = a.m[i] + b.m[i];
		return r;
	}
	friend Matrix3x3d operator*(const Matrix3x3d &a, const Matrix3x3d &b) {
		return Matrix3x3d(
			a.m[0] * b.m[0] + a.m[1] * b.m[3] + a.m[2] * b.m[6],
			a.m[0] * b.m[1] + a.m[1] * b.m[4] + a.m[2] * b.m[7],
			a.m[0] * b.m[2] + a.m[1] * b.m[5] + a.m[2] * b.m[8],
			a.m[3] * b.m[0] + a.m[4] * b.m[3] + a.m[5] * b.m[6],
			a.m[3] * b.m[1] + a.m[4] * b.m[4] + a.m[5] * b.m[7],
			a.m[3] * b.m[2] + a.m[4] * b.m[5] + a.m[5] * b.m[8],
			a.m[6] * b.m[0] + a.m[7] * b.m[3] + a.m[8] * b.m[6],
			a.m[6] * b.m[1] + a.m[7] * b.m[4] + a.m[8] * b.m[7],
			a.m[6] * b.m[2] + a.m[7] * b.m[5] + a.m[8] * b.m[8]
		);
	}
	friend Vector3d operator*(const Matrix3x3d &a, const Vector3d &v) {
		return Vector3d(
			a.m[0] * v.x + a.m[1] * v.y + a.m[2] * v.z,
			a.m[3] * v.x + a.m[4] * v.y + a.m[5] * v.z,
			a.m[6] * v.x + a.m[7] * v.y + a.m[8] * v.z
		);
	}
};

//-----------------------------------------------------------------------------
//	Tracker
//-----------------------------------------------------------------------------

class EKF {
	Matrix3x3d	so3SensorFromWorld;
	Matrix3x3d	so3LastMotion;
	Matrix3x3d	mP;
	Matrix3x3d	mQ;
	Matrix3x3d	mR;
	Matrix3x3d	mRAcceleration;
	Matrix3x3d	mS;
	Matrix3x3d	mH;
	Matrix3x3d	mK;
	Vector3d	vNu;
	Vector3d	vZ;
	Vector3d	vH;
	Vector3d	vU;
	Vector3d	vX;
	Vector3d	vDown;
	Vector3d	vNorth;
	double		sensorTimeStampGyro;
	float3		lastGyro;
	double		previousAccelNorm;
	double		movingAverageAccelNormChange;
	double		filteredGyroTimestep;
	int			numGyroTimestepSamples;
	bool		alignedToGravity;
	bool		alignedToNorth;
	
	double		filterGyroTimestep(double timestep);
	void		updateCovariancesAfterMotion();
	void		updateAccelerationCovariance(double currentAccelNorm);
	Vector3d	accelerationObservationFunctionForNumericalJacobian(const Matrix3x3d &so3SensorFromWorldPred);
public:
	EKF()	{ reset(0); }
	void		reset(double time);
	void		processGyro(param(float3) gyro, double sensorTimeStamp);
	void		processAccel(param(float3) acc, double sensorTimeStamp);
	
	double		getHeadingDegrees()	const;
	void		setHeadingDegrees(double heading);
	
	bool		isReady()			const { return alignedToGravity; }
	float3x3	getMatrix()			const { return so3SensorFromWorld; }
	float3x3	getPredictedMatrix(double time) const;
};
	
} } // namespace iso::SO3

#endif // EKF_H
