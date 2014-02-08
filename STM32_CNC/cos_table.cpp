#include "common.h"

//constexpr нету
//static_assert нету
//static const double нету

//вычисление косинуса с точностью float на препроцессоре

#define cos_t(x) (((1-x*x/2.0 \
                  *(1.0-x*x/3.0/4 \
                  *(1.0-x*x/5.0/6 \
                  *(1.0-x*x/7.0/8 \
                  *(1.0-x*x/9.0/10 \
                  *(1.0-x*x/11.0/12 \
                  *(1.0-x*x/13.0/14 \
                  *(1.0-x*x/15.0/16 \
                  *(1.0-x*x/17.0/18 \
                  *(1.0-x*x/19.0/20 \
                  *(1.0-x*x/21.0/22 \
                  *(1.0-x*x/23.0/24 \
                  *(1.0-x*x/25.0/26 \
                 )))))))))))))))

#define cos_i(x) ((int8_t)(COS_AMPLITUDE * cos_t(((2*M_PI*(x))/COS_TABLE_COUNT))))

//смысла в 10 битах нет, это просто для теста скорости
#define A10(x) A9(x) A9(x + 2048)
#define A9(x) A8(x) A8(x + 1024)
#define A8(x) A7(x) A7(x + 512)
#define A7(x) A6(x) A6(x + 256)
#define A6(x) A5(x) A5(x + 128)
#define A5(x) A4(x) A4(x +  64)
#define A4(x) A3(x) A3(x +  32)
#define A3(x) A2(x) A2(x +  16)
#define A2(x) A1(x) A1(x +   8)
#define A1(x) A0(x) A0(x +   4)
#define A0(x) F(x)   F(x +   2)
#define F(x)  I(x)   I(x +   1)

#define I(x) cos_i(x),

//это шаманство для автоматической подстановки нужного количества элементов массива
#define init(a,b) conc(a, b)
#define conc(A, n) A##n

int8_t cosTable[COS_TABLE_COUNT] = {init(A, DIV_BITS_COUNT)(0)};
