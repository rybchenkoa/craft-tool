#pragma once
// расчёт контрольной суммы

uint32_t calc_crc(char *buffer, int size);

struct Crc
{
	void init()
	{
	}

	uint32_t calc(char *buffer, int size)
	{
		return calc_crc(buffer, size);
	}
};

Crc crc;

uint32_t calc_crc(char *buffer, int size)
{
	//__disable_irq();
	volatile CRC_TypeDef *calc = CRC;
	calc->CR = CRC_CR_RESET;
	__NOP();//__NOP();__NOP();
	uint32_t *wordBuffer = (uint32_t*) buffer;
	
	while(size >= 4)
	{
		calc->DR = *(wordBuffer++);
		size -= 4;
	}
	
	uint8_t *buf = (uint8_t*)wordBuffer;
	while(size-- > 0)
	{
		calc->DR = *(buf++);
	}

	uint32_t r = calc->DR;
	//__enable_irq();
	return r;
}
