#include "sm_charger_utils.h"
#include "websocket_utils.h"
#include "common_sm.h"

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
  charge.parameters.current = MINIMAL_CHARGE_CURRENT;         // Positionnement du courant initial
}

/**
 * \fn void void sm_charger_read_limit_current(void)
 * \brief Fonction permettant de lire le courant limite
 * \return Le courant limite lut dans l'Eeprom
 */
static inline uint8_t sm_charger_read_limit_current(void) {
  uint8_t current;
  hal_disable_interrupt();                                    // Arrêt des interruptions
  current = READ_OFFSET_5B(charge.volatile_conf->limite_current);
  hal_enable_interrupt();                                     // Reprise des interruptions
  return current;
}

/**
 * \fn void void sm_charger_read_degraded_current(void)
 * \brief Fonction permettant de lire le courant dégradé
 * \return Le courant dégradé lut dans l'Eeprom
 */
static inline uint8_t sm_charger_read_degraded_current(void) {
  uint8_t current;
  hal_disable_interrupt();                                    // Arrêt des interruptions
  current = READ_OFFSET_5B(charge.volatile_conf->degraded_current);
  hal_enable_interrupt();                                     // Reprise des interruptions
  return current;
}

/**
 * \fn void int8_t sm_charger_compute_available_current(void)
 * \brief Fonction permettant de calculer le courant disponible sur le 
 *        réseau de distribution en Monophasé ou en Triphasé.
 * \return
 *    Le courant disponible en Ampères
 */
