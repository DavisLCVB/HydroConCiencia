// Incluyendo librerias necesarias
#include <string.h>  // Funciones con cadenas de caracteres
#include <WiFi.h>  // Funcionalidad de WiFi
#include <HTTPClient.h>  // Permite conexiones HTTP
#include <atomic>  // Permite operaciones atomicas
#include <stdio.h>  // Permite entrada y salida estandar
#include <freertos/FreeRTOS.h>  // Permite el uso de FreeRTOS
#include <freertos/task.h>  // Permite el uso de multiples tareas simultaneas

// Credenciales de la red WiFi
const char* ssid = ""; 
const char* password = "";

// Conexiones con el servidor
const String host = "https://lucd28.pythonanywhere.com/webserver";  // Url del servidor
const int port = 80;  // Numero de puerto

// Configuracion de pines en UART
const uint8_t pin_rx = 16;  // Pin de recepcion
const uint8_t pin_tx = 17;  // Pin de transmision

// Variables atomicas para las lecturas del sensor
std::atomic<double> humedad(0.0);

// Declaracion de multitarea
TaskHandle_t Tarea1;

void setup() {

  Serial.begin(9600);

  // Inicializacion de UART
  Serial1.begin(9600, SERIAL_8N1, pin_rx, pin_tx);

  // Inicializacion de WiFi
  Serial.println("Conectando a WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando...");
  }
  Serial.println("Conectado a la red WiFi");

  // Creacion de tareas
  xTaskCreatePinnedToCore(loop1, "Tarea_1", 18000, NULL, 14, &Tarea1, 1);
  Serial.println("CREADOS");
}


void loop() {

  // Lectura de datos del sensor
  char message[15];
  if (!Serial1.available()) {
    // Si el DSPIC no envia datos, cambia el color del led rgb a rojo
    Serial1.println('r');
  }
  if (Serial1.available()) {
    // Lectura de datos enviados por el DSPIC
    Serial1.readBytesUntil('\r', message, sizeof(message));
    Serial.print(message);
    Serial.print(" - ");
    int a = atoi(message);
    a += 30;
    Serial.println(a);

    // Actualizacion de la humedad en el servidor
    humedad = a;
    updateHumedad();

    // Cambio el estado de regado de acuerdo a la humedad
    if (a < 60) {
      Serial1.println('3');
      Serial1.println('4');
    } else {
      Serial1.println('4');
    }
  }
  if (Serial.available()) {

    // Envio de datos al DSPIC por entrada serial
    char send = Serial.read();
    Serial1.println(send);
  }
}

// Tarea que se encarga de recibir comandos del servidor
void loop1(void* parameter) {
  // Inicializacion de variables
  long espera = -1;
  unsigned long inicio = 0;
  char modo[100];

  while (true) {
    // Llamada a la funcion que obtiene los comandos del servidor
    ObtenerComando(&espera, &inicio, modo);
    vTaskDelay(1000 / portTICK_PERIOD_MS);  // Retardo de 1 segundo
    strcpy(modo, "");
  }
}

void updateHumedad() {
  // Apertura de una solicitud POST al servidor para actualizar el valor de humedad
  HTTPClient cHumedad;
  Serial.println(humedad.load());
  char buffer[40];
  sprintf(buffer, "%f", humedad.load());
  String url_h = host + "/humedad?valor=" + buffer;
  cHumedad.begin(String(url_h));
  cHumedad.addHeader("Content-Type", "text/plain");

  // Envio de datos al servidor
  int rpt = cHumedad.POST("valor=" + String(humedad, 1));
  while (rpt != 200) {
    rpt = cHumedad.POST("valor=" + String(humedad, 1));
  }
  if (rpt > 0) {
    String response = cHumedad.getString();
  }
  cHumedad.end();
}

