/*
 *	 Copyright (c) 2022 - Arclight Team
 *
 *	 This file is part of Arclight. All rights reserved.
 *
 *	 worley.hpp
 */

#pragma once

#include "noisebase.hpp"


enum class WorleyNoiseFlag {
	None,
	Second,
	Diff,
};


template<NoiseFractal Fractal = NoiseFractal::Standard, WorleyNoiseFlag Flag = WorleyNoiseFlag::None>
class WorleyNoiseBase : public NoiseBase {

public:

	using FlagT = WorleyNoiseFlag;


	template<Float F, Arithmetic A>
	constexpr F sample(F point, A frequency) const {

		using I = TT::ToInteger<F>;
		using UI = TT::MakeUnsigned<I>;

		constexpr F max = 2;

		point *= frequency;

		I ip = Math::floor(point);

		F p = point - ip;

		F first = max;
		F second = max;

		for (const auto& ofsx : offsets1D) {

			UI hx = Math::abs(ip + ofsx) & hashMask;

			F gx = gradient<F>[hash(hx) & grad1DMask];

			gx = gx / 2 + 0.5 + ofsx;

			F dist = Math::abs(p - gx);

			updateDistances(first, second, dist);

		}

		F sample = applyFlag(first, second) / max * 2 - 1;

		return applyFractal<Fractal>(sample);

	}

	template<FloatVector V, Arithmetic A> requires(V::Size == 2)
	constexpr typename V::Type sample(V point, A frequency) const {

		using F = typename V::Type;
		using I = TT::ToInteger<F>;
		using UI = TT::MakeUnsigned<I>;

		constexpr F max = 1.41421356237; // sqrt(2)

		point *= frequency;

		F x = point.x;
		F y = point.y;

		I ipx = Math::floor(x);
		I ipy = Math::floor(y);

		F px = x - ipx;
		F py = y - ipy;

		F first = max;
		F second = max;

		for (const auto& [ofsx, ofsy] : offsets2D) {

			UI hx = Math::abs(ipx + ofsx) & hashMask;
			UI hy = Math::abs(ipy + ofsy) & hashMask;

			auto [gx, gy] = gradient<V>[hash(hx, hy) & grad2DMask];

			gx = gx / 2 + 0.5 + ofsx;
			gy = gy / 2 + 0.5 + ofsy;

			F dist = V(px, py).distance(V(gx, gy));

			updateDistances(first, second, dist);

		}

		F sample = applyFlag(first, second) / max * 2 - 1;

		return applyFractal<Fractal>(sample);

	}

	template<FloatVector V, Arithmetic A> requires(V::Size == 3)
	constexpr typename V::Type sample(V point, A frequency) const {

		using F = typename V::Type;
		using I = TT::ToInteger<F>;
		using UI = TT::MakeUnsigned<I>;

		constexpr F max = 1.73205080756; // sqrt(3)

		point *= frequency;

		F x = point.x;
		F y = point.y;
		F z = point.z;

		I ipx = Math::floor(x);
		I ipy = Math::floor(y);
		I ipz = Math::floor(z);

		F px = x - ipx;
		F py = y - ipy;
		F pz = z - ipz;

		F first = max;
		F second = max;

		for (const auto& [ofsx, ofsy, ofsz] : offsets3D) {

			UI hx = Math::abs(ipx + ofsx) & hashMask;
			UI hy = Math::abs(ipy + ofsy) & hashMask;
			UI hz = Math::abs(ipz + ofsz) & hashMask;

			auto [gx, gy, gz] = gradient<V>[hash(hx, hy, hz) & grad3DMask];

			gx = gx / 2 + 0.5 + ofsx;
			gy = gy / 2 + 0.5 + ofsy;
			gz = gz / 2 + 0.5 + ofsz;

			F dist = V(px, py, pz).distance(V(gx, gy, gz));

			updateDistances(first, second, dist);

		}

		F sample = applyFlag(first, second) / max * 2 - 1;

		return applyFractal<Fractal>(sample);

	}

