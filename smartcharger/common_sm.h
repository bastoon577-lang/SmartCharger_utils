#ifndef __COMMON_SM__
#define __COMMON_SM__

#include <stdint.h>

#include "../../hal_utils.h"

//< MACRO de version logicielle SmartCharger_utils
#define V_SM_LOGICIEL             "v1.0.4"              // Version Logicielle

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
  uint16_t degraded_current                   :5;       // Intensite dégradée en perte de communication TIC (WARNING : I = intensity + 6)
  uint16_t limite_current                     :5;       // Intensite de charge limite (WARNING : I = intensity + 6)
  uint16_t off_super_peak_hours               :1;       // Charge sur les heures super creuses uniquement (1 : Heures super creuses)
  uint16_t off_peak_hours                     :1;       // Charge sur les heures creuses uniquement (1 : Heures creuses)
  uint16_t solar_active						  :1;		// Charge privilégiant l'énergie solaire
  uint16_t theme                              :1;       // Thème (0 : foncé / 1 : clair)
  uint16_t RUF                                :2;       // Réservé Usage Futures
} VOLATILE_CONF_FIELDS_t;

//< Structure de données TIC extraites au travers du Module TIC et du service WebSocket
typedef struct
{
  uint16_t p_injectee;									// Puissance injectée sur le réseau
  uint8_t i_inst[3];									// Intensite phase(s) absorée(s) (Monophasé & Triphasé)
  char tarif[19];										// Tarif actuel appliqué (Heures Creuses/Pleines)
  uint8_t i_max;										// Intensite max pouvant être absorbé avant disjonction
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
  charge_state_default            = 50,
  charge_state_default_et3k       = 51
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
