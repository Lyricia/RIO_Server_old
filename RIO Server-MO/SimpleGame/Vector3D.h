#pragma once

#include <Windows.h>

template<typename Ty>
class Vector3D {
private:
	const float EffectiveEffer = 0.01f;

public:
	Ty x;
	Ty y;
	Ty z;
	Vector3D() : x(0), y(0), z(0) {}
	template<typename OTy>
	Vector3D(OTy X, OTy Y, OTy Z) : x(static_cast<Ty>(X)), y(static_cast<Ty>(Y)), z(static_cast<Ty>(Z)) {}
	Vector3D& operator+=(const Vector3D& other) { x += other.x; y += other.y; z += other.z; return *this; }
	Vector3D& operator-=(const Vector3D& other) { x -= other.x; y -= other.y; z += other.z; return *this; }
	Vector3D& operator*=(const float f) { x *= f; y *= f; z *= f; return *this; }
	Vector3D& operator*=(const int i) { x *= i; y *= i; z *= i; return *this; }
	Vector3D& operator=(const Vector3D& other) { x = other.x; y = other.y; z = other.z; return *this; }

	Vector3D Normalize()
	{
		float fLength = Length(); 
		if (fLength > EffectiveEffer)
		{
			static_cast<float>(x) /= fLength; static_cast<float>(y) /= fLength; static_cast<float>(z) /= fLength;
		}
		return *this;
	}

	constexpr float Length() const { return(sqrt(x * x + y * y + z * z)); }

	bool operator==(const Vector3D& other) const
	{
		return(fabs(x - other.x) < EffectiveEffer && fabs(y - other.y) < EffectiveEffer && fabs(z - other.z) < EffectiveEffer);
	}
};

using Vec3f = Vector3D<float>;
using Vec3i = Vector3D<int>;

template<typename Ty>
inline Vector3D<Ty> operator+(const Vector3D<Ty> a, const Vector3D<Ty> b) { return Vector3D<Ty>(a.x + b.x, a.y + b.y, a.z + b.z); }
template<typename Ty>
inline Vector3D<Ty> operator-(const Vector3D<Ty> a, const Vector3D<Ty> b) { return Vector3D<Ty>(a.x - b.x, a.y - b.y, a.z - b.z); }

template<typename Ty>
inline Vector3D<Ty> operator*(const float a, const Vector3D<Ty> b) { return Vector3D<Ty>(a * b.x, a * b.y, a * b.z); }
template<typename Ty>
inline Vector3D<Ty> operator*(const Vector3D<Ty> a, const float b) { return Vector3D<Ty>(b * a.x, b * a.y, b * a.z); }

template<typename Ty>
inline constexpr float DotProduct(const Vector3D<Ty> a, const Vector3D<Ty> b) { return (a.x * b.x + a.y * b.y + a.z * b.z); }
template<typename Ty>
inline Vector3D<Ty> CrossProduct(const Vector3D<Ty> a, const Vector3D<Ty> b) { return Vector3D<Ty>(a.y * b.z - a.z * b.y, a.x * b.z - a.z * b.x, a.x * b.y - a.y * b.x); }