static int8_t sm_charger_compute_available_current(void) {

  enum {phase_1, phase_2, phase_3, nb_phases};
  int8_t available_current[nb_phases] = {0};
  int8_t ret;
  
  /* L'option de privilège solaire est active
   * Si aucune absorption n'est constaté sur la Phase 1, on retourne le courant injecté (P_Injecté/200)
   * Autrement, le courant absorbé provient de la Phase 1, on diminue de la valeur du courant absorbée sur la Phase 1
   */
  if(charge.volatile_conf->solar_active)
	return (!tic_data.i_inst[phase_1])?
			tic_data.p_injectee/200:-(tic_data.i_inst[phase_1]);
  
  if(!charge.static_conf->which_voltage) {                    // Le réseau est de type Monophasé
    ret = tic_data.i_max - tic_data.i_inst[phase_1];
  } else {                                                    // Autrement, le réseau est de type Triphasé
    for(uint8_t p=0;p<nb_phases;p++)                          // Calculs des courants disponibles sur chaque phases
      available_current[p] = tic_data.i_max - tic_data.i_inst[p];
    
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

  if(!charge.is_charge_active)
    return;
    
  if(available_current < -(BIG_GAP_GRID_CURRENT)) {           // Une surcharge du réseau trop importante est constatée !
    charge.parameters.current = MINIMAL_CHARGE_CURRENT;       // Diminution drastique du courant de charge
  } else if (available_current < NULL_GAP_GRID_CURRENT) {     // Une petite surcharge du réseau est constatée
    if(charge.parameters.current > MINIMAL_CHARGE_CURRENT)    // Le courant de charge peut-il être diminué ?
      sm_charger_waiting_before_decrease(1);                  // Diminution de 1A du courant de charge
    else {
      /////////////////////////////////////////////////////////////////////////////////////////////////////////
      // AUTREMENT, il est nécessaire de couper la charge !!!
      // Continuer d'observer les données TIC en attente que courant disponible soit > 6 pour repasser en charge !
      // NON IMPLEMENTEE POUR LE MOMENT !!!
      /////////////////////////////////////////////////////////////////////////////////////////////////////////
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
  
  // Néanmoins, il ne faut pas dépasser la limite imposée !
  if(charge.parameters.current > sm_charger_read_limit_current())
    charge.parameters.current = sm_charger_read_limit_current();
  
  // Mais, il est important de ne pas excéder la puissance maximale du réseau !
  if(charge.parameters.current > tic_data.i_max) {
    if(tic_data.i_max)                              // Uniquement s'il possède une valeur (Ne peut être égal à 0) !
      charge.parameters.current = tic_data.i_max;
  }
  // Mais aussi, ne pas excéder la puissance maximale admissible par le VE !
  if(charge.parameters.current > MAXIMAL_CHARGE_CURRENT)
    charge.parameters.current = MAXIMAL_CHARGE_CURRENT;
}

/**
 * \fn void sm_charger_manage_tic_connection(void)
 * \brief Fonction permettant la gestion de connexion de la WS au Module TIC
 */
static void sm_charger_manage_tic_connection(void) {
  static bool prev_need_connection = false;
  
  bool need_connection =                                      // La connexion WS Module TIC est nécessaire lorsque :
    charge.volatile_conf->off_super_peak_hours ||			  // L'option d'Heures Super Creuses est active
    charge.volatile_conf->off_peak_hours ||                   // L'option d'Heures Creuses est active
    charge.is_charge_active;                                  // La charge est en cours...
  
  // Détection de changement d'état (front) pour éviter la réinstanciation en boucle de la connexion
  if(need_connection != prev_need_connection) {
    prev_need_connection = need_connection;
    
    if (need_connection) {
      ws_client_connect_on_tic_module(&tic_data);             // Demande de connexion
      charge.flag_scrut_evse = 0;                             // Lecture de l'EVSE non autorisée tant qu'on cherche la synchro TIC
    } else {
      if (ws_client_is_connected()) {
        ws_client_disconnect_from_tic_module();               // Demande de déconnexion propre à la machine à états de la socket
      }
    }
  }
}

/**
 * \fn void sm_charger_manage_degraded_mode(void)
 * \brief Fonction permettant la gestion du mode dégradé lors de la perte de connexion 
 *        avec le Module TIC sur le réseau.
 */
static void sm_charger_manage_degraded_mode(void) {

  if(!charge.is_charge_active)
    return;

  if(charge.counter_starting_charge == 1 &&					  // Avant dernier incrément de counter_starting_charge
     !ws_client_is_connected()) {			                  // Le client n'est toujours pas connecté  
    charge.parameters.current =                               // Adoption du courant limite configuré
           sm_charger_read_degraded_current();
    charge.parameters.state = charge_state_charging_degraded;
    charge.is_limited_charge = 1;
  }
  else if (charge.counter_starting_charge > 1)                // Le compteur est encore trop élevé
    return;                                                   // Echappement immédiat
    
  if(!ws_client_is_connected()) {                             // La socket signale une perte de connexion
    charge.parameters.state = charge_state_charging_degraded;
    charge.is_limited_charge = 1;
  } else if(charge.is_limited_charge &&                       // Le mode dégradé est actif
            ws_client_is_connected()) {                       // La connexion est de nouveau effective
    charge.parameters.state = charge_state_charging;
    charge.is_limited_charge = 0;
  }
  
  if(!charge.is_limited_charge)
    return;

  uint8_t limit = sm_charger_read_degraded_current();
  if(charge.parameters.current > limit)                   
    charge.parameters.current = limit;                        // Adoption du courant limite configuré
  // Autrement le courant reste inchangé (déjà limité par l'asservissement précédent !)
}

/**
 * \fn void sm_charger_update_hc_state(void)
 * \brief Fonction permettant la gestion de charge en Heures Creuses ou Heures Super Creuse
 *        au travers de la connexion WS au Module TIC
 * \warning
 *		L'option de charge en Heures Super Creuses l'emporte; autrement dit, si l'option Heure Super Creuses est
 *		active, alors le déblocage ne se fera qu'en Heures Super Creuses.
 *		L'option Heures Creuses inclue quant à elle l'Heures Super Creuses.
 */
static void sm_charger_update_hc_state(void) {
    
  if(!ws_client_is_connected())                               // Pas de connexion effective
    return;                                                   // Echappement immédiat

  charge.is_hc_active = 0;                                    // Désautorisation de charge dans tous les cas

  if(!charge.volatile_conf->off_super_peak_hours &&           // Aucune configuration de charge en heures creuses ?
     !charge.volatile_conf->off_peak_hours)
    return;                                                   // Echappement immédiat
    
  /* Dans le cas d'une charge en Heures Creuses, il est nécessaire de prendre tous les cas :
   *    - Mode Standard peut être : 'HEURE CREUSE' , 'HC BLEU' , 'HC BLANC' , 'HC ROUGE' , 'HEURE SUPER CREUSE'.
   *    - Mode Historique peut être : 'HC..' , 'HCJB' , 'HCJW' , 'HCJR'.
   * La stratégie est d'utiliser le début de chaine 'HC' dans les 2 modes, et de prendre la chaine 
   * complète sur le mode standard.
   */
  if(charge.volatile_conf->off_peak_hours) {
	if(!memcmp(tic_data.tarif,"HEURE SUPER CREUSE",18) ||
       !memcmp(tic_data.tarif,"HEURE CREUSE",12) ||
       !memcmp(tic_data.tarif,"HC",2))
      charge.is_hc_active = 1;                                // Autorisation de charge
  }
  
  /* Mais dans le cas d'une charge en Heures Super Creuse, il est nécessaire de prendre uniquement
   * le cas de 
   * 	- Mode Standard doit être : 'HEURE SUPER CREUSE'.
   *    - Mode Historique, rien !
   */
  if(charge.volatile_conf->off_super_peak_hours) {
    if(!memcmp(tic_data.tarif,"HEURE SUPER CREUSE",18))
	  charge.is_hc_active = 1;                                // Autorisation de charge
    else													  // Désautorisation de charge précédente
	  charge.is_hc_active = 0;                                // Heures Super Creuse étant prioritaires
  }
}

/**
 * \fn void sm_charger_process_tic_data(void)
 * \brief Fonction permettant la gestion du process de charge en utilisant les données du Module TIC
 */
static void sm_charger_process_tic_data(void) {
  
  sm_charger_update_hc_state();                               // Vérification des données TIC

  int8_t available_current = 
    sm_charger_compute_available_current();                   // Calcul du courant disponible sur le réseau électrique
  sm_charger_adjust_current(available_current);               // Ajustement du courant de charge VE
}

/**
 * \fn void sm_charger_charge_with_tic_module(void)
 * \brief Fonction permettant la gestion de la charge avec les données issues du Module TIC au travers du réseau. 
 *        Cette méthode permet un asservissement dynamique de la puissance de charge du VE en fonction 
 *        de la puissance instantanée du réseau électrique.
 */
static void sm_charger_charge_with_tic_module(void) {

  // Gestion non-bloquante de la demande de connexion/déconnexion avec le Module TIC
  sm_charger_manage_tic_connection();

  sm_charger_process_tic_data();                              // Gestion du processus de charge avec les données TIC

  charge.flag_lock_evse = 0;                                  // Déblocage de l'EVSE, celui-ci sera bloqué après si nécessaire

  if(charge.volatile_conf->off_super_peak_hours ||   		  // La configuration Heures Super Creuses est active ou
	 charge.volatile_conf->off_peak_hours) {				  // La configuration Heures Creuses est active
    if(!charge.is_hc_active) {                                // Les Heures Creuses ne sont pas en cours
      if(ws_client_is_connected())
        charge.parameters.state = charge_state_wait_hc;       // Positionnement de l'état en Attente HC
      else
        charge.parameters.state = charge_state_wait_hc_default;
      charge.flag_scrut_evse = 0;                             // Lecture de l'EVSE non autorisée
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

  sm_charger_manage_degraded_mode();                          // Gestion du mode dégradé basée sur l'état de la socket
}

/**
 * \fn void sm_charger_charge_without_tic_module(void)
 * \brief Fonction permettant la gestion de la charge sans Module TIC
 *        La puissance de charge est donc statique et déterminée par l'utilisateur.
 */
static void sm_charger_charge_without_tic_module(void) {
  charge.parameters.current = sm_charger_read_limit_current();// Adoption du courant limite
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
  static CHARGE_STATE_EVSE_e previous_evse_state = evse_Not_Connected;
  static CHARGE_PARAMETERS_t previous_charge_parameters;
  static unsigned long timer_scrut_evse = millis();

  /*************************************************************************************************************
   *        Etage de contrôle du SmartCharger; cet étage permet le gestion intelligente du
   *        mécanisme de recharge VE avec ou sans Module TIC.
   ************************************************************************************************************/
  charge.flag_scrut_evse = 1;                                 // Autorisation de lecture de l'EVSE qui sera désautorisé si nécessaire
  if(!charge.static_conf->is_tic_module_used)                 // Le Module TIC n'est pas configuré ?
    sm_charger_charge_without_tic_module();                   // Utilisation du mécanisme de charge sans le Module TIC
  else                                                        // Autrement
    sm_charger_charge_with_tic_module();                      // Utilisation du mécanisme de charge avec le Module TIC

  /*************************************************************************************************************
   *        Etage de contrôle EVSE et d'update WS (période 1s).
   ************************************************************************************************************/
  hal_evse_update_input();                                    // Scrutation de l'EVSE (Mode Polling)
  
  if(millis() - timer_scrut_evse < TIMEOUT_SCRUT_EVSE)        // Le timer est en cours...
    return;                                                   // Echappement immédiat
  timer_scrut_evse = millis();                                // Réarmement du timer
  
  if(charge.counter_starting_charge)                          // Le compteur est décrémentable
    charge.counter_starting_charge--;                         // On décrémente (tendre à 0)
  
  // Communication avec l'EVSE (Courant de consigne et/ou blocage)
  hal_evse_update_output((charge.flag_lock_evse)?0:charge.parameters.current); 
  
  // Lecture de l'état de charge VE par l'EVSE
  if(charge.flag_scrut_evse) {                                // La lecture de l'état de l'EVSE est autorisée
    CHARGE_STATE_EVSE_e current_evse_state = hal_evse_get_state();
    if(current_evse_state != previous_evse_state) {           // L'état de l'EVSE à changé ?
      previous_evse_state = current_evse_state;
      charge.flag_prevent_updates = 0;                        // Autorisation de mise à jour firmware par défaut
      charge.is_charge_active = 0;                            // Désactivation de charge par défaut
      switch(current_evse_state) {
        case evse_Connected:
          charge.parameters.current = MINIMAL_CHARGE_CURRENT;
          charge.parameters.state = charge_state_connected;
          charge.flag_prevent_updates = 1;                    // Désautorise la mise à jour firmware
          break;
        case evse_Charging:
          charge.parameters.state = charge_state_charging;
          charge.counter_starting_charge = 10;                // Initialisation du compteur de début de charge
          charge.flag_prevent_updates = 1;                    // Désautorise la mise à jour firmware    
          charge.is_charge_active = 1;                        // Activation de charge
          break;
        case evse_Fault:
          charge.parameters.current = MINIMAL_CHARGE_CURRENT;
          charge.parameters.state = charge_state_default;
          break;
        case evse_Not_Connected:
          charge.parameters.state = charge_state_not_Connected;
          charge.parameters.current = MINIMAL_CHARGE_CURRENT;
          break;
        case evse_Com_Fault:
        default:
          charge.parameters.state = charge_state_default_et3k;
          break;
      }
    }
  }

  // Mise à jour de l'IHM par WebSocket (uniquement si un changement à eu lieu)
  if(memcmp(&previous_charge_parameters,&charge.parameters,sizeof(CHARGE_PARAMETERS_t)) != 0) {
    memcpy(&previous_charge_parameters,&charge.parameters,sizeof(CHARGE_PARAMETERS_t));
    ws_server_send_broadcast(); 
  }
}

/**
 * \fn uint8_t sm_charger_prevent_updates(void)
 */
uint8_t sm_charger_prevent_updates(void) {
    return charge.flag_prevent_updates;
}