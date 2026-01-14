#pragma once

#define MAX_AXES   5 //сколько всего есть осей на контроллере
using coord = double;//чтобы не путаться, координатный тип введём отдельно

struct Coords   //все координаты устройства
{
	union
	{
		struct
		{
			coord x, y, z, a, b;
		};
		struct
		{
			coord r[MAX_AXES];
		};
	};

	Coords() { for (int i = 0; i < MAX_AXES; ++i) r[i] = 0; };

	Coords operator+(const Coords& right) const
	{
		Coords result;
		for (int i = 0; i < MAX_AXES; ++i) {
			result.r[i] = r[i] + right.r[i];
		}
		return result;
	}

	Coords operator-(const Coords& right) const
	{
		Coords result;
		for (int i = 0; i < MAX_AXES; ++i) {
			result.r[i] = r[i] - right.r[i];
		}
		return result;
	}

	Coords& operator+=(const Coords& right)
	{
		for (int i = 0; i < MAX_AXES; ++i) {
			r[i] += right.r[i];
		}
		return *this;
	}

	Coords& operator-=(const Coords& right)
	{
		for (int i = 0; i < MAX_AXES; ++i) {
			r[i] -= right.r[i];
		}
		return *this;
	}
};
