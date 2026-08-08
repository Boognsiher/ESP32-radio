#pragma once

// I2C-Slave-Seite des in i2c_protocol.h definierten Protokolls.
// Beantwortet Taster-/Scan-Abfragen des S3 und nimmt einen neuen
// BT-Zielnamen entgegen.
namespace I2cSlave {
  void begin();

  // In loop() aufrufen: verarbeitet einen evtl. per I2C_CMD_SET_BT_TARGET
  // empfangenen neuen BT-Zielnamen. Der eigentliche Neustart darf nicht
  // aus dem I2C-ISR-Kontext (onReceive) selbst ausgelöst werden, daher
  // wird hier nur ein Flag gesetzt und in loop() abgearbeitet.
  void handlePendingBtTarget();
}
