//
// File: math.h
//
// Copyright (C) 2023 ranstar74. All rights violated.
//
// Part of "Rage Am" Research Project.
//
#pragma once

#include <cmath>

namespace rage
{
	static constexpr float PI = 3.14159265358979323846f;

	class Math
	{
	public:
		static bool AlmostEquals(float a, float b, float maxDelta = 0.01f) { return abs(a - b) <= maxDelta; }

		template<typename T>
		static const T& Max(const T& left, const T& right) { return left >= right ? left : right; }

		template<typename T>
		static const T& Min(const T& left, const T& right) { return left <= right ? left : right; }

		static constexpr float Clamp(float value, float min, float max)
		{
			if (value < min)
				return min;
			if (value > max)
				return max;
			return value;
		}

		static constexpr float DegToRad(float deg)
		{
			return deg / 180.0f * PI;
		}

		static constexpr float RadToDeg(float rad)
		{
			return rad / PI * 180.0f;
		}

	};
}