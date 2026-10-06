//Name: Thiven Anderson
//Email: thanderson@g.hmc.edu
//Date: 10/05/2026
//Description: Main program for lab 5 of E155 that enables the MCU to
// to determine the speed of a motor by reading from a quadrature encoder using interrupts.

int main(void) {
    //Enable encoders pins as inputs
    gpioEnable(GPIO_PORT_A);
    pinMode(ENCODERA_PIN, GPIO_INPUT);
    gpioEnable(GPIO_PORT_B);
    pinMode(ENCODERB_PIN, GPIO_INPUT);

    //Enable SYSCFG clock domain in RCC
    RCC->APB2ENR |= (1 << 0); // SYSCFGEN
    

}