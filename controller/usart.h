//ǁ
#include "stm32f4xx.h"
#include "stm32f4xx_ll_gpio.h"
#include "stm32f4xx_hal_gpio_ex.h"
#include "fifo.h"
#include "led.h"

void on_packet_received(char *packet, int size);

const int RECEIVE_SIZE = 512;
const int PACK_SIZE = 64;

class Usart
{
	public:
	char receiveBuffer[RECEIVE_SIZE];  //512 байт на приём
	int receivePos = 0;

	char pack[PACK_SIZE];
	int packPos = 0;

	FIFOBuffer<char, 9> transmitBuffer; //512 на отправку
	uint32_t lastSendSize;

	enum Tags
	{
		OP_CODE = '\\', //признак, что дальше идёт управляющий код
		OP_STOP = 'n',  //конец пакета
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

		USART1->BRR = 84000000 / 1000000;// 115200 230400 500000
		
		USART1->CR1 = USART_CR1_UE | USART_CR1_RE | USART_CR1_TE; 	// usart on, rx on, tx on,

		__enable_irq();               //разрешаем прерывания
		
		// DMA для TX
		USART1->CR3 |= USART_CR3_DMAT;
		lastSendSize = 0;
		
		DMA_Stream_TypeDef *stream = DMA2_Stream7;
		stream->CR = DMA_MEMORY_TO_PERIPH   // направление от памяти в периферию
		            | DMA_MDATAALIGN_BYTE   // размер данных 8 бит
		            | DMA_PDATAALIGN_BYTE   // размер периферии 8 бит
		            | DMA_SxCR_MINC   // память инкрементировать
		            | DMA_SxCR_TCIE | DMA_SxCR_TEIE //прерывание на полную посылку
                    | DMA_CHANNEL_4;
		stream->PAR = (uint32_t)&USART1->DR;           // адрес перифериии

		NVIC_EnableIRQ(DMA2_Stream7_IRQn);

		// DMA для RX
		USART1->CR3 |= USART_CR3_DMAR;
		receivePos = 0;
		packPos = 0;
		stream = DMA2_Stream2;
		stream->CR = DMA_PERIPH_TO_MEMORY   // читаем из порта в кольцевой буфер
		            | DMA_MDATAALIGN_BYTE   // размер данных 8 бит
		            | DMA_PDATAALIGN_BYTE   // размер периферии 8 бит
		            | DMA_SxCR_MINC   // память инкрементировать
		            | DMA_SxCR_CIRC   // циклично
                    | DMA_CHANNEL_4;
		stream->NDTR = RECEIVE_SIZE;
		stream->M0AR = (int)receiveBuffer;
		stream->PAR = (int)&USART1->DR;
		stream->CR |= DMA_SxCR_EN;
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
		if(transmitBuffer.Count() + size + 3 > transmitBuffer.Size())
		{
			start_send();
			return;
		}

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
	void put_char(char data)
	{
		pack[packPos] = data;
		++packPos;
		if (packPos >= PACK_SIZE) //при переполнении молча очищаем
			packPos = 0;
	}
	void process_receive()
	{
		int endPos = RECEIVE_SIZE - DMA2_Stream2->NDTR; //надо читать до этой позиции
		int boundPos = endPos;
		if (boundPos < receivePos) //если будет переход на начало буфера, то сначала дочитаем до конца
			boundPos = RECEIVE_SIZE;
		while (receivePos < boundPos)
		{
			if (receiveBuffer[receivePos] != OP_CODE) //обычные символы просто складываем в буфер
				put_char(receiveBuffer[receivePos++]);
			else
			{
				int incPos = receivePos + 1; //смотрим, где лежит следующий символ
				if (incPos == boundPos)
				{
					if (endPos < incPos) //либо в начале буфера
					{
						incPos = 0;
						boundPos = endPos;
					}
					else
						return; //либо ещё не пришёл
				}
				receivePos = incPos;
				if (receiveBuffer[receivePos] != OP_STOP)
					put_char(receiveBuffer[receivePos++]);
				else
				{
					on_packet_received(pack, packPos);
					packPos = 0;
					++receivePos;
				}

			}
		}
		if (receivePos == RECEIVE_SIZE) //следующий кусок прочитаем в следующий раз
			receivePos = 0;
	}

//----------------------------------------------------------
	void start_send()
	{
		
__disable_irq();
		if((DMA2_Stream7->CR & DMA_SxCR_EN) == 0)
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
			DMA2_Stream7->CR &= ~DMA_SxCR_EN;
			lastSendSize = transmitBuffer.ContinuousCount();
			if (lastSendSize > 64) //если вдруг попадем на посылку от начала буфера до конца
				lastSendSize = 64; // то чтобы не блокировать добавление элементов в начало, шлем по кускам
			DMA2_Stream7->M0AR = (uint32_t)&transmitBuffer.Front();   // адрес памяти
			DMA2_Stream7->NDTR = lastSendSize;   // длина пересылки
			DMA2_Stream7->CR |= DMA_SxCR_EN;
		}
		else
		{
			lastSendSize = 0;
			DMA2_Stream7->CR &= ~DMA_SxCR_EN;
		}
	}
};

Usart usart;

//----------------------------------------------------------
extern "C" void DMA2_Stream7_IRQHandler (void)
{
	DMA2->HIFCR |= DMA_HIFCR_CTCIF7 | DMA_HIFCR_CTEIF7;
	usart.process_send_data();
}

//----------------------------------------------------------
void send_packet(char *packet, int size)
{
	usart.send_packet(packet, size);
}
