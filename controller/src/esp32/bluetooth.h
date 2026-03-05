#pragma once
// модуль связи по последовательному порту через блютуз

#include <BluetoothSerial.h>

void on_packet_received(char *packet, int size);


class Bluetooth
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
	bool waitOpcode;

	BluetoothSerial bt;
	FIFOBuffer<char, 9> receiveBuffer; // 512 байт на приём // бодрейт / частота чтения = 3М/30к = 100

	//----------------------------------------------------------
	void init()
	{
		waitOpcode = false;
		
		bt.onData([this](const uint8_t *buffer, size_t size) {
			on_receive(buffer, size);
		});
		
		bt.setPin("9652");
		bt.begin("ESP32 CNC");
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
		
		/*size_t freeSpace = 0;
		uart_get_tx_buffer_free_size(uartId, &freeSpace);
		if(freeSpace < count + 1)
			return;
		*/
		bt.write((uint8_t*)buffer, count);
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
		int size = receiveBuffer.ContinuousCount();
		char* data = &receiveBuffer.Front();
		for(char *end = data + size; data != end; ++data)
			on_read_char(*data);
		receiveBuffer.Pop(size);
	}

	//----------------------------------------------------------
	// первичный приём происходит в прерывании
	void on_receive(const uint8_t *data, size_t size)
	{
		for(const uint8_t *end = data + size; data != end; ++data)
			receiveBuffer.Push(*data);
	}
};

Bluetooth bluetooth;
