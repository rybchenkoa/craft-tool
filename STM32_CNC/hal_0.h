
//=====================================================================
//разводка выводов под ШИМ
//[0,1,   2,3    ], [4,5,   6,7  ], [8,9,     10,11  ], [12,13,  14,15 ] //номера сгруппированы по обмоткам, моторам
//[a0,a1, a2,a3  ], [a6,a7, b0,b1], [b10,b11, b14,b15], [b13,a8, a11,b8]
//[t2,t2, t15,t15], [t3,t3, t3,t3], [t2,t2,   t1,t1  ], [t1,t1,  t1,t16]
//[c1,c2, c1,c2  ], [c1,c2, c3,c4], [c3,c4,   c2n,c3n], [c1n,c1, c4,c1 ]

//это перестановка ножек для удобной разводки на плате
#define P0 1
#define P1 0
#define P2 2
#define P3 3

#define P4 5
#define P5 4
#define P6 6
#define P7 7

#define P8 10
#define P9 11
#define P10 9
#define P11 8

#define P12 14
#define P13 15
#define P14 13
#define P15 12

//включение/выключение управления ножкой порта с помощью периферии
#define USE_AF 0x8
void __forceinline set_port_af(GPIO_TypeDef *port, int pinnum, bool state)
{
	if (pinnum < 8)
	{
		if (state)
			port->CRL |= USE_AF * (1<<(pinnum*4)); //на каждый пин 4 бита
		else
			port->CRL &= ~(USE_AF * (1<<(pinnum*4)));
	}
	else
	{
		pinnum -= 8;
		if (state)
			port->CRH |= USE_AF * (1<<(pinnum*4));
		else
			port->CRH &= ~(USE_AF * (1<<(pinnum*4)));
	}
}

//подключение/отключение ножки к выходу ШИМ компаратора
template <int pin> __forceinline void enablePWM(bool enable) { }; //переключение сотояния AF или ручное руление пином

#define FCALL template <> __forceinline void
//a0
FCALL enablePWM<P0>(bool enable) { set_port_af(GPIOA, 0, enable); }
//a1
FCALL enablePWM<P1>(bool enable) { set_port_af(GPIOA, 1, enable); }

//a2
FCALL enablePWM<P2>(bool enable) { set_port_af(GPIOA, 2, enable); }
//a3
FCALL enablePWM<P3>(bool enable) { set_port_af(GPIOA, 3, enable); }

//a6
FCALL enablePWM<P4>(bool enable) { set_port_af(GPIOA, 6, enable); }
//a7
FCALL enablePWM<P5>(bool enable) { set_port_af(GPIOA, 7, enable); }
//b0
FCALL enablePWM<P6>(bool enable) { set_port_af(GPIOB, 0, enable); }
//b1
FCALL enablePWM<P7>(bool enable) { set_port_af(GPIOB, 1, enable); }

//b10
FCALL enablePWM<P8>(bool enable) { set_port_af(GPIOB, 10, enable); }
//b11
FCALL enablePWM<P9>(bool enable) { set_port_af(GPIOB, 11, enable); }
//b14
FCALL enablePWM<P10>(bool enable) { set_port_af(GPIOB, 14, enable); }
//b15
FCALL enablePWM<P11>(bool enable) { set_port_af(GPIOB, 15, enable); }

//b13
FCALL enablePWM<P12>(bool enable) { set_port_af(GPIOB, 13, enable); }
//a8
FCALL enablePWM<P13>(bool enable) { set_port_af(GPIOA, 8, enable); }
//a11
FCALL enablePWM<P14>(bool enable) { set_port_af(GPIOA, 11, enable); }
//b8
FCALL enablePWM<P15>(bool enable) { set_port_af(GPIOB, 8, enable); }


//--------------------------------------------------
//задаёт состояние пина порта
void __forceinline set_pin_state(GPIO_TypeDef *port, int pinnum, bool state)
{
	if (state)
		port->BSRR = 1 << pinnum;
	else
		port->BRR  = 1 << pinnum;
}

template <int pin> void set_pin_state(bool state) {}
	
//a0
template <> void set_pin_state<P0> (bool state) { set_pin_state(GPIOA, 0, state); }
//a1
template <> void set_pin_state<P1> (bool state) { set_pin_state(GPIOA, 1, state); }

//a2
template <> void set_pin_state<P2> (bool state) { set_pin_state(GPIOA, 2, state); }
//a3
template <> void set_pin_state<P3> (bool state) { set_pin_state(GPIOA, 3, state); }

//a6
template <> void set_pin_state<P4> (bool state) { set_pin_state(GPIOA, 6, state); }
//a7
template <> void set_pin_state<P5> (bool state) { set_pin_state(GPIOA, 7, state); }

//b0
template <> void set_pin_state<P6> (bool state) { set_pin_state(GPIOB, 0, state); }
//b1
template <> void set_pin_state<P7> (bool state) { set_pin_state(GPIOB, 1, state); }

//b10
template <> void set_pin_state<P8> (bool state) { set_pin_state(GPIOB, 10, state); }
//b11
template <> void set_pin_state<P9> (bool state) { set_pin_state(GPIOB, 11, state); }

//b14
template <> void set_pin_state<P10> (bool state) { set_pin_state(GPIOB, 14, state); }
//b15
template <> void set_pin_state<P11> (bool state) { set_pin_state(GPIOB, 15, state); }

//b13
template <> void set_pin_state<P12> (bool state) { set_pin_state(GPIOB, 13, state); }
//a8
template <> void set_pin_state<P13> (bool state) { set_pin_state(GPIOA, 8, state); }

//a11
template <> void set_pin_state<P14> (bool state) { set_pin_state(GPIOA, 11, state); }
//b8
template <> void set_pin_state<P15> (bool state) { set_pin_state(GPIOB, 8, state); }


//--------------------------------------------------
//задаёт заполненность с учётом инвертированности вывода (CC1NE например)
template <int pin> __forceinline void set_pulse_width(int len) {}
	
FCALL set_pulse_width<P0> (int len) {TIM2->CCR1 = len;}
FCALL set_pulse_width<P1> (int len) {TIM2->CCR2 = len;}

FCALL set_pulse_width<P2> (int len) {TIM15->CCR1 = len;}
FCALL set_pulse_width<P3> (int len) {TIM15->CCR2 = len;}

FCALL set_pulse_width<P4> (int len) {TIM3->CCR1 = len;}
FCALL set_pulse_width<P5> (int len) {TIM3->CCR2 = len;}

FCALL set_pulse_width<P6> (int len) {TIM3->CCR3 = len;}
FCALL set_pulse_width<P7> (int len) {TIM3->CCR4 = len;}

FCALL set_pulse_width<P8> (int len) {TIM2->CCR3 = len;}
FCALL set_pulse_width<P9> (int len) {TIM2->CCR4 = len;}

FCALL set_pulse_width<P10> (int len) {TIM1->CCR2 = /*PWM_SIZE -*/ len;}
FCALL set_pulse_width<P11> (int len) {TIM1->CCR3 = /*PWM_SIZE -*/ len;}

FCALL set_pulse_width<P12> (int len) {TIM1->CCR1 = PWM_SIZE - len;}
FCALL set_pulse_width<P13> (int len) {TIM1->CCR1 = len;}

FCALL set_pulse_width<P14> (int len) {TIM1->CCR4 = len;}
FCALL set_pulse_width<P15> (int len) {TIM16->CCR1 = len;}
