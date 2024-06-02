/**
 * Proyecto Final de Arquitectura de computadoras
 * Grupo: 03
 * Integrantes:
 * - 22200193 - Cartagena Valera Brush, Davis Leonardo
 * - 22200134 - Rivera Llancari Aldair Alejandro
 * - 22200182 - Sernaque Cobeñas José Manuel
 * - 22200031 - Said William Najarro Llacza
 * - 22200113 - Huamani Quispe José Fernando
 * - 22200199 - Jhair Roussell Melendez Blas
 */

// Inclusión de librerías
#include "xc.h"
#include <libpic30.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Configuración del microcontrolador
#pragma config PWMPIN = ON    // Los pines PWM están habilitados (modo de salida digital)
#pragma config HPOL = ON      // Los pines PWM tienen polaridad alta
#pragma config LPOL = ON      // Los pines PWM tienen polaridad baja
#pragma config ALTI2C = OFF   // Los pines SDA1 y SCL1 son pines I/O (ya no exclusivos de I2C)
#pragma config FPWRT = PWR128 // El tiempo de espera del encendido es de 128 ms

// Watchdog: Temporizador que reinicia el microcontrolador si no recibe una señal de reinicio
#pragma config WDTPOST = PS32768 // El postescalador del temporizador de vigilancia es de 1:32,768 (Watchdog)
#pragma config WDTPRE = PR128    // El preescalador del temporizador de vigilancia es de 1:128 (Watchdog)
#pragma config WINDIS = OFF      // El temporizador de vigilancia está en modo de ventana
#pragma config FWDTEN = OFF      // El temporizador de vigilancia está deshabilitado

#pragma config POSCMD = HS    // Seleccionar el modo de oscilador primario (HS Crystal Oscillator Mode)
#pragma config OSCIOFNC = OFF // El pin OSC2 es una salida de reloj
#pragma config IOL1WAY = ON   // La configuración de selección de periféricos es de una sola vía
#pragma config FCKSM = CSDCMD // El cambio de reloj está habilitado, el monitor de reloj seguro está deshabilitado

#pragma config FNOSC = PRIPLL // Seleccionar la fuente de reloj (Primary Oscillator with PLL module)
#pragma config IESO = OFF     // El arranque del oscilador de dos velocidades está deshabilitado

// Definición de tipos de datos
typedef struct {
    int red;
    int green;
    int blue;
} Color; // Estructura para los colores

// Definición de constantes

const uint8_t MAX_SPEED_CLOCK_MHZ = 40;                  // Velocidad máxima del reloj en MHz
const uint8_t CRYSTAL_VALUE_MHZ = 20;                    // Valor del cristal en MHz
const int32_t FREQ = 40 * 1000000;                       // Frecuencia del reloj
const int32_t BAUDRATE = 9600;                           // Velocidad de transmisión de datos
const int32_t BRGVAL = ((40 * 1000000 / 9600) / 16) - 1; // Valor del registro de baudios
const int32_t DATA_SIZE = 5;                             // Tamaño del buffer de datos para la mediana (limpieza)
const Color P_BLUE = {0, 0, 255};                        // Color azul
const Color P_GREEN = {0, 255, 0};                       // Color verde
const Color P_RED = {255, 0, 0};                         // Color rojo

int32_t data_buffer[5];        // Buffer de datos para la mediana (limpieza)
int32_t data_buffer_index = 0; // Índice del buffer de datos para la mediana (limpieza)
int32_t min_read = 0;          // Valor mínimo de lectura del ADC
int32_t max_read = 1024;       // Valor máximo de lectura del ADC

// Declaración de funciones
void delay_ms(int32_t x);                                              // Función para generar un retardo en milisegundos
void clock_pll(void);                                                  // Función para configurar el reloj
void setup(void);                                                      // Función para configurar el microcontrolador
void pin_config(void);                                                 // Función para configurar los pines
void adc_config(void);                                                 // Función para configurar el ADC
void adc_start(void);                                                  // Función para iniciar el ADC
void uart_start(void);                                                 // Función para iniciar la comunicación serial
void loop(void);                                                       // Función para el bucle principal
void adc_prepare(void);                                                // Función para preparar el ADC
int32_t adc_read(void);                                                // Función para leer el valor del ADC
int32_t process_data(int32_t value);                                   // Función para procesar los datos
void prepare_data_buffer(void);                                        // Función para preparar el buffer de datos para la mediana (limpieza)
int32_t median_filter(void);                                           // Función para aplicar el filtro de mediana
void swap(int32_t *a, int32_t *b);                                     // Función para intercambiar dos valores
int32_t partition(int32_t arr[], int32_t low, int32_t high);           // Función para dividir el array y devolver el índice del pivote
void quick_sort(int32_t arr[], int32_t low, int32_t high);             // Función para ordenar el array
int8_t percentage(int32_t value);                                      // Función para calcular el porcentaje
void send_percentage(int8_t percentage);                               // Función para enviar el porcentaje
void send_string(char *string);                                        // Función para enviar una cadena
void send_char(char character);                                        // Función para enviar un caracter
void __attribute__((__interrupt__, no_auto_psv)) _U1RXInterrupt(void); // Función para la interrupción de recepción
void enable_light_show(void);                                          // Función para habilitar el espectáculo de luces
void disable_light_show(void);                                         // Función para deshabilitar el espectáculo de luces
void execute_plant_watering(void);                                     // Función para ejecutar el riego de la planta
void init_pwm(void);                                                   // Función para iniciar el PWM
void set_colors(Color color);                                          // Función para establecer los colores del LED RGB
void disable_plant_watering(void);                                     // Función para deshabilitar el riego de la planta

