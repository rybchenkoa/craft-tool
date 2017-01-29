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
#define CFG_INVERTED_AXES      "invertedAxes"
#define CFG_SWITCH_MIN         "switchMin"
#define CFG_SWITCH_MAX         "switchMax"
#define CFG_SWITCH_HOME        "switchHome"
#define CFG_SWITCH_POLARITY    "switchPolarity"
#define CFG_BACK_HOME          "backHome"
#define CFG_COORD_HOME         "coordHome"
