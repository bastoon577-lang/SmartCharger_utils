#include "sm_charger_utils.h"
#include "websocket_utils.h"
#include "common_utils.h"

//< Déclaration des variables globales
static TIC_DATA_t tic_data;
static CHARGER_t charge;

/*
 * **************************************************************************************
 *                    Fonctions privées de Gestion du SmartCharger
 * **************************************************************************************
 */
/**
 * \fn void void sm_charger_init_state(void)
 * \brief Fonction permettant l'initialisation de l'état et du courant
 */
static inline void sm_charger_init_state(void) {
  charge.parameters.state   = charge_state_not_Connected;     // Positionnement de l'état en Attente de VE
  charge.parameters.current = START_CHARGE_CURRENT;           // Positionnement du courant initial
}

/**
 * \fn void void sm_charger_adopt_limit_current(void)
 * \brief Fonction permettant d'adopter le courant limite
 */
static inline void sm_charger_adopt_limit_current(void) {
  hal_disable_interrupt();
  charge.parameters.current =
      READ_OFFSET_5B(charge.volatile_conf->limited_current);  // Adoption du courant limite
  hal_enable_interrupt();
}

/**
 * \fn void int8_t sm_charger_compute_available_current(void)
 * \brief Fonction permettant de calculer le courant disponible sur le 
 *        réseau de distribution en Monophasé ou en Triphasé.
 * \return
 *    Le courant disponible en Ampères
 */
static int8_t sm_charger_compute_available_current(void) {
  enum {phase_1,phase_2,phase_3,nb_phases};
  int8_t available_current[nb_phases] = {0};
  int8_t ret;
  
  if(!charge.static_conf->which_voltage) {                    // Le réseau est de type Monophasé
    ret = tic_data.isousc - tic_data.iinst;                   // Calcul par l'utilisation de IINST uniquement
  } else {                                                    // Autrement, le réseau est de type Triphasé
    // Calculs des courants disponibles sur chaque phases
    available_current[phase_1] = tic_data.isousc - tic_data.iinst_1;
    available_current[phase_2] = tic_data.isousc - tic_data.iinst_2;
    available_current[phase_3] = tic_data.isousc - tic_data.iinst_3;
    
    /// Recherche du courant minimum disponible sur le réseau
    ret = available_current[phase_1];
    for(uint8_t p=0;p<nb_phases;p++) {
      if(available_current[p] < ret)
        ret = available_current[p];
    }
  }
  return ret;
}

/**
 * \fn void void sm_charger_waiting_before_increase(uint8_t increase_value)
 * \brief Fonction permettant de une attente avant augmentation du courant de consigne.
 * \param in, la valeur d'incrément du courant de charge
 */
static inline void sm_charger_waiting_before_increase(uint8_t increase_value) {
  static unsigned long timer_last_increase = millis();
  if(millis() - timer_last_increase >= TIMEOUT_LAST_IVE_INCREASE) {
    charge.parameters.current+=increase_value;                // Augmentation par la valeur passée en paramètre
    timer_last_increase = millis();
  }
}

/**
 * \fn void void sm_charger_waiting_before_decrease(uint8_t decrease_value)
 * \brief Fonction permettant de une attente avant diminution du courant de consigne.
 * \param in, la valeur de décrément du courant de charge
 */
static inline void sm_charger_waiting_before_decrease(uint8_t decrease_value) {
  static unsigned long timer_last_decrease = millis();
  if(millis() - timer_last_decrease >= TIMEOUT_LAST_IVE_DECREASE) {
    charge.parameters.current-=decrease_value;                // Diminution par la valeur passée en paramètre
    timer_last_decrease = millis();
  }
}

/**
 * \fn void sm_charger_adjust_current(int8_t available_current)
 * \brief Fonction permettant la gestion du courant disponible pour le VE.
 *        Cette fonction permet l'adaptation du courant de charge par l'intermédiaire 
 *        des données issues du Module TIC. La stratégie consiste à diminuer/augmenter la consigne
 *        en utilisant une néanmoins une priorité vis à vis des diminutions de courant face aux augmentations.
 *        L'intérêt ici est de préserver le réseau et d'effectuer un "PID" lent mais efficace.
 * \param in, le courant disponible sur le réseau
 */
