// контроллер для платы stm32f407
#include "main.h"

int main()
{
	main_setup();
	
	while(1)
	{
		main_loop();
	}
}