// Función principal
int main(void) {
    setup();
    loop();
}

// Definición de funciones

void delay_ms(int32_t x) {
    __delay32((FREQ / 1000) * x); // Retardo en milisegundos
}

void setup(void) {
    clock_pll();           // Configurar el reloj
    pin_config();          // Configurar los pines
    adc_config();          // Configurar el ADC
    adc_start();           // Iniciar el ADC
    uart_start();          // Iniciar la comunicación serial
    init_pwm();            // Iniciar el PWM
    prepare_data_buffer(); // Preparar el buffer de datos para la mediana (limpieza)
    set_colors(P_GREEN);
}

void clock_pll(void) {
    int M = MAX_SPEED_CLOCK_MHZ * 8 / CRYSTAL_VALUE_MHZ; // M = Fcy/Fosc
    PLLFBD = M - 2;                                      // M = PLLFBD + 2
    CLKDIVbits.PLLPRE = 0;                               // N1 = 2
    CLKDIVbits.PLLPOST = 0;                              // N2 = 2
    while (_LOCK == 0) {                                 // Esperar a que el PLL se bloquee
    }
}

void pin_config(void) {
    AD1PCFGL = 0xffff;    // Configurar todos los pines A como salidas digitales
    AD1CHS0 = 0x0005;     // AN5 como entrada
    RPINR18 = 0x0000;     // RP0 como U1RX
    RPOR0 = 0x0300;       // RP1 como U1TX
    TRISAbits.TRISA1 = 0; // Configurar RA1 como salida
    TRISAbits.TRISA4 = 0; // Configurar RA4 como salida
}

void adc_config(void) {
    AD1CON1 = 0x00E4;  // Formato de salida: Entero, 12 bits, auto-convertir
    AD1CON2 = 0x003C;  // Interrupción después de 16 conversiones
    AD1CON3 = 0x0353;  // Tad = 3 * Tcy
    AD1PCFGL = 0xFFDF; // AN5 como entrada analógica
}

void adc_start(void) {
    AD1CON1bits.ADON = 1; // Prende el ADC
}

void uart_start(void) {
    U1BRG = BRGVAL;        // Configurar el valor del registro de baudios
    U1MODEbits.UARTEN = 1; // Prender el módulo UART
    U1STAbits.UTXEN = 1;   // Prender el transmisor
    IFS0bits.U1RXIF = 0;   // Limpiar la bandera de interrupción de recepción
    IEC0bits.U1RXIE = 1;   // Habilitar la interrupción de recepción
}

void init_pwm(void) {
    PTCONbits.PTEN = 0b00;   // Deshabilitar el módulo PWM durante la configuración
    PTCONbits.PTCKPS = 0b00; // Prescaler 1:1
    PTCONbits.PTOPS = 0b00;  // Postscaler 1:1
    PTPER = 999;             // Periodo del PWM
    PWM1CON2bits.IUE = 1;    // Actualizar el valor del periodo del PWM
    PWM1CON1bits.PMOD1 = 0;  // Modo de operación del PWM (no inversor)
    PWM1CON1bits.PMOD2 = 0;  // Modo de operación del PWM (no inversor)
    PWM1CON1bits.PMOD3 = 0;  // Modo de operación del PWM (no inversor)

    PWM1CON1bits.PEN1H = 1; // Habilitar el PWM1H1
    PWM1CON1bits.PEN2H = 1; // Habilitar el PWM1H2
    PWM1CON1bits.PEN3H = 1; // Habilitar el PWM1H3

    PTCONbits.PTEN = 1; // Habilitar el módulo PWM
    P1DC1 = 0;          // 1000 50% duty
    P1DC2 = 0;          // 1000 50% duty
    P1DC3 = 0;          // 1000 50% duty
}

void prepare_data_buffer(void) {
    for (int32_t i = 0; i < DATA_SIZE; i++) {
        adc_prepare();
        int32_t humidity_raw_value = adc_read();
        data_buffer[i] = humidity_raw_value;
    }
}

