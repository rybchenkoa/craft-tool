#include "stm32f10x.h"
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
	#define USART_NORM (USART_CR1_UE | USART_CR1_RE | USART_CR1_TE | USART_CR1_RXNEIE)
	#define USART_SEND (USART_CR1_UE | USART_CR1_RE | USART_CR1_TE | USART_CR1_RXNEIE | USART_CR1_TXEIE)
//----------------------------------------------------------
	void init()
	{
		RCC->APB2ENR |= RCC_APB2ENR_AFIOEN | RCC_APB2ENR_USART1EN; // включаем тактирование usart
		
		USART1->BRR = 96;//48;//*/208;//2500;//417;//94;// // 24 000 000/115200
		
		USART1->CR1 = USART_CR1_UE | USART_CR1_RE | USART_CR1_TE | 	// usart on, rx on, tx on, 
									USART_CR1_RXNEIE; 														//прерывание: байт принят

		RCC->APB2ENR |= RCC_APB2ENR_IOPAEN;   	// подключаем к usart pin porta								

		GPIOA->CRH &= ~(GPIO_CRH_CNF9 | GPIO_CRH_MODE9); 						//a[9] - tx
		GPIOA->CRH |= GPIO_CRH_CNF9_1   														//выход, управляется периферией (push-pull)
								| GPIO_CRH_MODE9_0; 														//частота до 10 MHz

		GPIOA->CRH &= ~(GPIO_CRH_CNF10 | GPIO_CRH_MODE10); 					//a[10] - rx
		GPIOA->CRH |= GPIO_CRH_CNF10_0; 														//hiZ
								 //|~GPIO_CRH_MODE10_0; 												//вход

		__enable_irq(); 																						//разрешаем прерывания

		NVIC_EnableIRQ (USART1_IRQn); 															//разрешаем прерывание usart
		
		// init DMA for transmit
		USART1->CR3 |= USART_CR3_DMAT;
		lastSendSize = 0;
		
		RCC->AHBENR |= RCC_AHBENR_DMA1EN;
		DMA1_Channel4->CCR &= ~DMA_CCR4_MEM2MEM;   // не из памяти в память
		DMA1_Channel4->CCR &= ~DMA_CCR4_MSIZE;   // размер данных 8 бит
		DMA1_Channel4->CCR &= ~DMA_CCR4_PSIZE;   // размер периферии 8 бит
		DMA1_Channel4->CCR |= DMA_CCR4_MINC;   // память инкрементировать
		DMA1_Channel4->CCR &= ~DMA_CCR4_PINC;   // периферию не инкрементировать
		DMA1_Channel4->CCR &= ~DMA_CCR4_CIRC;   // циркулярный режим выключен
		DMA1_Channel4->CCR |= DMA_CCR4_DIR;      // направление от памяти в периферию
		DMA1_Channel4->CCR |= DMA_CCR4_TCIE | DMA_CCR4_TEIE; //прерывание на полную посылку
		DMA1_Channel4->CPAR = (uint32_t)&(USART1->DR);           // адрес перифериии

		NVIC_EnableIRQ(DMA1_Channel4_IRQn);
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
		if((DMA1_Channel4->CCR & DMA_CCR4_EN) == 0)
		{
			process_send_data();
		}
__enable_irq();
	}

//----------------------------------------------------------
	void process_send_data()
	{
		transmitBuffer.Pop(lastSendSize);
		if (!transmitBuffer.IsEmpty())
		{
			DMA1_Channel4->CCR &= ~DMA_CCR4_EN;
			lastSendSize = transmitBuffer.ContinuousCount();
			DMA1_Channel4->CMAR = (uint32_t)&transmitBuffer.Front();   // адрес памяти
			DMA1_Channel4->CNDTR = lastSendSize;   // длина пересылки
			DMA1_Channel4->CCR |= DMA_CCR4_EN;
		}
		else
		{
			lastSendSize = 0;
			DMA1_Channel4->CCR &= ~DMA_CCR4_EN;
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
extern "C" void DMA1_Channel4_IRQHandler (void)
{//led.flip();
	//обмен завершен
	//if(DMA1->ISR & DMA_ISR_TCIF4)
	//{
		DMA1->IFCR |= DMA_IFCR_CTCIF4 /*| DMA_IFCR_CHTIF4*/ | DMA_IFCR_CTEIF4;
		usart.process_send_data();
	//}
	//else
	//	DMA1->IFCR |= DMA_IFCR_CTCIF4 | DMA_IFCR_CHTIF4 | DMA_IFCR_CTEIF4;
	//DMA1->IFCR |= DMA_IFCR_CGIF4;
}

//----------------------------------------------------------
void send_packet(char *packet, int size)
{
	usart.send_packet(packet, size);
}
