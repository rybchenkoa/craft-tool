#pragma once

//программные числа с плавающей точкой, заточенные под быструю работу без всяких проверок

struct float16
{
	short mantis;
	short exponent;

	float16(void)
	{
		//mantis = 0;
		//exponent = 0;
	}

	float16(short _mantis, short _exponent)
	{
		mantis = _mantis;
		exponent = _exponent;
	}

	float16(float value)
	{
		int input = *(int*)&value;
		mantis = ((1<<30) | ((input & 0x007FFFFF)<<7))>>16;
		if(input & (1u<<31))
			mantis = -mantis;
		int exp = (input << 1) & 0xFF000000;
		exponent = exp>>24u;
		exponent = int8_t(exponent + 0x81);
	}
	
	operator float() const
	{
		int result;
		int exp = exponent - 0x81;
		int mant = mantis > 0 ? mantis : -mantis;
		result = ((mantis << 16) & (1u<<31)) | ((exp & 0xFF) << 23) | ((mant << 9) & 0x007FFFFF);
		return *(float*)&result;
	}
	
	float16(int value)
	{
		int mant = value > 0 ? value : -value;
		int pos = __clz(mant);
		exponent = 32 - pos - 1;
		if(pos > 16)
			mantis = value << (pos - 17);
		else
			mantis = value >> (17 - pos);
	}

	operator int() const
	{
		if(exponent > 14)
			return int(mantis) << (exponent - 14);
		else
			return int(mantis) >> (14 - exponent);
	}

	float16 operator * (float16 operand)
	{ //1+15*2 - максимум 31 //1+1+14*2 - минимум 31 знак
		short _mantis, _exponent;
		int mant = (int(mantis)*operand.mantis)>>14;
		int mantVal = mant > 0 ? mant : -mant;
		_exponent = exponent + operand.exponent;
		if(mantVal >= (1<<15))
		{
			_mantis = (mant >> 1);
			_exponent++;
		}
		else
			_mantis = mant;
		
		return float16(_mantis, _exponent);
	}

	float16 operator / (float16 operand)
	{
		short _mantis, _exponent;
		int mant = (int(mantis)<<15)/operand.mantis;
		int mantVal = mant > 0 ? mant : -mant;
		_exponent = exponent - operand.exponent;
		if(mantVal >= (1<<15))
			_mantis = (mant >> 1);
		else
		{
			_mantis = mant;
			_exponent--;
		}

		return float16(_mantis, _exponent);
	}
	
	float16 operator + (float16 operand) const
	{
		short _mantis, _exponent;
		int mant;
		int exp;
		if(exponent < operand.exponent)
		{
			mant = (int(mantis)>>(operand.exponent - exponent)) + operand.mantis;
			exp = operand.exponent;
		}
		else
		{
			mant = (int(operand.mantis)>>(exponent - operand.exponent)) + mantis;
			exp = exponent;
		}

		int mantVal = mant > 0 ? mant : -mant;
		int pos = 17 - __clz(mantVal);
		if(pos > 16)
			_mantis = mant << -pos;
		else
			_mantis = mant >> pos;
		_exponent = exp + pos;

		return float16(_mantis, _exponent);
	}

	float16 operator - (float16 operand) const
	{
		short _mantis, _exponent;
		int mant;
		int exp;
		if(exponent < operand.exponent)
		{
			mant = (int(mantis)>>(operand.exponent - exponent)) - operand.mantis;
			exp = operand.exponent;
		}
		else
		{
			mant = mantis - (int(operand.mantis)>>(exponent - operand.exponent));
			exp = exponent;
		}

		int mantVal = mant > 0 ? mant : -mant;
		int pos = 17 - __clz(mantVal);
		if(pos > 16)
			_mantis = mant << -pos;
		else
			_mantis = mant >> pos;
		_exponent = exp + pos;

		return float16(_mantis, _exponent);
	}
	
	bool operator < (float16 value) const
	{
		if (mantis < 0)
		{
			if(value.mantis >= 0) //-1 < +1
				return true;
			
			//if(value.mantis == 0) //-1 < 0*10^5
			//	return false;
			
			if(exponent > value.exponent) // -100 < -1
				return true;
				
			if(exponent < value.exponent) // -1 < -100
				return false;
			
			if(mantis < value.mantis) //-2 < -1
				return true;
				
			return false;
		}
		else 
		{
			if(value.mantis < 0)         //1 < -1
				return false;
			
			if(mantis == 0)              //0 < 1
				return (value.mantis > 0);
				
			if(exponent < value.exponent) // 1 < 10
				return true;
				
			if(exponent > value.exponent) // 10 < 1
				return false;
				
			if(mantis < value.mantis) //1 < 2
				return true;
				
			return false;
		}
	}
	
	bool operator > (float16 value) const
	{
		return value < *this;
	}
	
	float16& operator += (float16 value)
	{
		*this = *this + value;
		return *this;
	}
	
	float16 operator << (int i) const
	{
		float16 result = *this;
		result.exponent += i;
		return result;
	}
	
	float16 operator >> (int i) const
	{
		float16 result = *this;
		result.exponent -= i;
		return result;
	}
};

float16 sqrt(float16 value)
{
	short _mantis, _exponent;
	if(value.mantis <= 0)
		return float16(0, 0);

	unsigned int val = value.mantis << 16u;
	if((value.exponent & 1) == 1)
		val = val << 1u; //бит знака = 0, ничего не затрём
	_exponent = value.exponent >> 1;

	unsigned int res = value.mantis;
	res = (val/res + res)/2;
	res = (val/res + res)/2;
	res = (val/res + res)/2;
	//res = (val/res + res)/2;
	//res = (val/res + res)/2;
	//res = (val/res + res)/2;

	if(res > INT16_MAX * 2) //либо увеличить число итераций
		res = INT16_MAX * 2;
		
	_mantis = res>>1u;
	return float16(_mantis, _exponent);
}

float16 pow2(float16 value)
{
	return value * value;
}

float16 abs(float16 value)
{
	short _mantis, _exponent;
	_exponent = value.exponent;
	_mantis = (value.mantis > 0 ? value.mantis : -value.mantis);
	return float16(_mantis, _exponent);
}
