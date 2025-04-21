// globals.h
#ifndef GLOBALS_H
#define GLOBALS_H

#include "PrioDateTime.h"  // Zorg dat de header van PrioDateTime bekend is
#include "PrioInputPanel.h"
#include "PrioRfReceiver.h"

extern PrioDateTime pDateTime;  // Declareer als extern (definitie komt elders)
extern PrioInputPanel inputPanel;
extern PrioRfReceiver rfReceiver;

#endif