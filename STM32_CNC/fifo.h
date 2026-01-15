//ǁ
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

	int Size()     { return size; }
	int Count()    { return last - first; }
	bool IsFull()  { return first + size == last; }
	bool IsEmpty() { return last == first; }
	void Clear()   { last = first = 0; }
	void Push(item value) { buffer[last & mask] = value; ++last; }
	item Pop()     { return buffer[(first++) & mask]; }
	void Pop(int count) {first += count;}
	item& Front()  { return buffer[first & mask]; }
	item& Back()   { return buffer[(last - 1) & mask]; }
	int ContinuousCount() {return ((last & mask) >= (first & mask)) ? (last - first) : (size - (first & mask));}

	item& End()    { return buffer[last & mask];}
	void Push()    { ++last; }
	item& Element(int pos) { return buffer[pos & mask]; }

	FIFOBuffer()   { Clear(); }
};