static void sm_charger_adjust_current(int8_t available_current) {
  if(!ws_client_is_connected())
    return;
    
  if(available_current < -(BIG_GAP_GRID_CURRENT)) {           // Une surcharge du réseau trop importante est constatée !
    charge.parameters.current = MINIMAL_CHARGE_CURRENT;       // Diminution drastique du courant de charge
  } else if (available_current < NULL_GAP_GRID_CURRENT) {     // Une petite surcharge du réseau est constatée
    if(charge.parameters.current > MINIMAL_CHARGE_CURRENT)    // Le courant de charge peut-il être diminué ?
      sm_charger_waiting_before_decrease(1);                  // Diminution de 1A du courant de charge
    else {
      /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
      // AUTREMENT, il est nécessaire de couper la charge !!!
      // Continuer d'observer les données TIC en attente que courant disponible soit > 6 pour repasser en charge !
      /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    }
  } else if (available_current == NULL_GAP_GRID_CURRENT) {    // La puissance maximale est atteinte
    // NE RIEN FAIRE, LA TANGENTE SUR LA PUISSANCE MAXIMALE DU COMPTEUR EST ATTEINTE !
  } else if (available_current < BIG_GAP_GRID_CURRENT) {      // Le réseau peut fournir d'avantage de puissance
    if(charge.parameters.current < MAXIMAL_CHARGE_CURRENT) {  // Le courant de charge peut-il être augmenté ?
      sm_charger_waiting_before_increase(1);                  // Augmentation de 1A du courant de charge
    }
  } else {                                                    // Le réseau peut fournir beaucoup de puissance !
    sm_charger_waiting_before_increase(4);                    // Augmentation de 4A du courant de charge
  }
  
  // Néanmoins, il est important de ne pas excéder la puissance maximale du réseau !
  if(charge.parameters.current > tic_data.isousc) {
    if(tic_data.isousc)                                       // Uniquement s'il possède une valeur (Ne peut être égal à 0) !
      charge.parameters.current = tic_data.isousc;
  }
  // Mais aussi, ne pas excéder la puissance maximale admissible par le VE !
  if(charge.parameters.current > MAXIMAL_CHARGE_CURRENT)
    charge.parameters.current = MAXIMAL_CHARGE_CURRENT;
}

/**
 * \fn void sm_charger_manage_tic_connection(uint8_t *try_connexions)
 * \brief Fonction permettant la gestion de connexion de la WS au Module TIC
 * \param out, le pointeur du nombre de tentatives de connexions
 */
static void sm_charger_manage_tic_connection(uint8_t *try_connexions) {
  static bool connexion_flag = false;
  
  bool need_connection =                                      // La connexion WS Module TIC est nécessaire
    charge.volatile_conf->off_peak_hours ||                   // Lors de la configuration de charge en Heures Creuses uniquement
    (charge.is_charge_active && charge.volatile_conf->tic_slaved);// Lorsqu'une charge asservie aux données TIC est en cours... 
  
  if(!need_connection) {                                      // Aucune connexion n'est nécessaire
    if(ws_client_is_connected() || connexion_flag) {          // La connexion est établie ou en cours...
      ws_client_disconnect_from_tic_module();                 // Forcer la déconnexion
      connexion_flag = false;
      *try_connexions = 0;
    }
    return;                                                   // Echappement
  }
  
  if(ws_client_is_connected()) {                              // Si la connexion est établie
    *try_connexions = 0;                                      // Réinitialisation du nombre de tentatives
    return;                                                   // Echappement
  }
  
  if(connexion_flag) {
    ws_client_disconnect_from_tic_module();
    connexion_flag = false;
  } else {
    ws_client_connect_on_tic_module(&tic_data);
    connexion_flag = true;
    *try_connexions+=1;
    if(*try_connexions > NB_LIMIT_TRY_CONNEXIONS)
      *try_connexions = NB_LIMIT_TRY_CONNEXIONS + 1;          // Maintien du nombre de tentatives de reconnexions (éviter l'overflow)
    charge.flag_scrut_evse = 0;  	                          // Lecture de l'EVSE non autorisée
  }
}

/**
 * \fn void sm_charger_update_hc_state(void)
 * \brief Fonction permettant la gestion de charge en Heures Creuses
 *        au travers de la connexion WS au Module TIC
 */
static void sm_charger_update_hc_state(void) { 
  if(!charge.volatile_conf->off_peak_hours) {               // Aucune configuration de charge en heure creuse effective ?
    charge.is_hc_active = 0;                                // Réinitialisation du flag (Ce cas ne doit pas arriver)
    return;                                                 // Echappement
  }

  if(!ws_client_is_connected())                             // Pas de connexion effective
    return;                                                 // Echappement
  
  if(!memcmp(tic_data.ptec,"HC..",4))                       // Heures creuses en cours...
    charge.is_hc_active = 1;                                // La charge est autorisée
  
  if(!memcmp(tic_data.ptec,"HP..",4))                       // Heures pleines en cours...
    charge.is_hc_active = 0;                                // La charge n'est pas autorisée
}

