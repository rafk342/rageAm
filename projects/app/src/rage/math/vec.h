//
// File: vec.h
//
// Copyright (C) 2023 ranstar74. All rights violated.
//
// Part of "Rage Am" Research Project.
//
#pragma once

#include <cmath>

#include "am/system/asserts.h"
#include "helpers/ranges.h"

namespace rage
{
	// For fast math operations use ScalarV and Vec3V

	struct Vec3V;
	struct ScalarV;

	struct Vector2
	{
		float X, Y;

		Vector2();
		Vector2(float c) { X = Y = c; }
		Vector2(float x, float y) { X = x; Y = y; }

		void FromArray(const float* arr) { X = arr[0]; Y = arr[1]; }
		void ToArray(float* arr) const { arr[0] = X; arr[1] = Y; }

		float& operator[](int index)
		{
			switch (index)
			{
			case 0: return X;
			case 1: return Y;
			}
#ifdef DEBUG
			AM_UNREACHABLE("Vector2::operator[] -> Index %u is invalid.", index);
#endif
		}

		Vector2 operator+(const Vector2& other) const { return Vector2(X + other.X, Y + other.Y); }
		Vector2 operator-(const Vector2& other) const { return Vector2(X - other.X, Y - other.Y); }
		Vector2& operator+=(const Vector2& other) { X += other.X; Y += other.Y; return *this; }
		Vector2& operator-=(const Vector2& other) { X -= other.X; Y -= other.Y; return *this; }
		Vector2 operator*(const Vector2& other) const { return Vector2(X * other.X, Y * other.Y); }
		Vector2 operator/(const Vector2& other) const { return Vector2(X / other.X, Y / other.Y); }
		Vector2& operator*=(const Vector2& other) { X *= other.X; Y *= other.Y; return *this; }
		Vector2& operator/=(const Vector2& other) { X /= other.X; Y /= other.Y; return *this; }

		Vector2 operator-() const { return Vector2(-X, -Y); }
	};

	struct Vector3
	{
		float X, Y, Z;

		Vector3();
		Vector3(const Vec3V& v);
		Vector3(const ScalarV& s);
		Vector3(float c) { X = Y = Z = c; }
		Vector3(float x, float y, float z) { X = x; Y = y; Z = z; }

		float Length() const { return sqrtf(X * X + Y * Y + Z * Z); }
		float LengthSquared() const { return X * X + Y * Y + Z * Z; }

		float Min() const { return MIN(X, MIN(Y, Z)); }
		float Max() const { return MAX(X, MAX(Y, Z)); }

		Vector3 operator+(const Vector3& other) const { return Vector3(X + other.X, Y + other.Y, Z + other.Z); }
		Vector3 operator-(const Vector3& other) const { return Vector3(X - other.X, Y - other.Y, Z - other.Z); }
		Vector3& operator+=(const Vector3& other) { X += other.X; Y += other.Y; Z += other.Z; return *this; }
		Vector3& operator-=(const Vector3& other) { X -= other.X; Y -= other.Y; Z -= other.Z; return *this; }
		Vector3 operator*(const Vector3& other) const { return Vector3(X * other.X, Y * other.Y, Z * other.Z); }
		Vector3 operator/(const Vector3& other) const { return Vector3(X / other.X, Y / other.Y, Z / other.Z); }
		Vector3& operator*=(const Vector3& other) { X *= other.X; Y *= other.Y; Z *= other.Z; return *this; }
		Vector3& operator/=(const Vector3& other) { X /= other.X; Y /= other.Y; Z /= other.Z; return *this; }

		Vector3 operator-() const { return Vector3(-X, -Y, -Z); }

		bool operator==(const Vector3& other) const = default;

		void FromArray(const float* arr) { X = arr[0]; Y = arr[1]; Z = arr[2]; }
		void ToArray(float* arr) const { arr[0] = X; arr[1] = Y; arr[2] = Z; }

		float& operator[](int index)
		{
			switch (index)
			{
			case 0: return X;
			case 1: return Y;
			case 2: return Z;
			}
#ifdef DEBUG
			AM_UNREACHABLE("Vector3::operator[] -> Index %u is invalid.", index);
#endif
		}
	};

	struct Vector4
	{
		float X, Y, Z, W;

		Vector4();
		Vector4(float c) { X = Y = Z = W = c; }
		Vector4(float x, float y, float z, float w) { X = x; Y = y; Z = z; W = w; }

		void FromArray(const float* arr) { X = arr[0]; Y = arr[1]; Z = arr[2]; W = arr[3]; }
		void ToArray(float* arr) const { arr[0] = X; arr[1] = Y; arr[2] = Z; arr[3] = W; }

		float& operator[](int index)
		{
			switch (index)
			{
			case 0: return X;
			case 1: return Y;
			case 2: return Z;
			case 3: return W;
			}
#ifdef DEBUG
			AM_UNREACHABLE("Vector4::operator[] -> Index %u is invalid.", index);
#endif
		}
	};
}
