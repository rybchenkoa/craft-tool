:: flash loader demonstrator с сайта stm
:: загружает данные в мк и запускает его. можно сделать даже автопередёргивание reset и boot0
STMFlashLoader.exe -c --pn 3 --br 115200 --co OFF -i STM32_Low-density-value_16K -e --all -d --fn E:\my_programs\uVision\STM32_CNC\STM32_CNC.hex --v -p --dwp -v
PAUSE 