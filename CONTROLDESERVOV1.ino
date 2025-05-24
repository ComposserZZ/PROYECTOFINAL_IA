#include <Servo.h>

Servo miServo;

const int tiempoBase = 150;        // Tiempo de giro por paso
const int pausaEntrePasos = 500;   // Pausa entre pasos
int posicionActual = 0;            // Caja actual (0 a 5)
bool accionPendiente = false;
int posicionObjetivo = 0;

// Mapeo invertido: caja 0 = 20 pesos, caja 5 = 0.5 pesos
const float valores[] = {20, 10, 5, 2, 1, 0.5};

void setup() {
  miServo.attach(9);
  miServo.write(90); // Detener
  Serial.begin(9600);
  Serial.println("Introduce el valor de la moneda (0.5, 1, 2, 5, 10, 20):");
}

void loop() {
  if (Serial.available()) {
    float entrada = Serial.parseFloat();

    // Buscar si la entrada es válida
    bool valido = false;
    for (int i = 0; i < 6; i++) {
      if (entrada == valores[i]) {
        posicionObjetivo = i;
        valido = true;
        break;
      }
    }

    if (valido) {
      accionPendiente = true;
    } else {
      Serial.println("Valor no válido. Intenta con: 0.5, 1, 2, 5, 10 o 20.");
    }

    while (Serial.available()) Serial.read(); // Limpiar buffer
  }

  if (accionPendiente && posicionObjetivo != posicionActual) {
    int diferencia = (posicionObjetivo - posicionActual + 6) % 6;

    // Decidir dirección más corta
    int pasos;
    int direccion; // 1 = derecha, -1 = izquierda

    if (diferencia <= 3) {
      pasos = diferencia;
      direccion = 1;  // Giro horario
    } else {
      pasos = 6 - diferencia;
      direccion = -1; // Giro antihorario
    }

    // Ejecutar pasos
    for (int i = 0; i < pasos; i++) {
      miServo.write(direccion > 0 ? 0 : 180);
      delay(tiempoBase);
      miServo.write(90);
      delay(pausaEntrePasos);
    }

    posicionActual = posicionObjetivo;

    Serial.print("Movido a caja ");
    Serial.print(posicionActual);
    Serial.print(" (valor ");
    Serial.print(valores[posicionActual]);
    Serial.println(" pesos).");
    accionPendiente = false;
  }
}
