#include "stm32f10x.h"
#include "fifo.h"

uint32_t calc_crc(char *buffer, int size)
{
	volatile CRC_TypeDef *calc = CRC;
	calc->CR |= CRC_CR_RESET;
	
	uint32_t wordLength = size>>2;
	uint32_t *wordBuffer = (uint32_t*) buffer;
	
	while(wordLength--)
	{
		calc->DR = *(wordBuffer++);
		__NOP();__NOP();__NOP();__NOP();
	}
	
	switch(size & 3)
	{
		case 1: calc->DR = (*wordBuffer) & 0x000000FF; __NOP();__NOP();__NOP();__NOP(); break;
		case 2: calc->DR = (*wordBuffer) & 0x0000FFFF; __NOP();__NOP();__NOP();__NOP(); break;
		case 3: calc->DR = (*wordBuffer) & 0x00FFFFFF; __NOP();__NOP();__NOP();__NOP(); break;
	}
		
	return calc->DR;
}

class Usart
{
	public:
	FIFOBuffer<char, 6> receiveBuffer;  //64 байта на приём
	FIFOBuffer<char, 6> transmitBuffer; //64 на отправку

	enum Tags
	{
		OP_CODE = '\\', //признак, что дальше идёт управляющий код, если надо послать 100, надо послать его 2 раза
		OP_STOP = 'n',  //конец пакета
		OP_RUN  = 'r',  //начало пакета
	};
	
	enum States
	{
		S_READY,     //очередь пуста, никто ничего не присылал
		S_CODE,      //принят управляющий символ
		S_RUN,       //вначале принят символ '\' , ждём символ 'r'
		S_RECEIVING, //приём пакета
		S_END,       //принят символ конца, проверка пакета
	};
	
	char receiveState;
//----------------------------------------------------------
	void init()
	{
		RCC->APB2ENR |= RCC_APB2ENR_AFIOEN | RCC_APB2ENR_USART1EN; // включаем тактирование usart
		
		USART1->BRR = 208;//2500;//417;//94;// // 24 000 000/115200
		
		USART1->CR1 = USART_CR1_UE | USART_CR1_RE | USART_CR1_TE | 	// usart on, rx on, tx on, 
									USART_CR1_RXNEIE | USART_CR1_TCIE; 														//прерывание: байт принят
									
		RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;   	// подключаем к usart pin porta								

		GPIOA->CRH &= ~(GPIO_CRH_CNF9 | GPIO_CRH_MODE9); 						//a[9] - tx
		GPIOA->CRH |= GPIO_CRH_CNF9_1   														//выход, управляется периферией (push-pull)
								| GPIO_CRH_MODE9_0; 														//частота до 10 MHz

		GPIOA->CRH &= ~(GPIO_CRH_CNF10 | GPIO_CRH_MODE10); 					//a[10] - rx
		GPIOA->CRH |= GPIO_CRH_CNF10_0; 														//hiZ
								 //|~GPIO_CRH_MODE10_0; 												//вход

		__enable_irq(); 																						//разрешаем прерывания

		NVIC_EnableIRQ (USART1_IRQn); 															//разрешаем прерывание usart

	}

//----------------------------------------------------------
	void start_send()
	{
		if (!transmitBuffer.IsEmpty())
			USART1->DR = transmitBuffer.Pop();
	}

//----------------------------------------------------------
	void send_data(char *data, int size)
	{
		for(char *endp = data+size; data != endp; data++)
			transmitBuffer.Push(*data);
		start_send();
	}

//----------------------------------------------------------
	void process_receive_byte(char data)
	{
		switch (receiveState)
		{
			case S_READY:
				if (data == OP_CODE)
					receiveState = S_RUN;
				return;

			case S_RUN:
				if (data == OP_RUN)
					receiveState = S_RECEIVING;
				else
					receiveState = S_READY;
				return;

			case S_RECEIVING:
			{
				if (data == OP_CODE)
				{
					receiveState = S_CODE;
					return;
				}
				else
					if(!receiveBuffer.IsFull())
						receiveBuffer.Push(data);
				return;
			}
			
			case S_CODE:
			{
				switch (data)
				{
					case OP_CODE:                 //в пересылаемом пакете случайно был байт '\'
						receiveBuffer.Push(data);   //и мы его переслали таким образом
						receiveState = S_RECEIVING;
						return;
					case OP_STOP:
						receiveState = S_END;
						while(!receiveBuffer.IsEmpty())
							transmitBuffer.Push(receiveBuffer.Pop());
						start_send();
						receiveState = S_READY;
						return;
				}
				return;
			}
			
			case S_END:
				return;
				
			default:
				return;
		}
	}

//----------------------------------------------------------
	void process_send_byte()
	{
		if (!transmitBuffer.IsEmpty())
			USART1->DR = transmitBuffer.Pop();
	}
};

Usart usart;

//----------------------------------------------------------
extern "C" void USART1_IRQHandler(void)
{
	if (USART1->SR & USART_SR_RXNE) //байт принят
	{
		usart.process_receive_byte(USART1->DR);
		USART1->SR &= ~USART_SR_RXNE;
	}
	if (USART1->SR & USART_SR_TC) //байт послан
	{
		usart.process_send_byte();
		USART1->SR &= ~USART_SR_TC;
	}
}
