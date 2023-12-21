#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <Servo.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

const byte ROW_NUM = 4;
const byte COLUMN_NUM = 4;
char keys[ROW_NUM][COLUMN_NUM] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte pin_rows[ROW_NUM] = {9, 8, 7, 6};
byte pin_column[COLUMN_NUM] = {5, 4, 3, 2};
Keypad keypad = Keypad(makeKeymap(keys), pin_rows, pin_column, ROW_NUM, COLUMN_NUM);

const int trigPin = 10;
const int echoPin = 11;

Servo servo1;
Servo servo2;

struct Usuario {
  String numeroTarjeta;
  String pin;
  float saldo;
};

Usuario usuarios[] = {
  {"9726354826", "2222", 2000.0},  // Usuario 1
  {"9888657561", "0222", 15000.0},  // Usuario 2
  {"9286097873", "1111", 9000.0}   // Usuario 3
};

int indiceUsuarioActual = -1;

bool tarjetaInsertada = false;
int intentosFallidos = 0;
const int maxIntentos = 3;
bool tarjetaBloqueada = false;
unsigned long tiempoDesbloqueo = 0;

void setup() {
  Serial.begin(9600);
  initializeLCD();
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  servo1.attach(12);
  servo2.attach(13);
   servo1.write(0);
    servo2.write(0);
}

void loop() {
  if (!tarjetaBloqueada) {
    while (!tarjetaInsertada) {
      if (detectCardInsertion()) {
        tarjetaInsertada = true;
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Ingrese su PIN:");
        Serial.println("Tarjeta detectada. Ingrese su PIN.");
      }
    }

    String pinCode = getPIN();

    for (int i = 0; i < sizeof(usuarios) / sizeof(usuarios[0]); i++) {
      if (validatePIN(pinCode, usuarios[i])) {
        indiceUsuarioActual = i;
        lcd.clear();
        lcd.setCursor(0, 0);
        displayMessage("1. Consulta");
        displayMessage("2. Retiro");
        char userChoice = getKey();
        switch (userChoice) {
          case '1':
            checkBalance();
            break;
          case '2':
            if (confirmOperation()) {
              lcd.clear();
              lcd.setCursor(0, 0);
              displayMessage("Ingrese el monto:");
              int withdrawAmount = getAmount();
              if (withdrawAmount >= 100 && withdrawAmount <= usuarios[indiceUsuarioActual].saldo) {
                withdrawCash(withdrawAmount);
              } else {
                lcd.clear();
                lcd.setCursor(0, 0);
                if (withdrawAmount > usuarios[indiceUsuarioActual].saldo) {
                  displayMessage("Cuenta con $" + String(usuarios[indiceUsuarioActual].saldo));
                } else {
                  displayMessage("Monto no Valido");
                }
                Serial.println("Operación cancelada: Monto no válido.");
                delay(2000);
                tarjetaInsertada = false;
                resetDisplay();
                intentosFallidos = 0;
              }
            }
            break;
        }
        tarjetaInsertada = false;
        resetDisplay();
        intentosFallidos = 0;
        break;
      }
    }

    if (indiceUsuarioActual == -1) {
      intentosFallidos++;
      if (intentosFallidos >= maxIntentos) {
        lcd.clear();
        lcd.setCursor(0, 0);
        displayMessage("Tarjeta bloqueada");
        Serial.println("Tarjeta bloqueada después de 3 intentos fallidos.");
        tiempoDesbloqueo = millis() + 10000;
        tarjetaBloqueada = true;
      } else {
        lcd.clear();
        lcd.setCursor(0, 0);
        displayMessage("PIN incorrecto");
        Serial.println("PIN incorrecto. Inténtelo de nuevo.");
        delay(2000);
      }
    }
  } else {
    if (millis() >= tiempoDesbloqueo) {
      tarjetaBloqueada = false;
      resetDisplay();
      indiceUsuarioActual = -1;
    } else {
      lcd.clear();
      lcd.setCursor(0, 0);
      displayMessage("Tarjeta bloqueada");
      Serial.println("Tarjeta bloqueada. Contacte al banco.");
      delay(2000);
    }
  }
}

bool detectCardInsertion() {
  while (!ultrasonicSensorDetects()) {
    delay(500);
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ingrese su PIN:");
  Serial.println("Tarjeta detectada. Ingrese su PIN.");
  return true;
}

bool ultrasonicSensorDetects() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH);
  float distance = duration * 0.034 / 2;
  return (distance < 5);
}