void ObtenerComando(long* espera, unsigned long* inicio, char modo[]) {
  // Apertura de una solicitud GET al servidor para obtener comandos
  const String host2 = "https://lucd28.pythonanywhere.com/webserver";
  char parametros[5][30];
  HTTPClient http;
  //String comando = "";
  String url = host2 + "/rcomando";
  http.begin(url.c_str());
  http.addHeader("Content-Type", "text/plain");
  int Codigohttp = http.GET();
  while (Codigohttp != 200) {
    Codigohttp = http.GET();
  }


  if (Codigohttp > 0) {
    // Lectura de la respuesta del servidor
    String respuesta = http.getString();
    char comando[80];
    respuesta.toCharArray(comando, 80);

    if (strcmp(comando, "Comando no recibido") != 0) {
      // Si se recibe un comando del servidor, se evalua
      Serial.println("COMANDO");
      Serial.println(comando);
      bool flag = evaluarComando(comando, modo, parametros);

      if (flag == true) {
        // Si el comando es valido, se programa el regado
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        programarRegado(espera, inicio, parametros);

        // Se actualiza el estado del LED RGB a azul en el DSPIC
        Serial1.println('b');

        // Se espera el tiempo programado
        while (millis() - (*inicio) <= (*espera)) {
          Serial.println("ESPERANDO");
        }

        // Se actualiza el estado del LED RGB a verde en el DSPIC
        Serial1.println('g');

        // Se inicia el regado o encendido programado
        Serial.println((*espera));
        Serial.println("REGADO INICIADO");
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        if (strcmp(modo, "regaren") == 0) {
          // Se inicia el regado programado
          iniciarRegadoProgramado();
        } else if (strcmp(modo, "lucesen") == 0) {
          // Se inicia el encendido programado
          iniciarEncendidoProgramado();
        } else if (strcmp(modo, "apagarlucesInmediato") == 0) {
          // Se envia el mensaje para apagar las luces inmediatamente
          apagar_leds();
        } else if (strcmp(modo, "apagarregadoInmediato") == 0) {
          // Se envia el mensaje para apagar el relay inmediatamente
          apagar_relay();
        } else if (strcmp(modo, "regarInmediato") == 0) {
          // Se envia el mensaje para activar el relay inmediatamente
          activar_relay();
        } else if (strcmp(modo, "lucesInmediato") == 0) {
          // Se envia el mensaje para encender las luces inmediatamente
          encender_leds();
        }

        // Se completa la programacion del regado o encendido
        completarProgramacion();

      } else {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        denegarSolicitud();
      }
    }

    // Limpieza de variables
    strcpy(modo, "");
    strcpy(comando, "");

  } else {
    Serial.println("ERROR EN obtenercomando");
    Serial.println("Fallo en la solicitud, Código de estado: " + Codigohttp);
  }

  // Limpieza de variables
  strcpy(parametros[0], "");
  strcpy(parametros[1], "");
  strcpy(parametros[2], "");
  strcpy(modo, "");
  (*espera) = -1;

  http.end();
}

bool evaluarComando(char* comando, char modo[], char parametros[5][30]) {
  // Si es un comando escrito se elimina el caracter '&'
  if (comando[0] == '&') {
    int longitud = strlen(comando);

    for (int i = 0; i < longitud; i++) {
      comando[i] = comando[i + 1];
    }
  }

  Serial.println("Comando evaluado");

  // Se separan los parametros del comando
  int indice = 0;
  char* palabra = strtok(comando, ", ");

  while (palabra != NULL) {
    strcpy(parametros[indice], palabra);
    indice++;
    palabra = strtok(NULL, ", ");
  }

  // Se evalua si el comando es valido
  if (strcmp(parametros[1], "None") == 0) {
    return false;
  }

  // Se copia el modo del comando
  strncpy(modo, parametros[0], sizeof(modo) - 1);
  modo[sizeof(modo) - 1] = '\0';

  return true;
}

void programarRegado(long* espera, unsigned long* inicio, char parametros[5][30]) {

  // Apertura de una solicitud POST al servidor para programar el regado
  HTTPClient cliente;
  String url = "https://lucd28.pythonanywhere.com/webserver/rptcomando?estado=valido";
  cliente.begin(url.c_str());
  int httpResponseCode = cliente.POST("estado=valido");
  while (httpResponseCode != 200) {
    httpResponseCode = cliente.POST("estado=valido");
  }

  if (httpResponseCode > 0) {

    // Se determina el modo de programacion
    if ((strcmp(parametros[0], "regaren") == 0) || (strcmp(parametros[0], "lucesen") == 0)) {
      (*inicio) = millis() / 1000;

      // Se determina el tiempo de espera
      (*espera) = std::stoi(parametros[1]);
      if (strcmp(parametros[2], "seg") == 0 || strcmp(parametros[2], "segundos") == 0) {
        (*espera) *= 1000;
      } else if (strcmp(parametros[2], "min") == 0 || strcmp(parametros[2], "minutos") == 0) {
        (*espera) *= 60000;
      } else if (strcmp(parametros[2], "horas") == 0) {
        (*espera) *= 3600000;
      }

      // Se disminuye el tiempo de espera por el delay presentado en el sistema
      if ((*espera) > 10000) {
        (*espera) -= 10000;
      } else if (((*espera) <= 10000) && ((*espera) > 4000)) {
        (*espera) -= 4000;
      }

    } else if ((strcmp(parametros[0], "lucesInmediato") == 0) || (strcmp(parametros[0], "regarInmediato") == 0)
               || (strcmp(parametros[0], "apagarlucesInmediato") == 0) || (strcmp(parametros[0], "apagarregadoInmediato") == 0)) {
      // En caso la solicitud sea del "boton" del servidor, se inicia el regado o encendido inmediato
      (*inicio) = millis();
      (*espera) = 0;
    }
  } else {
    Serial.println("error en programar regado");
    Serial.println("Fallo en la solicitud, Código de estado: " + httpResponseCode);
  }

  cliente.end();
}

