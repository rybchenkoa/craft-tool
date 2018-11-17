//ǁ
#include "stm32f4xx.h"
#include "fifo.h"
#include "led.h"

void on_packet_received(char *packet, int size);

class Usart
{
	public:
	FIFOBuffer<char, 6> receiveBuffer;  //64 байта на приём
	FIFOBuffer<char, 9> transmitBuffer; //512 на отправку
	uint32_t lastSendSize;

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
        LL_GPIO_InitTypeDef gpio;
        gpio.Pin = LL_GPIO_PIN_9|LL_GPIO_PIN_10;
        gpio.Mode = LL_GPIO_MODE_ALTERNATE;
        gpio.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH;
        gpio.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
        gpio.Pull = LL_GPIO_PULL_UP;
        gpio.Alternate = GPIO_AF7_USART1;
        LL_GPIO_Init(GPIOA, &gpio);

		USART1->BRR = 84000000 / 230400;// 115200 230400 500000
		
		USART1->CR1 = USART_CR1_UE | USART_CR1_RE | USART_CR1_TE | 	// usart on, rx on, tx on, 
									USART_CR1_RXNEIE; 				//прерывание: байт принят

		__enable_irq();               //разрешаем прерывания
		NVIC_EnableIRQ (USART1_IRQn); //разрешаем прерывание usart
		
		// DMA для TX
		USART1->CR3 |= USART_CR3_DMAT;
		lastSendSize = 0;
		
		DMA_Stream_TypeDef *stream = DMA2_Stream7;
		stream->CR = DMA_MEMORY_TO_PERIPH   // направление от памяти в периферию
		            | DMA_MDATAALIGN_BYTE   // размер данных 8 бит
		            | DMA_PDATAALIGN_BYTE   // размер периферии 8 бит
		            | DMA_SxCR_MINC   // память инкрементировать
		            | DMA_SxCR_DIR      // направление от памяти в периферию
		            | DMA_SxCR_TCIE | DMA_SxCR_TEIE //прерывание на полную посылку
                    | DMA_CHANNEL_4;
		stream->PAR = (uint32_t)&USART1->DR;           // адрес перифериии

		NVIC_EnableIRQ(DMA2_Stream4_IRQn);
	}

//----------------------------------------------------------
	void send_data(char *data, int size)
	{
		for(char *endp = data+size; data != endp; data++)
			transmitBuffer.Push(*data);
		start_send();
	}

//----------------------------------------------------------
	void send_packet(char *data, int size)
	{
		if(transmitBuffer.Count() + size + 5 > transmitBuffer.Size())
		{
			start_send();
			return;
		}
			
		transmitBuffer.Push(OP_CODE);
		transmitBuffer.Push(OP_RUN);
		for(char *endp = data+size; data != endp; data++)
		{
			if(*data == OP_CODE)
			{
				transmitBuffer.Push(OP_CODE);
				if(transmitBuffer.Count() + (endp - data) + 3 > transmitBuffer.Size())
				{
					start_send();
					return;
				}
			}
			transmitBuffer.Push(*data);
		}
		transmitBuffer.Push(OP_CODE);
		transmitBuffer.Push(OP_STOP);
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
				{
					receiveState = S_RECEIVING;
					receiveBuffer.Clear();
				}
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
						if(!receiveBuffer.IsFull())
							receiveBuffer.Push(data);   //и мы его переслали таким образом
						receiveState = S_RECEIVING;
						return;
					case OP_STOP:
						receiveState = S_END;
						on_packet_received(receiveBuffer.buffer, receiveBuffer.Count());
						receiveBuffer.Clear();
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
	void start_send()
	{
		
__disable_irq();
		if((DMA2_Stream4->CR & DMA_SxCR_EN) == 0)
		{
			process_send_data();
		}
__enable_irq();
	}

//----------------------------------------------------------
	void process_send_data()
	{
		transmitBuffer.Pop(lastSendSize);
		if (transmitBuffer.Count()>0) //возможно, из-за многопоточности != здесь не работает
		{
			DMA2_Stream4->CR &= ~DMA_SxCR_EN;
			lastSendSize = transmitBuffer.ContinuousCount();
			if (lastSendSize > 64) //если вдруг попадем на посылку от начала буфера до конца
				lastSendSize = 64; // то чтобы не блокировать добавление элементов в начало, шлем по кускам
			DMA2_Stream4->M0AR = (uint32_t)&transmitBuffer.Front();   // адрес памяти
			DMA2_Stream4->NDTR = lastSendSize;   // длина пересылки
			DMA2_Stream4->CR |= DMA_SxCR_EN;
		}
		else
		{
			lastSendSize = 0;
			DMA2_Stream4->CR &= ~DMA_SxCR_EN;
		}
	}
};

Usart usart;

//----------------------------------------------------------
extern "C" void USART1_IRQHandler(void)
{
	if (USART1->SR & USART_SR_RXNE) //байт принят
	{
		usart.process_receive_byte(USART1->DR);
	}
}

//----------------------------------------------------------
extern "C" void DMA2_Stream4_IRQHandler (void)
{//led.flip();
	//обмен завершен
	//if(DMA2->ISR & DMA_ISR_TCIF4)
	//{
		DMA2->HIFCR |= DMA_HIFCR_CTCIF4 /*| DMA_HIFCR_CHTIF4*/ | DMA_HIFCR_CTEIF4;
		usart.process_send_data();
	//}
	//else
	//	DMA2->IFCR |= DMA_HIFCR_CTCIF4 | DMA_HIFCR_CHTIF4 | DMA_HIFCR_CTEIF4;
	//DMA2->HIFCR |= DMA_HIFCR_CGIF4;
}

//----------------------------------------------------------
void send_packet(char *packet, int size)
{
	usart.send_packet(packet, size);
}
