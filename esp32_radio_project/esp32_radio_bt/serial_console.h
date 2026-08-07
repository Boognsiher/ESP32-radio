#pragma once

// Serial-Kommandos (115200 Baud): setbt:NAME, status, scan
namespace SerialConsole {
  void poll();  // liest+verarbeitet ein evtl. anstehendes Kommando (nicht blockierend)
}
