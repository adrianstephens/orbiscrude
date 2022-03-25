#include "EKF.h"

namespace iso {
using namespace SO3;

//static const double pi		= 3.141592653589793;
//static const double rsqrt2	= 0.707106781186548;

void Matrix3x3d::transpose(Matrix3x3d &result) {
	result.m[0] = m[0];
	result.m[1] = m[3];
	result.m[2] = m[6];
	result.m[3] = m[1];
	result.m[4] = m[4];
	result.m[5] = m[7];
	result.m[6] = m[2];
	result.m[7] = m[5];
	result.m[8] = m[8];
}

bool Matrix3x3d::invert(Matrix3x3d &result) const {
	double d = determinant();
	if (d == 0.0)
		return false;
	
	double invdet = 1.0 / d;
	result.set(
		(m[4] * m[8] - m[7] * m[5]) * invdet,
		-(m[1] * m[8] - m[2] * m[7]) * invdet,
		(m[1] * m[5] - m[2] * m[4]) * invdet,
		-(m[3] * m[8] - m[5] * m[6]) * invdet,
		(m[0] * m[8] - m[2] * m[6]) * invdet,
		-(m[0] * m[5] - m[3] * m[2]) * invdet,
		(m[3] * m[7] - m[6] * m[4]) * invdet,
		-(m[0] * m[7] - m[6] * m[1]) * invdet,
		(m[0] * m[4] - m[3] * m[1]) * invdet
	);
	return true;
}

Vector3d iso::SO3::ortho(const Vector3d &v) {
	int			k = largestAbsComponent(v);
	Vector3d	temp;
	temp.setComponent(k == 0 ? 2 : k - 1, 1.0);
	return cross(v, temp).normalize();
}

int iso::SO3::largestAbsComponent(const Vector3d &v) {
	double xAbs = iso::abs(v.x);
	double yAbs = iso::abs(v.y);
	double zAbs = iso::abs(v.z);
	if (xAbs > yAbs)
		return xAbs > zAbs ? 0 : 2;
	return yAbs > zAbs ? 1 : 2;
}

Matrix3x3d RodriguesSO3Exp(const Vector3d &w, double kA, double kB) {
	Matrix3x3d	r;
	const double wx2 = w.x * w.x;
	const double wy2 = w.y * w.y;
	const double wz2 = w.z * w.z;
	r.set(0, 0, 1 - kB * (wy2 + wz2));
	r.set(1, 1, 1 - kB * (wx2 + wz2));
	r.set(2, 2, 1 - kB * (wx2 + wy2));
	
	double a, b;
	a = kA * w.z;
	b = kB * (w.x * w.y);
	r.set(0, 1, b - a);
	r.set(1, 0, b + a);
	
	a = kA * w.y;
	b = kB * (w.x * w.z);
	r.set(0, 2, b + a);
	r.set(2, 0, b - a);
	
	a = kA * w.x;
	b = kB * (w.y * w.z);
	r.set(1, 2, b - a);
	r.set(2, 1, b + a);
	return r;
}

Matrix3x3d rotationPiAboutAxis(const Vector3d &v) {
	static const double kA = 0, kB = 2 / (pi * pi);
	return RodriguesSO3Exp(v * (pi / v.length()), kA, kB);
}

Matrix3x3d SO3FromTwoVecN(const Vector3d &a, const Vector3d &b) {
	Vector3d c = cross(a, b);
	
	if (Dot(c, c) == 0.0) {
		if (Dot(a, b) >= 0)
			return iso::identity;
		return rotationPiAboutAxis(ortho(a));
	}
	
	Vector3d an = normalize(a);
	Vector3d bn = normalize(b);
	c.normalize();
	
	Matrix3x3d r1;
	r1.setColumn(0, an);
	r1.setColumn(1, c);
	r1.setColumn(2, cross(c, an));
	
	Matrix3x3d r2;
	r2.setColumn(0, bn);
	r2.setColumn(1, c);
	r2.setColumn(2, cross(c, bn));
	
	r1.transpose();
	return r2 * r1;
}

Matrix3x3d SO3FromMu(const Vector3d &w) {
	const double thetaSq	= Dot(w, w);
	const double theta		= iso::sqrt(thetaSq);
	double kA, kB;
	if (thetaSq < 1.0e-08) {
		kA = 1 - thetaSq / 6;
		kB = 0.5;
	} else if (thetaSq < 1.0e-06) {
		kB = 0.5 - thetaSq / 24;
		kA = 1.0 - thetaSq / 6 * (1 - thetaSq / 6);
	} else {
		const double invTheta = 1.0 / theta;
		kA = sin(theta) * invTheta;
		kB = (1 - cos(theta)) * (invTheta * invTheta);
	}
	return RodriguesSO3Exp(w, kA, kB);
}

Vector3d muFromSO3(const Matrix3x3d &so3) {
	const double cosAngle = (so3.get(0, 0) + so3.get(1, 1) + so3.get(2, 2) - 1) * 0.5;
	Vector3d	r(
		(so3.get(2, 1) - so3.get(1, 2)) / 2,
		(so3.get(0, 2) - so3.get(2, 0)) / 2,
		(so3.get(1, 0) - so3.get(0, 1)) / 2
	);
	
	double sinAngleAbs = r.length();
	if (cosAngle > rsqrt2) {
		if (sinAngleAbs > 0.0)
			r.scale(asin(sinAngleAbs) / sinAngleAbs);
		return r;
		
	} else if (cosAngle > -rsqrt2) {
		return r.scale(acos(cosAngle) / sinAngleAbs);
		
	} else {
		double angle = pi - asin(sinAngleAbs);
		double d0 = so3.get(0, 0) - cosAngle;
		double d1 = so3.get(1, 1) - cosAngle;
		double d2 = so3.get(2, 2) - cosAngle;
		
		Vector3d	r2;
		if (d0 * d0 > d1 * d1 && d0 * d0 > d2 * d2)
			r2.set(d0, (so3.get(1, 0) + so3.get(0, 1)) / 2.0, (so3.get(0, 2) + so3.get(2, 0)) / 2.0);
		else if (d1 * d1 > d2 * d2)
			r2.set((so3.get(1, 0) + so3.get(0, 1)) / 2.0, d1, (so3.get(2, 1) + so3.get(1, 2)) / 2.0);
		else
			r2.set((so3.get(0, 2) + so3.get(2, 0)) / 2.0, (so3.get(2, 1) + so3.get(1, 2)) / 2.0, d2);
		
		if (Dot(r2, r) < 0.0)
			r2.scale(-1.0);
		
		return r2.normalize().scale(angle);
	}
}

void generatorField(int i, const Matrix3x3d &pos, Matrix3x3d &result) {
	result.set(i, 0.0, 0.0);
	result.set((i + 1) % 3, 0, -pos.get((i + 2) % 3, 0));
	result.set((i + 2) % 3, 0,	pos.get((i + 1) % 3, 0));
}

//-----------------------------------------------------------------------------
//	Tracker
//-----------------------------------------------------------------------------
void EKF::reset(double time) {
	sensorTimeStampGyro		= time;
	previousAccelNorm		= 0;
	movingAverageAccelNormChange = 0;
	alignedToGravity		= false;
	alignedToNorth			= false;
	numGyroTimestepSamples	= 0;
	lastGyro				= float3(zero);

	so3SensorFromWorld.setIdentity();
	so3LastMotion.setIdentity();
	mP.setZero();
	mP.setSameDiagonal(25.0);
	mQ.setZero();
	mQ.setSameDiagonal(1.0);
	mR.setZero();
	mR.setSameDiagonal(0.0625);
	mRAcceleration.setZero();
	mRAcceleration.setSameDiagonal(0.5625);
	mS.setZero();
	mH.setZero();
	mK.setZero();
	vNu.setZero();
	vZ.setZero();
	vH.setZero();
	vU.setZero();
	vX.setZero();
	// Flipped from Android so it uses the same convention as CoreMotion (was: _vDown.set(0.0, 0.0, 9.81))
	vDown.set(0.0, 0.0, -9.81);
	vNorth.set(0.0, 1.0, 0.0);
}

double EKF::getHeadingDegrees() const {
	double x = so3SensorFromWorld.get(2, 0);
	double y = so3SensorFromWorld.get(2, 1);
	double mag = sqrt(x * x + y * y);
	if (mag < 0.1)
		return 0.0;
	
	double heading = -90.0 - to_degrees(atan2(y, x));
	return heading < 0.0 ? heading + 360.0 : heading >= 360.0 ? heading - 360.0 : heading;
}

void EKF::setHeadingDegrees(double heading) {
	double currentHeading = getHeadingDegrees();
	double deltaHeading = heading - currentHeading;
	double s = sin(degrees(deltaHeading));
	double c = cos(degrees(deltaHeading));
	Matrix3x3d deltaHeadingRotationMatrix(c, -s, 0.0, s, c, 0.0, 0.0, 0.0, 1.0);
	so3SensorFromWorld = so3SensorFromWorld * deltaHeadingRotationMatrix;
}

iso::float3x3 EKF::getPredictedMatrix(double time) const {
	double dT = time - sensorTimeStampGyro;
	Matrix3x3d so3PredictedMotion	= SO3FromMu(float3(lastGyro * -float(dT)));
	return so3PredictedMotion * so3SensorFromWorld;
}

void EKF::processGyro(param(float3) gyro, double sensorTimeStamp) {
	if (sensorTimeStampGyro != 0.0) {
		double dT = filterGyroTimestep(sensorTimeStamp - sensorTimeStampGyro);
	
		vU = float3(gyro * -float(dT));
		so3LastMotion		= SO3FromMu(vU);
		so3SensorFromWorld	= so3LastMotion * so3SensorFromWorld;
		updateCovariancesAfterMotion();
		Matrix3x3d temp(mQ);
		temp.scale(dT * dT);
		mP += temp;
	}
	sensorTimeStampGyro = sensorTimeStamp;
	lastGyro = gyro;
}

void EKF::processAccel(param(float3) acc, double sensorTimeStamp) {
	vZ.set(acc.x, acc.y, acc.z);
	updateAccelerationCovariance(vZ.length());
	if (alignedToGravity) {
		vNu = accelerationObservationFunctionForNumericalJacobian(so3SensorFromWorld);
		const double eps = 1.0E-7;
		for (int dof = 0; dof < 3; dof++) {
			Vector3d delta;
			delta.setZero();
			delta.setComponent(dof, eps);
			Matrix3x3d	tempM	= SO3FromMu(delta) * so3SensorFromWorld;
			Vector3d	tempV	= vNu - accelerationObservationFunctionForNumericalJacobian(tempM);
			tempV.scale(1.0 / eps);
			mH.setColumn(dof, tempV);
		}
		
		
		Matrix3x3d mHt;
		mH.transpose(mHt);
		Matrix3x3d temp = mP * mHt;
		temp	= mH * temp;
		mS		= temp + mRAcceleration;
		mS.invert(temp);
		mK		= mP * mHt * temp;
		vX		= mK * vNu;
		Matrix3x3d temp2;
		temp2.setIdentity();
		temp2 -= mK * mH;
		mP = temp2 * mP;
		so3LastMotion = SO3FromMu(vX);
		so3SensorFromWorld = so3LastMotion * so3SensorFromWorld;
		updateCovariancesAfterMotion();
		
	} else {
		so3SensorFromWorld = SO3FromTwoVecN(vDown, vZ);
		alignedToGravity = true;
	}
}

double EKF::filterGyroTimestep(double timestep) {
	if (timestep > 0.04f)
		return numGyroTimestepSamples > 10 ? filteredGyroTimestep : 0.01;
		
	const double kFilterCoeff = 0.95;
	if (numGyroTimestepSamples == 0) {
		filteredGyroTimestep = timestep;
	} else {
		filteredGyroTimestep = kFilterCoeff * filteredGyroTimestep + (1.0 - kFilterCoeff) * timestep;
	}
	++numGyroTimestepSamples;
	return timestep;
}

void EKF::updateCovariancesAfterMotion() {
	Matrix3x3d temp;
	so3LastMotion.transpose(temp);
	mP = so3LastMotion * mP * temp;
	so3LastMotion.setIdentity();
}

void EKF::updateAccelerationCovariance(double currentAccelNorm) {
	double currentAccelNormChange = abs(currentAccelNorm - previousAccelNorm);
	previousAccelNorm = currentAccelNorm;
	const double kSmoothingFactor	= 0.5;
	movingAverageAccelNormChange = kSmoothingFactor * movingAverageAccelNormChange + (1.0 - kSmoothingFactor) * currentAccelNormChange;
	const double kMaxAccelNormChange = 0.15;
	const double kMinAccelNoiseSigma = 0.75;
	const double kMaxAccelNoiseSigma = 7.0;
	double normChangeRatio = movingAverageAccelNormChange / kMaxAccelNormChange;
	double accelNoiseSigma = min(kMaxAccelNoiseSigma, kMinAccelNoiseSigma + normChangeRatio * (kMaxAccelNoiseSigma - kMinAccelNoiseSigma));
	mRAcceleration.setSameDiagonal(accelNoiseSigma * accelNoiseSigma);
}

Vector3d EKF::accelerationObservationFunctionForNumericalJacobian(const Matrix3x3d &so3SensorFromWorldPred) {
	vH = so3SensorFromWorldPred * vDown;
	return muFromSO3(SO3FromTwoVecN(vH, vZ));
}
} // namespace iso