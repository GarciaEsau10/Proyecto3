#include <xc.h>
#include <stdio.h>
#include <string.h>

#define _XTAL_FREQ 8000000

// CONFIG
#pragma config FOSC = INTRC_NOCLKOUT  // Oscillator: Internal RC, no clock out
#pragma config WDTE = OFF             // Watchdog Timer: Disabled
#pragma config PWRTE = OFF            // Power-up Timer: Disabled
#pragma config MCLRE = OFF            // MCLR Pin: Digital input, MCLR internally disabled
#pragma config CP = OFF               // Code Protection: Disabled
#pragma config CPD = OFF              // Data Code Protection: Disabled
#pragma config BOREN = OFF            // Brown-out Reset: Disabled
#pragma config IESO = OFF             // Internal/External Switchover: Disabled
#pragma config FCMEN = OFF            // Fail-Safe Clock Monitor: Disabled
#pragma config LVP = OFF              // Low-Voltage Programming: Disabled
#pragma config BOR4V = BOR40V         // Brown-out Reset Voltage: 4.0V
#pragma config WRT = OFF              // Flash Program Memory Write: Disabled

// Pines LCD (4-bit)
#define RS   RD0
#define EN   RD1
#define D4   RD2
#define D5   RD3
#define D6   RD4
#define D7   RD5

// Pines HC-SR04
#define TRIG RC0
#define ECHO RC1

// LEDs en PORTA
#define LED_ROJO     RA0
#define LED_AMARILLO RA1
#define LED_VERDE    RA2

// Pines UART (ESP8266)
#define ESP_TX RC6
#define ESP_RX RC7

// ThingSpeak API Key
#define THINGSPEAK_API_KEY  "0VKGIDGQBDYSMZEAI"

// === Prototipos ===
void LCD_PulseEnable(void);
void LCD_Cmd(unsigned char cmd);
void LCD_Char(unsigned char data);
void LCD_Init(void);
void LCD_String(const char *str);
void LCD_Set_Cursor(char row, char col);
void LCD_Clear(void);

void send_trigger(void);
unsigned int read_echo(void);

void UART_Init(void);
void UART_WriteChar(char c);
void UART_WriteString(const char *s);
unsigned char UART_DataReady(void);
char UART_ReadChar(void);
void ESP_SendToThingSpeak(float distance);

// === Funciones LCD ===
void LCD_PulseEnable(void) {
    EN = 1; __delay_us(1); EN = 0;
}

void LCD_Cmd(unsigned char cmd) {
    RS = 0;
    D4 = (cmd >> 4) & 1; D5 = (cmd >> 5) & 1; D6 = (cmd >> 6) & 1; D7 = (cmd >> 7) & 1;
    LCD_PulseEnable(); __delay_us(200);
    D4 = cmd & 1; D5 = (cmd >> 1) & 1; D6 = (cmd >> 2) & 1; D7 = (cmd >> 3) & 1;
    LCD_PulseEnable(); __delay_ms(2);
}

void LCD_Char(unsigned char data) {
    RS = 1;
    D4 = (data >> 4) & 1; D5 = (data >> 5) & 1; D6 = (data >> 6) & 1; D7 = (data >> 7) & 1;
    LCD_PulseEnable();
    D4 = data & 1; D5 = (data >> 1) & 1; D6 = (data >> 2) & 1; D7 = (data >> 3) & 1;
    LCD_PulseEnable(); __delay_ms(2);
}

void LCD_Init(void) {
    TRISD = 0x00; RS = EN = 0;
    __delay_ms(20);
    LCD_Cmd(0x02); LCD_Cmd(0x28); LCD_Cmd(0x0C); LCD_Cmd(0x06); LCD_Cmd(0x01);
    __delay_ms(2);
}

void LCD_String(const char *str) { while (*str) LCD_Char(*str++); }
void LCD_Set_Cursor(char row, char col) { LCD_Cmd((row==1?0x80:0xC0)+(col-1)); }
void LCD_Clear(void) { LCD_Cmd(0x01); __delay_ms(2); }

// === Sensor ultrasónico ===
void send_trigger(void) { TRIG = 1; __delay_us(10); TRIG = 0; }
unsigned int read_echo(void) {
    unsigned int timeout=0;
    while (!ECHO && timeout++<30000);
    if(timeout>=30000) return 0;
    TMR1=0; T1CON=0x10; TMR1ON=1;
    while(ECHO && TMR1<30000);
    TMR1ON=0; return TMR1;
}

// === UART ===
void UART_Init(void) {
    BRGH=0; SPBRG=25; // 9600 bps @8MHz
    SYNC=0; SPEN=1; TXEN=1; CREN=1;
}
void UART_WriteChar(char c){ while(!TXIF); TXREG=c; }
void UART_WriteString(const char*s){ while(*s) UART_WriteChar(*s++); }
unsigned char UART_DataReady(void){ return RCIF; }
char UART_ReadChar(void){ if(OERR){CREN=0;CREN=1;} while(!UART_DataReady()); return RCREG; }

// === ThingSpeak ===
void ESP_SendToThingSpeak(float distance) {
    char num[10]; unsigned int len;
    UART_WriteString("AT+CWMODE=1\r\n"); __delay_ms(500);
    UART_WriteString("AT+CWJAP=\"TuSSID\",\"TuPASS\"\r\n"); __delay_ms(5000);
    UART_WriteString("AT+CIPSTART=\"TCP\",\"api.thingspeak.com\",80\r\n"); __delay_ms(2000);

    // Construir campo1
    sprintf(num, "%.2f", distance);

    // Preparar CIPSEND
    len = strlen("GET /update?api_key=") + strlen(THINGSPEAK_API_KEY)
          + strlen("&field1=") + strlen(num) + strlen("\r\n");
    sprintf(num, "%u", len);
    UART_WriteString("AT+CIPSEND="); UART_WriteString(num); UART_WriteString("\r\n"); __delay_ms(500);

    // Enviar GET
    UART_WriteString("GET /update?api_key=");
    UART_WriteString(THINGSPEAK_API_KEY);
    UART_WriteString("&field1="); UART_WriteString(num); UART_WriteString("\r\n");
    __delay_ms(1500);

    UART_WriteString("AT+CIPCLOSE\r\n"); __delay_ms(500);
}

// === MAIN ===
void main(void) {
    OSCCON=0x71; ADCON1=0x0F;
    TRISA=0b11111000; TRISD=0x00; TRISC=0b100010;
    PORTA=0;
    LCD_Init(); UART_Init();

    while(1) {
        send_trigger(); __delay_us(100);
        unsigned int t = read_echo();
        float d = t/58.0;
        LCD_Clear(); __delay_ms(50);
        LCD_Set_Cursor(1,1);
        if(!t) LCD_String("Sin respuesta");
        else { char b[16]; sprintf(b,"Dist:%.2fcm",d); LCD_String(b); }

        LED_ROJO=LED_AMARILLO=LED_VERDE=0; __delay_us(10);
        if(d<=10) LED_ROJO=1;
        else if(d<=30) LED_AMARILLO=1;
        else LED_VERDE=1;
        __delay_us(10);

        ESP_SendToThingSpeak(d);
        __delay_ms(15000);
    }
}
