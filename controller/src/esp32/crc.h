#pragma once
// расчёт контрольной суммы

#define CRC32_POLY 0x04C11DB7
struct Crc
{
	uint32_t crc32Table[256];
	
	void init()
	{
		uint32_t crc;
		for (int i = 0; i < 256; ++i)
		{
			crc = i << 24;
			for (int j = 8; j > 0; --j)
				crc = crc & 0x80000000 ? (crc << 1) ^ CRC32_POLY : (crc << 1);
			crc32Table[i] = crc;
		}
	}

	uint32_t calc(char *buffer, int size)
	{
		uint32_t *pInt = (uint32_t*)buffer;
		uint32_t v;
		uint32_t crc;
		crc = 0xFFFFFFFF;
		while(size >= 4)
		{
			v = *pInt++;
			crc = ( crc << 8 ) ^ crc32Table[0xFF & ( (crc >> 24) ^ (v >> 24) )];
			crc = ( crc << 8 ) ^ crc32Table[0xFF & ( (crc >> 24) ^ (v >> 16) )];
			crc = ( crc << 8 ) ^ crc32Table[0xFF & ( (crc >> 24) ^ (v >> 8) )];
			crc = ( crc << 8 ) ^ crc32Table[0xFF & ( (crc >> 24) ^ (v ) )];
			size -= 4;
		}

		uint8_t *pChar = (uint8_t*)pInt;
		while(size-- != 0)
		{
			v = *pChar++;
			crc = ( crc << 8 ) ^ crc32Table[0xFF & ( (crc >> 24) ^ (v >> 24) )];
			crc = ( crc << 8 ) ^ crc32Table[0xFF & ( (crc >> 24) ^ (v >> 16) )];
			crc = ( crc << 8 ) ^ crc32Table[0xFF & ( (crc >> 24) ^ (v >> 8) )];
			crc = ( crc << 8 ) ^ crc32Table[0xFF & ( (crc >> 24) ^ (v ) )];
		}
		return crc;
	}
};

Crc crc;

uint32_t calc_crc(char *buffer, int size)
{
	return crc.calc(buffer, size);
}
