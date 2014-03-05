#pragma once

template <class item, int bits> //какие элементы хранятся, сколько бит в счётчике (т.е. размер буфера = 2^bits)
class FIFOBuffer
{
	public:
	int first; //первый элемент в очереди
	int last;  //следующий за последним (первый свободный)
	static const int mask = (1 << bits) - 1;
	static const int size = 1 << bits;
	item buffer[size];


	int Size()     { return size - 1; }
	int Count()    { return (last - first) & mask; }
	bool IsFull()  { return (last + 1) & mask == first; } //остался один элемент - массив заполнен,
	bool IsEmpty() { return last == first; }              //совпадение указателей - массив пуст
	void Clear()   { last = first = 0; }
	void Push(item value) { buffer[last] = value; last = (last+1) & mask; }
	item Pop()     { int index = first; first = (first+1) & mask; return buffer[index]; }
	item Front()   { return buffer[first]; }

/*
	int Size()     { return size; }
	int Count()    { return last - first; }
	bool IsFull()  { return first + size == last; }
	bool IsEmpty() { return last == first; }
	void Clear()   { last = first = 0; }
	void Push(item value) { buffer[(last++) & mask] = value;}
	item Pop()     { return buffer[(first++) & mask]; }
	item Front()   { return buffer[first & mask]; }
*/
	FIFOBuffer()   { Clear(); }
};