	template<FloatVector V, Arithmetic A> requires(V::Size == 4)
	constexpr typename V::Type sample(V point, A frequency) const {

		using F = typename V::Type;
		using I = TT::ToInteger<F>;
		using UI = TT::MakeUnsigned<I>;

		constexpr F max = 2; // sqrt(4)

		point *= frequency;

		F x = point.x;
		F y = point.y;
		F z = point.z;
		F w = point.w;

		I ipx = Math::floor(x);
		I ipy = Math::floor(y);
		I ipz = Math::floor(z);
		I ipw = Math::floor(w);

		F px = x - ipx;
		F py = y - ipy;
		F pz = z - ipz;
		F pw = w - ipw;

		F first = max;
		F second = max;

		for (const auto& [ofsx, ofsy, ofsz, ofsw] : offsets4D) {

			UI hx = Math::abs(ipx + ofsx) & hashMask;
			UI hy = Math::abs(ipy + ofsy) & hashMask;
			UI hz = Math::abs(ipz + ofsz) & hashMask;
			UI hw = Math::abs(ipw + ofsw) & hashMask;

			auto [gx, gy, gz, gw] = gradient<V>[hash(hx, hy, hz, hw) & grad3DMask];

			gx = gx / 2 + 0.5 + ofsx;
			gy = gy / 2 + 0.5 + ofsy;
			gz = gz / 2 + 0.5 + ofsz;
			gw = gw / 2 + 0.5 + ofsw;

			F dist = V(px, py, pz, pw).distance(V(gx, gy, gz, gw));

			updateDistances(first, second, dist);

		}

		F sample = applyFlag(first, second) / max * 2 - 1;

		return applyFractal<Fractal>(sample);

	}

	template<class T, Arithmetic A, Arithmetic L, Arithmetic P> requires(Float<T> || FloatVector<T>)
	constexpr auto sample(const T& point, A frequency, u32 octaves, L lacunarity, P persistence) const -> TT::CommonArithmeticType<T> {
		return fractalSample<Fractal>([this](T p, A f) constexpr { return sample(p, f); }, point, frequency, octaves, lacunarity, persistence);
	}

private:

	template<Float F>
	static constexpr void updateDistances(F& first, F& second, F dist) {
		if constexpr (Flag == FlagT::None) {

			first = Math::min(first, dist);

		} else {

			second = Math::min(second, dist);

			if (Math::less(dist, first)) {
				second = first;
				first = dist;
			}

		}
	}


	template<Float F>
	static constexpr F applyFlag(F first, F second) {
		if constexpr (Flag == FlagT::Second) {
			return second;
		} else if constexpr (Flag == FlagT::Diff) {
			return second - first;
		} else {
			return first;
		}
	}


	template<class T, SizeT Size> requires(Integer<T> || IntegerVector<T>)
	static constexpr auto generateOffsets() -> std::array<T, Size> {

		using I = TT::CommonArithmeticType<T>;

		std::array<T, Size> offsets;

		for (u32 i = 0; auto& ofs : offsets) {

			if constexpr (Integer<T>) {

				ofs = T(i % 3 - 1);

			} else {

				constexpr I sizes[] = {1, 3, 9, 27};

				for (u32 j = 0; j < T::Size; j++) {
					ofs[j] = I(i / sizes[j] % 3 - 1);
				}

			}

			i++;

		}

		return offsets;

	}

	static constexpr auto offsets1D = generateOffsets<i32, 3>();

	static constexpr auto offsets2D = generateOffsets<Vec2i, 9>();

	static constexpr auto offsets3D = generateOffsets<Vec3i, 27>();

	static constexpr auto offsets4D = generateOffsets<Vec4i, 81>();

};


using WorleyNoise				= WorleyNoiseBase<NoiseFractal::Standard,	WorleyNoiseFlag::None>;
using WorleyNoise2nd			= WorleyNoiseBase<NoiseFractal::Standard,	WorleyNoiseFlag::Second>;
using WorleyNoiseDiff			= WorleyNoiseBase<NoiseFractal::Standard,	WorleyNoiseFlag::Diff>;
using WorleyNoiseRidged			= WorleyNoiseBase<NoiseFractal::Ridged,		WorleyNoiseFlag::None>;
using WorleyNoiseRidged2nd		= WorleyNoiseBase<NoiseFractal::Ridged,		WorleyNoiseFlag::Second>;
using WorleyNoiseRidgedDiff		= WorleyNoiseBase<NoiseFractal::Ridged,		WorleyNoiseFlag::Diff>;
using WorleyNoiseRidgedSq		= WorleyNoiseBase<NoiseFractal::RidgedSq,	WorleyNoiseFlag::None>;
using WorleyNoiseRidgedSq2nd	= WorleyNoiseBase<NoiseFractal::RidgedSq,	WorleyNoiseFlag::Second>;
using WorleyNoiseRidgedSqDiff	= WorleyNoiseBase<NoiseFractal::RidgedSq,	WorleyNoiseFlag::Diff>;