String getPIN() {
  String pinCode = "";
  char key;
  do {
    key = getKey();
    if (key != NO_KEY) {
      if (key != '#') {
        pinCode += key;
        lcd.setCursor(pinCode.length() - 1, 1);
        lcd.print('*');
      }
    }
    delay(500);
  } while (key != '#' && tarjetaInsertada);
  return pinCode;
}

bool validatePIN(String pin, Usuario& usuario) {
  bool isValid = pin == usuario.pin;
  if (!isValid) {
    Serial.println("PIN incorrecto. Inténtelo de nuevo.");
  }
  return isValid;
}

char getKey() {
  char key = keypad.getKey();
  while (key == NO_KEY) {
    key = keypad.getKey();
    delay(100);
  }
  return key;
}

void checkBalance() {
  lcd.clear();
  lcd.setCursor(0, 0);
  displayMessage("Saldo: $" + String(usuarios[indiceUsuarioActual].saldo));
  Serial.println("Consulta de saldo. Saldo actual: $" + String(usuarios[indiceUsuarioActual].saldo));
  delay(2000);
}

bool confirmOperation() {
  lcd.clear();
  lcd.setCursor(0, 0);
  displayMessage("Presione 'C' para confirmar");
  Serial.println("Esperando confirmación ('C') para la operación.");
  char key = getKey();
  return (key == 'C');
}

int getAmount() {
  String amountStr = "";
  char key;
  do {
    key = getKey();
    if (key != NO_KEY && key != '#') {
      amountStr += key;
      lcd.clear();
      lcd.setCursor(0, 0);
      displayMessage("Ingrese el monto:");
      displayMessage(amountStr);
    }
    delay(500);
  } while (key != '#' && tarjetaInsertada);
  int amount = amountStr.toInt();
  if (amount < 100 || amount > usuarios[indiceUsuarioActual].saldo) {
    lcd.clear();
    lcd.setCursor(0, 0);
    if (amount > usuarios[indiceUsuarioActual].saldo) {
      displayMessage("Limite Excedido");
    } else {
      displayMessage("Monto no Valido");
    }
    Serial.println("Operación cancelada: Monto no válido.");
    delay(2000);
    amount = getAmount();
  }
  return amount;
}

void withdrawCash(int amount) {
  if (amount >= 100) {
    lcd.clear();
    lcd.setCursor(0, 0);
    displayMessage("Retirando $" + String(amount));
    Serial.println("Retirando $" + String(amount));
    delay(2000);
    usuarios[indiceUsuarioActual].saldo -= amount;
    servo1.write(180);
    servo2.write(180);
    delay(2000);
    servo1.write(0);
    servo2.write(0);
    lcd.clear();
    lcd.setCursor(0, 0);
    displayMessage("Operacion completada");
    Serial.println("Saldo restante: $" + String(usuarios[indiceUsuarioActual].saldo));
    delay(2000);

    // Mensaje adicional después de cada retiro
    lcd.clear();
    lcd.setCursor(0, 0);
    displayMessage("Cuenta con $" + String(usuarios[indiceUsuarioActual].saldo));
    Serial.println("Saldo restante: $" + String(usuarios[indiceUsuarioActual].saldo));
    delay(2000);
  } else {
    lcd.clear();
    lcd.setCursor(0, 0);
    displayMessage("Monto minimo: $100");
    displayMessage("Ingrese el monto:");
    lcd.setCursor(0, 1);
    displayMessage("Limite Excedido");
    int nuevoMonto = getAmount();
    if (nuevoMonto >= 100 && nuevoMonto <= usuarios[indiceUsuarioActual].saldo) {
      withdrawCash(nuevoMonto);
    } else {
      lcd.clear();
      lcd.setCursor(0, 0);
      if (nuevoMonto > usuarios[indiceUsuarioActual].saldo) {
        displayMessage("Limite Excedido");
      } else {
        displayMessage("Monto no valido");
      }
      Serial.println("Operación cancelada: Monto no válido.");
      delay(2000);
      tarjetaInsertada = false;
      resetDisplay();
    }
  }
  tarjetaInsertada = false;
  resetDisplay();
}

void resetDisplay() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("   J&J Bank");
  lcd.setCursor(0, 1);
  lcd.print("Ingrese Tarjeta");
  tarjetaInsertada = false;
}

void initializeLCD() {
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("   J&J Bank");
  lcd.setCursor(0, 1);
  lcd.print("Ingrese Tarjeta");
}

void displayMessage(String message) {
  lcd.print(message.substring(0, 16));
  lcd.setCursor(0, 1);
  lcd.print(message.substring(16, 32));
}

