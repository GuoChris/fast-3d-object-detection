#pragma once

#include <opencv2/opencv.hpp>
#include "utils.h"

class QuantizedTripletValues
{
public:
	uchar d1, d2, d3, phi1, phi2, phi3;
	int tripletIndex;

	QuantizedTripletValues(const int tripletIndex, const uchar d1, const uchar d2, const uchar d3,
		const uchar phi1, const uchar phi2, const uchar phi3)
	{
		this->d1 = d1;
		this->d2 = d2;
		this->d3 = d3;

		this->phi1 = phi1;
		this->phi2 = phi2;
		this->phi3 = phi3;

		this->tripletIndex = tripletIndex;
	}

	bool operator==(const QuantizedTripletValues &cmp) const
	{
		return (this->d1 == cmp.d1 &&
			this->d2 == cmp.d2 &&
			this->d3 == cmp.d3 &&

			this->phi1 == cmp.phi1 &&
			this->phi2 == cmp.phi2 &&
			this->phi3 == cmp.phi3 &&

			this->tripletIndex == cmp.tripletIndex);
	}
};
namespace std {
	template <>
	struct hash<QuantizedTripletValues>
	{
		std::size_t operator()(const QuantizedTripletValues& k) const
		{
			return improveIntHash((k.tripletIndex << 18) | (k.d1 << 15) | (k.d2 << 12) | (k.d3 << 9) | (k.phi1 << 6) | (k.phi2 << 3) | (k.phi3 << 0));
			
		}
	};
}
