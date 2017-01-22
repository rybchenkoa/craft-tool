#pragma once
#include "Config.h"
extern Config *g_config;
#define CFG_CONFIG_NAME        "config.cfg"

#define CFG_COM_PORT_NUMBER    "comPort"
#define CFG_STEPS_PER_MM       "stepsPermm"
#define CFG_MAX_VELOCITY       "maxVelocity"
#define CFG_MAX_ACCELERATION   "maxAcceleration"
#define CFG_USED_COORDS        "usedCoords"
#define CFG_SLAVE              "slave"
#define CFG_AXE_MAP            "axeMap"