/**
 * \fn void sm_charger_manage_degraded_mode(uint8_t *try_connexions)
 * \brief Fonction permettant la gestion du mode dégradé lors de la perte de connexion 
 *        avec le Module TIC sur le réseau
 * \param in, le nombre de tentatives de connexions
 */
static void sm_charger_manage_degraded_mode(uint8_t try_connexions) {
  if(!charge.is_charge_active)
    return;
    
  if(try_connexions == NB_LIMIT_TRY_CONNEXIONS) {
    charge.parameters.state = charge_state_charging_degraded;
    charge.is_limited_charge = 1;
  }
  
  if(charge.is_limited_charge &&
     !try_connexions) {
    charge.parameters.state = charge_state_charging;
    charge.is_limited_charge = 0;
  }
  
  if(!charge.is_limited_charge)
    return;

  uint8_t limit = READ_OFFSET_5B(charge.volatile_conf->limited_current);
  if(charge.parameters.current > limit)                                       
    charge.parameters.current = limit;                        // Adoption du courant limite configuré
  // Autrement le courant reste inchangé (déjà limité par l'asservissement précédent !)
}

/**
 * \fn void sm_charger_process_tic_data(void)
 * \brief Fonction permettant la gestion du process de charge en utilisant les données du Module TIC
 */
static void sm_charger_process_tic_data(void) {
  
  sm_charger_update_hc_state();                               // Vérification des HP/HC

  if(charge.volatile_conf->tic_slaved) {                      // La configuration d'asservissement aux données TIC est active
    int8_t available_current = 
      sm_charger_compute_available_current();                 // Calcul du courant disponible sur le réseau électrique
    sm_charger_adjust_current(available_current);             // Ajustement du courant de charge VE
  } else {                                                    // Autrement, la charge n'est pas asservie
    sm_charger_adopt_limit_current();                         // Adoption du courant limite
  }
}

/**
 * \fn void sm_charger_charge_with_tic_module(uint8_t *try_connexions)
 * \brief Fonction permettant la gestion de la charge avec les données issues du Module TIC au travers du réseau. 
 *        Cette méthode permet un asservissement dynamique de la puissance de charge du VE en fonction 
 *        de la puissance instantanée du réseau électrique.
 * \param out, le pointeur du nombre de tentatives de connexions
 */
static void sm_charger_charge_with_tic_module(uint8_t *try_connexions) {
  static unsigned long timer_scrut_ws_connexion = millis();

  // Gestion de connexion avec la WS avec le Module TIC
  unsigned long time_to_scrut = 
                (*try_connexions>NB_LIMIT_TRY_CONNEXIONS)?TIMEOUT_SCRUT_WS_TIC_LONG:TIMEOUT_SCRUT_WS_TIC_FAST;
  if(millis() - timer_scrut_ws_connexion >= time_to_scrut) {
    sm_charger_manage_tic_connection(try_connexions);
    timer_scrut_ws_connexion = millis();
  }

  sm_charger_process_tic_data();                              // Gestion du processus de charge avec les données TIC

  charge.flag_lock_evse = 0;          	// Déblocage de l'EVSE, celui-ci sera bloqué après si nécessaire

  if(charge.volatile_conf->off_peak_hours) {                  // La configuration Heures Creuses est active
    if(!charge.is_hc_active) {                                // Les Heures Creuses ne sont pas en cours
      if(ws_client_is_connected())
        charge.parameters.state = charge_state_wait_hc;       // Positionnement de l'état en Attente HC
      else
        charge.parameters.state = charge_state_wait_hc_default;
      charge.flag_scrut_evse = 0;                         	  // Lecture de l'EVSE non autorisée
      charge.flag_lock_evse = 1;                              // Blocage de l'EVSE
      return;                                                 // Echappement
    }
    // Autrement, les Heures Creuses sont en cours...
    if(charge.parameters.state == charge_state_wait_hc_default ||
       charge.parameters.state == charge_state_wait_hc)
      sm_charger_init_state();
  
  } else {                                                    // La configuration Heures Creuses n'est pas active
                                                              // Elle a été désactivée par l'utilisateur
    if(charge.parameters.state == charge_state_wait_hc_default ||
       charge.parameters.state == charge_state_wait_hc)
      sm_charger_init_state();
  }

  sm_charger_manage_degraded_mode(*try_connexions);            // Gestion du mode dégradé
}

