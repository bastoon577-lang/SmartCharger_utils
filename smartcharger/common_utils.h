#ifndef __COMMON_UTILS__
#define __COMMON_UTILS__

#include <stdint.h>

#include "../../hal_utils.h"

//< MACRO de version logicielle
#define V_LOGICIEL                "v0.1"                // Version Logicielle

//< MACRO du Mode Access Point
#define AP_SSID                   "SmartCharger"        // SSID SmartCharger Access Point
#define AP_PASS                   AP_SSID               // Password SmartCharger Access Point
#define AP_CHANNEL                0                     // Channel SmartCharger Access Point
#define AP_VISIBILITE             false                 // Visibilite SmartCharger Access Point
#define AP_MAX_CONN               1                     // Nombre de connexion SmartCharger Access Point
#define AP_TCP_PORT               8082                  // Port TCP SmartCharger Access Point

//< Define des Timeout
#define TIMEOUT_SCAN_NETWORK      300000                // Temps de scrutation des réseaux Wifi disponibles (5 Minutes)
#define TIMEOUT_REBOOT            1000                  // Temps d'anti-rebond pour le reboot (1 Seconde)
#define TIMEOUT_LED_LENT          500                   // Clignotement lent de la LED
#define TIMEOUT_LED_RAPI          50                    // Clignotement rapide de la LED

//< Define de gestion d'intensite sur 5 bits (optimisation spatiale de l'intensité en mémoire EEPROM)
#define READ_OFFSET_5B(x)         (x+6)
#define WRITE_OFFSET_5B(x)        (x-6)

//< Structure de Configurations fonctionnelles figées (Configurable uniquement à la configuration de l'équippement)
typedef struct
{
  char                  Hostname[65];                   // Hostname
  char                  SmSsid[33];                     // SSID
  char                  SmPass[65];                     // Password
  uint8_t               address[4];                     // Adresse IP Static
  uint8_t               subnet[4];                      // Masque de sous reseau
  uint8_t               gateway[4];                     // Passerelle
  uint16_t              port;                           // Port de service Web
  uint16_t              portWs;                         // Port de service WebSocket
  uint8_t               is_configured         : 1;      // Bitfield indiquant que l'équippement est configuré
  uint8_t               is_wifi_network_used  : 1;      // Bitfield indiquant un accrochage sur une borne Wifi
  uint8_t               which_voltage         : 1;      // Bitfield indiquant la tension du reseau (0 : 230V & 1 : 400V)
  uint8_t               is_tic_module_used    : 1;      // Bitfield indiquant qu'un ModuleTIC externe est actif
  uint8_t               RUF                   : 4;      // Reservé Usage Future
} STATIC_CONF_FIELDS_t;

//< Structure de Configurations du Module TIC figées (Configurable uniquement à la configuration de l'équippement)
typedef struct
{
  char                  ip_or_hostname[65];             // IP ou Hostname du Module TIC
  uint16_t              portWs;                         // Port de service WebSocket du Module TIC 
} TIC_CONF_FIELDS_t;

//< Structure de Configurations fonctionnelles qui peuvent evoluer dans le temps (Configurable à la volé dans l'onglet d'exploitation)
typedef struct
{
  uint16_t limited_current                    :5;       // Intensite lors de l'absence d'asservissement dynamique (WARNING : I = intensity + 6)
  uint16_t off_peak_hours                     :1;       // Charge sur les heures creuses uniquement (1 : Heures creuses uniquement)
  uint16_t tic_slaved                         :1;       // Charge asservie par les donnees TIC (1 : OUI)
  uint16_t theme                              :1;       // Thème (0 : foncé / 1 : clair)
  uint16_t RUF                                :8;       // Réservé Usage Futures
} VOLATILE_CONF_FIELDS_t;

//< Structure des données TIC extraites au travers du Module TIC et du service WebSocket
typedef struct
{
  uint8_t isousc;                                       // Intensite maximale (A ne pas depasser !)
  uint8_t iinst;                                        // Intensite absorbé (Monophasé)
  uint8_t iinst_1;                                      // Intensite phase 1 absorée (Triphasé)
  uint8_t iinst_2;                                      // Intensite phase 2 absorée (Triphasé)
  uint8_t iinst_3;                                      // Intensite phase 3 absorée (Triphasé)
  uint16_t papp;                                        // Puissance apparente absorbée
  char ptec[5];                                         // Tarif actuel (HC.. ou HP..)
} TIC_DATA_t;

//< Enumération des états du SmartCharger
typedef enum 
{
  charge_state_not_Connected      = 10,
  charge_state_connected          = 20,
  charge_state_charging           = 30,
  charge_state_charging_degraded  = 31,
  charge_state_wait_hc            = 40,
  charge_state_wait_hc_default    = 41,
  charge_state_default            = 50
}CHARGE_STATE_e;

//< Structure de paramètres de charge VE
typedef struct
{
  CHARGE_STATE_e state;                                 // Etat de charge
  uint8_t current;                                      // Courant de charge
} CHARGE_PARAMETERS_t;

//< Enumération des types de sauvegardes en mémoire EEPROM
enum EepromDataType {
  EEPROM_STATIC_CFG,                                    // Configuration des données STATIC
  EEPROM_VOLATILE_CFG,                                  // Configuration des données VOLATILE
  EEPROM_TIC_CFG                                        // Configuration du Module TIC
};

#endif