void denegarSolicitud() {

  // Apertura de una solicitud POST al servidor para denegar la solicitud
  HTTPClient cliente;
  String url = "https://lucd28.pythonanywhere.com/webserver/rptcomando?estado=invalido";
  cliente.begin(url.c_str());
  cliente.addHeader("Content-Type", "text/plain");

  int httpResponseCode = cliente.POST("estado=invalido");
  while (httpResponseCode != 200) {
    httpResponseCode = cliente.POST("estado=invalido");
  }
  if (httpResponseCode > 0) {

  } else {
    Serial.println("ERROR EN denegarSOlicitud");
    Serial.println("Fallo en la solicitud, Código de estado: " + httpResponseCode);
  }

  cliente.end();
}

void completarProgramacion() {

  // Apertura de una solicitud POST al servidor para completar la programacion del regado o encendido
  HTTPClient http;
  String url = "https://lucd28.pythonanywhere.com/webserver/riegoprog?pendiente=False";
  http.begin(url.c_str());
  http.addHeader("Content-Type", "text/plain");
  int httpResponseCode = http.POST("pendiente=False");
  while (httpResponseCode != 200) {
    httpResponseCode = http.POST("pendiente=False");
  }

  if (httpResponseCode > 0) {

  } else {
    Serial.println("ERROR EN completarProgramacion");
    Serial.println("Fallo en la solicitud, Código de estado: " + httpResponseCode);
  }
  http.end();
}

void iniciarRegadoProgramado() {

  // Apertura de una solicitud POST al servidor para iniciar el regado programado
  // (Actualizar el estado del boton de regado en el servidor)
  HTTPClient http;
  String url = "https://lucd28.pythonanywhere.com/webserver/regar?state=1";
  http.begin(url.c_str());
  http.addHeader("Content-Type", "text/plain");
  int httpResponseCode = http.POST("state=1");
  while (httpResponseCode != 200) {
    httpResponseCode = http.POST("state=1");
  }

  if (httpResponseCode > 0) {
    
    // Se envia el mensaje para activar el relay
    activar_relay();

  } else {
    Serial.println("ERROR EN iniciarRegadoProgramado");
    Serial.println("Fallo en la solicitud, Código de estado: " + httpResponseCode);
  }

  http.end();
}

void iniciarEncendidoProgramado() {

  // Apertura de una solicitud POST al servidor para iniciar el encendido de luces programado
  // (Actualizar el estado del boton de encendido de luces en el servidor)
  HTTPClient http;
  String url = "https://lucd28.pythonanywhere.com/webserver/encender?state=1";
  http.begin(url.c_str());
  http.addHeader("Content-Type", "text/plain");
  int httpResponseCode = http.POST("state=1");
  while (httpResponseCode != 200) {
    httpResponseCode = http.POST("state=1");
  }

  if (httpResponseCode > 0) {
    // Se envia el mensaje para encender las luces
    encender_leds();

  } else {
    Serial.println("ERROR EN iniciarEncendidoProgramado");
    Serial.println("Fallo en la solicitud, Código de estado: " + httpResponseCode);
  }

  http.end();
}

// Funciones para el control de los actuadores
void activar_relay() {
  Serial1.println('3');
}

void encender_leds() {
  Serial1.println('1');
}

void apagar_leds() {
  Serial1.println('2');
}

void apagar_relay() {
  Serial1.println('4');
}