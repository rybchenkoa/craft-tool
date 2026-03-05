#pragma once
// модуль связи по последовательному порту

// какой способ связи использовать
// чтоб не тратить процессорные такты, лишнее не включаем
// bluetooth занимает много места
#define CONNECTION_UART
//#define CONNECTION_BLUETOOTH

#ifdef CONNECTION_UART
#include <driver/uart.h>

void on_packet_received(char *packet, int size);

class Usart
{
public:
	static const int PACK_SIZE = 64;

	enum Tags
	{
		OP_CODE = '\\', //признак, что дальше идёт управляющий код
		OP_STOP = 'n',  //конец пакета
	};

	char pack[PACK_SIZE];
	int packPos = 0;
	bool waitOpcode = false;

	int uartId;

//----------------------------------------------------------
	void init()
	{
		uartId = UART_NUM_2;
		int rxPin = 16;
		int txPin = 17;
		
		// тестовая настройка для работы через встроенный на плате uart
		/*
		uartId = UART_NUM_0;
		rxPin = 3;
		txPin = 1;
		esp_log_level_set("*", ESP_LOG_NONE);
		/**/
		
		uart_config_t uart_config = {
			.baud_rate = 1000000,
			.data_bits = UART_DATA_8_BITS,
			.parity    = UART_PARITY_DISABLE,
			.stop_bits = UART_STOP_BITS_1,
			.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
			.source_clk = UART_SCLK_REF_TICK,
		};
		
		int receiveBufferSize = 200; // бодрейт / частота чтения = 3М/30к = 100, но минимум UART_HW_FIFO_LEN = 128
		int transmitBufferSize = 1000;
		uart_driver_delete(uartId);
		uart_driver_install(uartId, receiveBufferSize, transmitBufferSize, 0, NULL, 0);
		uart_param_config(uartId, &uart_config);
		uart_set_pin(uartId, txPin, rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
	}

//----------------------------------------------------------
	void send_packet(char *data, int size)
	{
		if (size > PACK_SIZE)
			return;
		
		char buffer[PACK_SIZE * 2];
		int count = 0;
		
		for(char *endp = data + size; data != endp; data++)
		{
			if(*data == OP_CODE)
				buffer[count++] = OP_CODE;
			
			buffer[count++] = *data;
		}
		buffer[count++] = OP_CODE;
		buffer[count++] = OP_STOP;
		
		size_t freeSpace = 0;
		uart_get_tx_buffer_free_size(uartId, &freeSpace);
		if(freeSpace < count + 1)
			return;
		
		uart_write_bytes(uartId, buffer, count);
	}

//----------------------------------------------------------
	void put_char(char data)
	{
		pack[packPos] = data;
		++packPos;
		if (packPos >= PACK_SIZE) // при переполнении молча очищаем
			packPos = 0;
	}

//----------------------------------------------------------
	void on_read_char(char data)
	{
		if (!waitOpcode) {
			if (data != OP_CODE) // обычные символы просто складываем в буфер
				put_char(data);
			else
				waitOpcode = true;
		}
		else
		{
			if (data != OP_STOP)
				put_char(data);
			else
			{
				on_packet_received(pack, packPos);
				packPos = 0;
			}
			waitOpcode = false;
		}
	}

//----------------------------------------------------------
	void process_receive()
	{
		char buf[32];
		while (true) {
			int size = uart_read_bytes(uartId, buf, 32, 0);
			for (int i = 0; i < size; ++i) {
				on_read_char(buf[i]);
			}
			if (size < 32)
				break;
		}
	}
};

Usart usart;
#endif

#ifdef CONNECTION_BLUETOOTH
#include "bluetooth.h"
#endif

//----------------------------------------------------------
void init_connection()
{
#ifdef CONNECTION_UART
	usart.init();
#endif
	
#ifdef CONNECTION_BLUETOOTH
	bluetooth.init();
#endif
}

//----------------------------------------------------------
void send_packet(char *packet, int size)
{
#ifdef CONNECTION_UART
	usart.send_packet(packet, size);
#endif
	
#ifdef CONNECTION_BLUETOOTH
	bluetooth.send_packet(packet, size);
#endif
}

//----------------------------------------------------------
void process_receive()
{
#ifdef CONNECTION_UART
	usart.process_receive();
#endif
	
#ifdef CONNECTION_BLUETOOTH
	bluetooth.process_receive();
#endif
}
