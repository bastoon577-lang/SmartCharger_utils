#ifndef __SMART_CHARGER_UTILS__
#define __SMART_CHARGER_UTILS__

#include "common_utils.h"

//< Defines des TIMEOUT
#define TIMEOUT_SCRUT_WS_TIC_LONG 30000     // Temps de scrutation long de l'état de connexion WS Module TIC
#define TIMEOUT_SCRUT_WS_TIC_FAST 6000      // Temps de scrutation rapide de l'état de connexion WS Module TIC
#define TIMEOUT_LAST_IVE_INCREASE 6000      // Temps de la dernière augmentation de courant de charge
#define TIMEOUT_LAST_IVE_DECREASE 2000      // Temps de la dernière diminution de courant de charge
#define TIMEOUT_SCRUT_EVSE        1000      // Temps de scrutation de l'état de l'EVSE

//< Defines décisionels de puissance
#define BIG_GAP_GRID_CURRENT      4         // Le GAP est important (aussi bien en surcharge qu'en sous-charge)
#define NULL_GAP_GRID_CURRENT     0         // Le GAP est très faible (proche de zéro)

//< Defines autres
#define NB_LIMIT_TRY_CONNEXIONS   2         // Nombre d'essais de connexions avant passage en charge dégradée
#define START_CHARGE_CURRENT      15        // Courant de démarrage de charge (lié au ISCOUS minimum EDF)

//< Enumération des Timers de charge
typedef enum 
{
  charger_scrut_ws_state,
  charger_scrut_tic_module,
  charge_nb_timers
}CHARGE_TIMERS_e;

//< Structure de données Charger
typedef struct
{
  VOLATILE_CONF_FIELDS_t *volatile_conf;    // Pointeur sur les données VOLATILE_CONF_FIELDS_t
  STATIC_CONF_FIELDS_t *static_conf;        // Pointeur vers la structure STATIC_CONF_FIELDS_t
  CHARGE_PARAMETERS_t parameters;           // Paramètres de charge VE
  uint8_t is_charge_active     :1;          // Bitfield de Charge en cours
  uint8_t is_limited_charge    :1;          // Bitfield de Charge dégradée suite à la perte de connexion au Module TIC
  uint8_t is_hc_active         :1;          // Bitfield d'heures creuses en cours...
  uint8_t flag_lock_evse       :1;          // Bitfield de blocage de l'EVSE
  uint8_t flag_scrut_evse      :1;          // Bitfield de lecture de l'EVSE
  uint8_t RUF                  :3;          // Réservé Usage Futur
} CHARGER_t;

/**
 * \fn void sm_charger_init(STATIC_CONF_FIELDS_t *static_conf, VOLATILE_CONF_FIELDS_t *volatile_conf)
 * \brief Fonction permettant d'initialiser le service CHARGER
 * \param in, le pointeur vers la structure STATIC_CONF_FIELDS_t
 * \param in, le pointeur vers la structure VOLATILE_CONF_FIELDS_t
 */
void sm_charger_init(STATIC_CONF_FIELDS_t *static_conf, VOLATILE_CONF_FIELDS_t *volatile_conf);

/**
 * \fn void sm_charger_handler(void)
 * \brief Handler du service CHARGER à appeler régulièrement
 */
void sm_charger_handler(void);

#endif
