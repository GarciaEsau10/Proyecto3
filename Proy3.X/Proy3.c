#define _XTAL_FREQ 8000000
#include <xc.h>
#include <stdio.h>

// CONFIG
#pragma config FOSC = INTRC_NOCLKOUT
#pragma config WDTE = OFF, PWRTE = OFF, MCLRE = ON
#pragma config CP = OFF, CPD = OFF, BOREN = OFF, IESO = OFF, FCMEN = OFF, LVP = OFF
#pragma config BOR4V = BOR40V, WRT = OFF

// Pines LCD
#define RS RD0
#define EN RD1
#define D4 RD2
#define D5 RD3
#define D6 RD4
#define D7 RD5

// Pines HC-SR04
#define TRIG RC0
#define ECHO RC1

// === Funciones LCD ===
void LCD_PulseEnable() {
    EN = 1;
    __delay_us(1);
    EN = 0;
}

void LCD_Cmd(unsigned char cmd) {
    RS = 0;
    D4 = (cmd >> 4) & 1;
    D5 = (cmd >> 5) & 1;
    D6 = (cmd >> 6) & 1;
    D7 = (cmd >> 7) & 1;
    LCD_PulseEnable();
    __delay_us(200);
    D4 = cmd & 1;
    D5 = (cmd >> 1) & 1;
    D6 = (cmd >> 2) & 1;
    D7 = (cmd >> 3) & 1;
    LCD_PulseEnable();
    __delay_ms(2);
}

void LCD_Char(unsigned char data) {
    RS = 1;
    D4 = (data >> 4) & 1;
    D5 = (data >> 5) & 1;
    D6 = (data >> 6) & 1;
    D7 = (data >> 7) & 1;
    LCD_PulseEnable();
    D4 = data & 1;
    D5 = (data >> 1) & 1;
    D6 = (data >> 2) & 1;
    D7 = (data >> 3) & 1;
    LCD_PulseEnable();
    __delay_ms(2);
}

void LCD_Init() {
    TRISD = 0x00;
    RS = EN = 0;
    __delay_ms(20);
    LCD_Cmd(0x02); // 4-bit mode
    LCD_Cmd(0x28); // 2 lines, 5x7
    LCD_Cmd(0x0C); // Display ON, cursor OFF
    LCD_Cmd(0x06); // Entry mode
    LCD_Cmd(0x01); // Clear
    __delay_ms(2);
}

void LCD_String(const char *str) {
    while (*str) LCD_Char(*str++);
}

void LCD_Set_Cursor(char row, char col) {
    char pos = (row == 1) ? 0x80 + (col - 1) : 0xC0 + (col - 1);
    LCD_Cmd(pos);
}

void LCD_Clear() {
    LCD_Cmd(0x01);
    __delay_ms(2);
}

// === Funciones Sensor HC-SR04 ===

void send_trigger() {
    TRIG = 1;
    __delay_us(10);
    TRIG = 0;
}

unsigned int read_echo() {
    unsigned int timeout = 0;

    // Esperar que ECHO suba
    while (!ECHO && timeout++ < 30000);
    if (timeout >= 30000) return 0; // Timeout

    TMR1 = 0;
    T1CON = 0x10; // Prescaler 1:1, Fosc/4
    TMR1ON = 1;

    // Esperar que ECHO baje o se exceda tiempo
    while (ECHO && TMR1 < 30000);

    TMR1ON = 0;
    return TMR1;
}

// === MAIN ===

void main(void) {
    OSCCON = 0x71;      // Oscilador interno 8 MHz
    TRISC0 = 0;         // TRIG como salida
    TRISC1 = 1;         // ECHO como entrada

    LCD_Init();
    LCD_Clear();
    LCD_Set_Cursor(1, 1);
    LCD_String("Sensor Ultra");
    __delay_ms(1500);

    while (1) {
        send_trigger();
        __delay_us(100);
        unsigned int time = read_echo();
        float distance = time / 58.0;

        char buffer[16];
        LCD_Clear();
        LCD_Set_Cursor(1, 1);
        LCD_String("La distancia");

        LCD_Set_Cursor(2, 1);
        if (time == 0) {
            LCD_String("Sin respuesta");
        } else {
            sprintf(buffer, "es: %.2f cm", distance);
            LCD_String(buffer);
        }

        __delay_ms(500);
    }
}
