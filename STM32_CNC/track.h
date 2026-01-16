#pragma once
// обработка кусков траектории

#include "fifo.h"

struct Track
{
	int segments;  //число отрезков в траектории
	int uLength;  //общая длина пути в микронах
};

//=========================================================================================
class TrackContainer
{
public:
	FIFOBuffer<Track, 5> tracks;

	void init()
	{
		tracks.Clear();
	}

	//=====================================================================================================
	//вызывается в потоке приёма
	void new_track()
	{
		if(tracks.IsFull())
			log_console("ERR: fracts overflow %d\n", 1);
		Track track;
		track.segments = 0;
		track.uLength = 0;
		tracks.Push(track);
	}

	//=====================================================================================================
	//вызывается в потоке приёма перед добавлением пакета
	void increment(int length)
	{
		if(tracks.IsEmpty())
		{
			Track track;
			track.segments = 0;
			track.uLength = 0;
			tracks.Push(track);
			log_console("no fract %d\n", 0);
		}
		Track *track = &tracks.Back();
		++track->segments;
		track->uLength += length;
	}

	//=====================================================================================================
	//вызывается в основном потоке
	//на момент извлечения отрезок уже есть в линии,
	//так что число отрезков в ней != 0
	bool decrement(int uLength)
	{
		bool isEmpty = false;
		Track *track = &tracks.Front();
		__disable_irq();
		while(track->segments == 0)
		{
			tracks.Pop();
			isEmpty = true;          //при завершении линии сбрасываем скорость
			track = &tracks.Front();
		}

		--track->segments;
		track->uLength -= uLength;
		if(track->uLength < 0) //вычитаем то, что перед этим было добавлено
			log_console("ERR: len %d, %d, %d\n", track->segments, track->uLength, uLength);
		__enable_irq();

		return isEmpty;
	}

	//=====================================================================================================
	int current_length()
	{
		return tracks.Front().uLength;
	}
};

TrackContainer tracks;