void loop(void) {
    while (1) {
        adc_prepare();
        int32_t humidity_raw_value = adc_read();                   // Leer el valor del ADC
        int32_t humidity_value = process_data(humidity_raw_value); // Procesar el valor
        int8_t humidity_percentage = percentage(humidity_value);   // Calcular el porcentaje
        send_percentage(humidity_percentage);                      // Enviar el porcentaje
        delay_ms(3000);                                            // Esperar 1 segundo
    }
}

void adc_prepare(void) {
    while (!IFS0bits.AD1IF) { // Esperar a que el ADC esté listo
    }
    IFS0bits.AD1IF = 0; // Limpiar la bandera de interrupción del ADC
}

int32_t adc_read(void) {
    int32_t value = ADC1BUF0; // Leer el valor del buffer del ADC
    return value;
}

int32_t process_data(int32_t value) {
    data_buffer[data_buffer_index] = value;                  // Guardar el valor en el buffer de datos
    data_buffer_index = (data_buffer_index + 1) % DATA_SIZE; // Actualizar el índice del buffer de datos
    return median_filter();                                  // Aplicar el filtro de mediana
}

int32_t median_filter(void) {
    quick_sort(data_buffer, 0, DATA_SIZE - 1); // Ordenar el buffer de datos
    if (DATA_SIZE % 2 == 0) {
        return (data_buffer[DATA_SIZE / 2] + data_buffer[DATA_SIZE / 2 - 1]) / 2; // Devolver la mediana
    } else {
        return data_buffer[DATA_SIZE / 2]; // Devolver la mediana
    }
}

void swap(int32_t *a, int32_t *b) { // Función para intercambiar dos valores
    int t = *a;
    *a = *b;
    *b = t;
}

int32_t partition(int32_t array[], int32_t low, int32_t high) { // Función para dividir el array y devolver el índice del pivote
    int pivot = array[high];
    int i = (low - 1);

    for (int j = low; j <= high - 1; j++) {
        if (array[j] < pivot) {
            i++;
            swap(&array[i], &array[j]);
        }
    }
    swap(&array[i + 1], &array[high]);
    return (i + 1);
}

void quick_sort(int32_t array[], int32_t low, int32_t high) { // Función para ordenar el array
    if (low < high) {
        int pi = partition(array, low, high);
        quick_sort(array, low, pi - 1);
        quick_sort(array, pi + 1, high);
    }
}

int8_t percentage(int32_t value) {
    return 100 - ((value - min_read) * 100 / (max_read - min_read)); // Devolver el porcentaje
}

void send_percentage(int8_t percentage) {
    char buffer[10];
    sprintf(buffer, "%d\r", percentage); // Convertir el porcentaje a cadena
    send_string(buffer);                 // Enviar el porcentaje
}

void send_string(char *string) {
    while (*string) {
        send_char(*string++); // Enviar cada caracter de la cadena
    }
}

void send_char(char character) {
    while (U1STAbits.UTXBF == 1) { // Esperar a que el buffer de transmisión esté vacío
    }
    U1TXREG = character; // Enviar el caracter
}

void __attribute__((__interrupt__, no_auto_psv)) _U1RXInterrupt(void) {
    char data = U1RXREG; // Leer el dato recibido
    switch (data) {
    case '1':
        enable_light_show(); // Habilitar el espectáculo de luces
        break;
    case '2':
        disable_light_show(); // Deshabilitar el espectáculo de luces
        break;
    case '3':
        execute_plant_watering(); // Ejecutar el riego de la planta
        break;
    case '4':
        disable_plant_watering(); // Deshabilitar el riego de la planta
        break;
    case 'r':
        set_colors(P_RED); // Establecer el color rojo
        break;
    case 'g':
        set_colors(P_GREEN); // Establecer el color verde
        break;
    case 'b':
        set_colors(P_BLUE); // Establecer el color azul
        break;
    }
    IFS0bits.U1RXIF = 0; // Limpiar la bandera de interrupción de recepción
}

void enable_light_show(void) {
    if (LATAbits.LATA1 == 0) {
        LATAbits.LATA1 = 1;
    }
}

void disable_light_show(void) {
    if (LATAbits.LATA1 == 1) {
        LATAbits.LATA1 = 0;
    }
}

void execute_plant_watering(void) {
    if (LATAbits.LATA4 == 0) {
        LATAbits.LATA4 = 1; // Encender el relé
    }
}

void set_colors(Color color) {
    int _red = (color.red * 2000) / (255);   // Ajustar el valor del duty cycle para el canal 1 (Rojo)
    int _green = (color.green * 2000) / 255; // Ajustar el valor del duty cycle para el canal 2 (Verde)
    int _blue = (color.blue * 2000) / 255;   // Ajustar el valor del duty cycle para el canal 3 (Azul)

    P1DC1 = _red;   // Canal 1 (Rojo)
    P1DC2 = _green; // Canal 2 (Verde)
    P1DC3 = _blue;  // Canal 3 (Azul)
}

void disable_plant_watering(void) {
    if (LATAbits.LATA4 == 1) {
        LATAbits.LATA4 = 0; // Apagar el relé
    }
}