/**
 * \fn void sm_charger_charge_without_tic_module(void)
 * \brief Fonction permettant la gestion de la charge sans Module TIC
 *        La puissance de charge est donc statique et déterminée par l'utilisateur.
 */
static void sm_charger_charge_without_tic_module(void) {
  sm_charger_adopt_limit_current();                           // Adoption du courant limite
}

/*
 * **************************************************************************************
 *                    Fonctions publiques de Gestion du SmartCharger
 * **************************************************************************************
 */
/**
 * \fn void sm_charger_init(STATIC_CONF_FIELDS_t *static_conf, VOLATILE_CONF_FIELDS_t *volatile_conf)
 */
void sm_charger_init(STATIC_CONF_FIELDS_t *static_conf, VOLATILE_CONF_FIELDS_t *volatile_conf) {
  ws_server_set_charge_parameters(&charge.parameters);
  charge.volatile_conf = volatile_conf;
  charge.static_conf = static_conf;

  sm_charger_init_state();
  hal_evse_init();                                            // Initialisation du service Viridian
}

/**
 * \fn void sm_charger_handler(void)
 */
void sm_charger_handler(void) {
  static CHARGE_STATE_EVSE_e previous_evse_state, current_evse_state;
  static CHARGE_PARAMETERS_t previous_charge_parameters;
  static unsigned long timer_scrut_evse = millis();
  static uint8_t try_connexions;

  /*************************************************************************************************************
   *        Etage de contrôle du SmartCharger; cet étage permet le gestion intelligente du
   *        mécanisme de recharge VE avec ou sans Module TIC.
   ************************************************************************************************************/
  charge.flag_scrut_evse = 1;                                 // Autorisation de lecture de l'EVSE qui sera désautorisé si nécessaire
  if(!charge.static_conf->is_tic_module_used)                 // Le Module TIC n'est pas configuré ?
    sm_charger_charge_without_tic_module();                   // Utilisation du mécanisme de charge sans le Module TIC
  else                                                        // Autrement
    sm_charger_charge_with_tic_module(&try_connexions);       // Utilisation du mécanisme de charge avec le Module TIC

  /*************************************************************************************************************
   *        Etage de contrôle de l'EVSE
   ************************************************************************************************************/
  hal_evse_update_input();                                    // Scrutation de l'EVSE (Mode Polling)
  if(millis() - timer_scrut_evse >= TIMEOUT_SCRUT_EVSE) {    
    timer_scrut_evse = millis();

    if(charge.flag_scrut_evse) {                              // La lecture de l'état de l'EVSE est autorisée
      current_evse_state = hal_evse_get_state();
      if(current_evse_state != previous_evse_state) {         // L'état de l'EVSE à changé ?
        previous_evse_state = current_evse_state;             // Mise à jour vers l'état courant
        switch(current_evse_state) {
          case evse_Connected:
            charge.parameters.current = START_CHARGE_CURRENT;
            charge.parameters.state = charge_state_connected;
            charge.is_charge_active = 0;
            break;
          case evse_Charging:
            charge.parameters.state = charge_state_charging;
            charge.is_charge_active = 1;
			try_connexions = 0;
            break;
          case evse_Fault:
            charge.parameters.current = START_CHARGE_CURRENT;
            charge.parameters.state = charge_state_default;
            charge.is_charge_active = 0;
            break;
          case evse_Not_Connected:
          default:
            charge.parameters.state = charge_state_not_Connected;
            charge.parameters.current = START_CHARGE_CURRENT;
            charge.is_charge_active = 0;
            break;
        }
      }
    }
  }

  /*************************************************************************************************************
   *        Etage de pilotage de l'EVSE.
   ************************************************************************************************************/
  if(charge.flag_lock_evse)                                   // L'EVSE doit il être LOCK ?
    hal_lock_current();
  else                                                        // Autrement
    hal_set_current(charge.parameters.current);

  /*************************************************************************************************************
   *        Etage de mise à jour de l'IHM Web au travers de la WebSocket Serveur
   *        Uniquement lors d'un changement de valeur (State ou Current)
   ************************************************************************************************************/
  if(memcmp(&previous_charge_parameters,&charge.parameters,sizeof(CHARGE_PARAMETERS_t)) != 0) {
    memcpy(&previous_charge_parameters,&charge.parameters,sizeof(CHARGE_PARAMETERS_t));
    ws_server_send_broadcast(); 
  }
}
