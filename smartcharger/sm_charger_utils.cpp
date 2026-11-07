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
 * \brief Fonction permettant l'initialisation de l'état et du courant initial
 */
static inline void sm_charger_init_state(void) {
  charge.parameters.state   = charge_state_not_Connected;     // Positionnement de l'état en Attente de VE
  charge.parameters.current = MINIMAL_CHARGE_CURRENT;         // Positionnement du courant initial
}

/**
 * \fn bool sm_charger_get_off_peak_hours(void)
 * \brief Fonction permettant de retourner l'état d'activation d'heure creuse.
 * \return
 *		1 - Une ou plusieurs option(s) Heure(s) Creuse(s) est active
 *		0 - Aucune option
 */
static inline bool sm_charger_get_off_peak_hours(void) {
  return ((*(uint16_t*)&charge.volatile_conf->off_peak_hours) != 0);
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
  bool network_power = false, solar_power = false;
  enum {phase_1, phase_2, phase_3, nb_phases};
  int8_t available_current[nb_phases] = {0};
  int8_t ret = MINIMAL_CHARGE_CURRENT;
  
  if(!ws_client_is_connected())
    return ret;
  
  /* Attribution du type de charge selon les énergies :
   * 	- Solaire,
   *	- Du réseau de distribution.
   */
  if(charge.flag_force_quick_charge) {						  // Le mode charge rapide est demandé
    network_power = true;							
  } else if (charge.is_solar_enable &&						  // La charge solaire est en cours...
		     !charge.is_hc_enable) {						  // La charge HC n'est pas en cours...
	solar_power = true;
  } else if (!charge.is_solar_enable &&						  // La charge solaire n'est pas en cours... 
			 charge.is_hc_enable) {							  // La charge HC est en cours...
	network_power = true; 
  } else if (charge.is_solar_enable &&						  // La charge solaire est en cours... 
			 charge.is_hc_enable) {							  // La charge HC est en cours...
	// Il y a chevauchement de charge solaire et de charge HC
	if(charge.volatile_conf->prio_hc_on_solar)				  // L'option de priorisation HC est active
	  network_power = true;
	else 
	  solar_power = true;
  } else													  // Autrement
    network_power = true;	
  
  /* La charge est oppérée par l'énergie solaire :
   * 	On raisonne en terme de courant disponible "Pouvant être absorbé par le VE".
   * 	On cherche ici à minimiser le courant injecté sur le réseau et le courant absorbé sur le réseau.
   */
  if(solar_power) {
    if(!charge.static_conf->which_voltage) {				  // Le réseau est de type Monophasé
	  /* Il y a présence de puissance injectée, on retourne le courant injecté (P/U) (Pour augmenter la puissance du VE).
       * Autrement, on retourne l'opposé du courant absorbé (Pour diminuer la puissance du VE).
	   */
	  ret = (tic_data.p_injectee)?
			  tic_data.p_injectee/VOLTAGE_1PH:-(tic_data.i_inst[phase_1]);
	} else {												  // Autrement, le réseau est de type Triphasé
	  /* Il y a présence de puissance injectée, on retourne le courant injecté (P/U) (Pour augmenter la puissance du VE).
       * Autrement, on retourne l'opposé du courant maximal absorbé (Pour diminuer la puissance du VE).
	   */
	  if(tic_data.p_injectee)
		ret = tic_data.p_injectee/(VOLTAGE_3PH*ROOT_3);
	  else {
		// Recherche du courant maximum absorbé sur chaque phases du réseau
		ret = tic_data.i_inst[phase_1];
		for(uint8_t p=0;p<nb_phases;p++) {
		  if(tic_data.i_inst[p] > ret)
            ret = tic_data.i_inst[p];
		}
		ret = -(ret);										  // On retourne l'opposé
	  }
	}
  }
  
  // La charge est oppérée par l'énergie du réseau de distribution
  if(network_power) {
    if(!charge.static_conf->which_voltage) {                  // Le réseau est de type Monophasé
      ret = tic_data.i_max - tic_data.i_inst[phase_1];
    } else {                                                  // Autrement, le réseau est de type Triphasé
      for(uint8_t p=0;p<nb_phases;p++)                        // Calculs des courants disponibles sur chaque phases
        available_current[p] = tic_data.i_max - tic_data.i_inst[p];
    
      /// Recherche du courant minimum disponible sur chaque phases du réseau
      ret = available_current[phase_1];
      for(uint8_t p=0;p<nb_phases;p++) {
        if(available_current[p] < ret)
          ret = available_current[p];
      }
    }
  }
  
  // Retour du résultat
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
 *        par une priorité vis à vis des diminutions de courant face aux augmentations.
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
  
  // Néanmoins, il ne faut pas dépasser la limite imposée, sauf lorsqu'une recharge rapide est demandée
  if(charge.parameters.current > sm_charger_read_limit_current() &&
	 !charge.flag_force_quick_charge)
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
  static unsigned long last_connection_attempt = 0;
  static bool prev_need_connection = false;

  bool need_connection =                                      // La connexion WS Module TIC est nécessaire lorsque :
    charge.volatile_conf->solar_active ||                     // La charge solaire est active
    sm_charger_get_off_peak_hours()    ||                     // Une option d'Heures Creuses est active
    charge.is_charge_active;                                  // La charge est en cours...

  if(need_connection != prev_need_connection) {				  // Détection de changement d'état (front)
    prev_need_connection = need_connection;

    if(need_connection) {
      last_connection_attempt = millis();
      ws_client_connect_on_tic_module(&tic_data);         	  // Demande de connexion
      charge.flag_scrut_evse = 0;                         	  // Lecture de l'EVSE non autorisée
    } else {
      if(ws_client_is_connected()) {
        ws_client_disconnect_from_tic_module();         	  // Demande de déconnexion propre
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

  // Pendant la phase de démarrage, attente sans déclenchement du mode dégradé
  if(charge.counter_starting_charge > 1)
    return;

  // Juste au moment critique (compteur == 1) et si le client n'est toujours pas là
  if(charge.counter_starting_charge == 1 && !ws_client_is_connected()) {
    charge.parameters.current = 						      // Attribution du courant de charge dégradé
					sm_charger_read_degraded_current();
    charge.parameters.state = 								  // Attribution de l'état IHM
					charge_state_charging_degraded;
    charge.is_limited_charge = 1;
	return;													  // Echappement pour ce cycle
  }

  // Surveillance continue de la connexion WebSocket en cours de route
  if(!ws_client_is_connected()) {
    charge.is_limited_charge = 1;
  } else if(charge.is_limited_charge && ws_client_is_connected()) {
    charge.parameters.state  = charge_state_charging;
    charge.is_limited_charge = 0;
  }

  // Application finale de la limitation de courant si le mode dégradé est actif
  if(!charge.is_limited_charge)
    return;

  charge.parameters.state = charge_state_charging_degraded;
  uint8_t limit = sm_charger_read_degraded_current();
  if(charge.parameters.current > limit) {
    charge.parameters.current = limit;
  }
  // Autrement le courant reste inchangé (déjà limité par l'asservissement précédent !)
}

/**
 * \fn void sm_charger_update_hc_state(void)
 * \brief Fonction permettant la gestion de charge en Heures Creuses
 *        au travers de la connexion WS au Module TIC
 */
static void sm_charger_update_hc_state(void) {  
  /* Par défaut, on inhibe la charge possible en HC, puis on la récative !
   * La logique de contrôle ici est de vérifier:
   *	- Si l'option est active
   *	- Si la valeur de Tarif correspond
   * Pour permettre d'activer la charge
   */
  charge.is_hc_enable = 0;
  
  /* Cas d'une charge en Heures Creuses Standards :
   *    - Mode Standard peut être : 'HEURE CREUSE',
   *    - Mode Historique peut être : 'HC..'.
   */
  if(charge.volatile_conf->off_peak_hours.off_standards) {
	if(!memcmp(tic_data.tarif,"HEURE CREUSE",12) ||
	   !memcmp(tic_data.tarif,"HC..",4))
	  charge.is_hc_enable = 1;
  }
  
  /* Cas d'une charge en Heures Creuses Bleues :
   *    - Mode Standard peut être : 'HC BLEU',
   *    - Mode Historique peut être : 'HCJB'.
   */
  if(charge.volatile_conf->off_peak_hours.off_blues) {
	if(!memcmp(tic_data.tarif,"HC BLEU",7) ||
	   !memcmp(tic_data.tarif,"HCJB",4)) {
	  charge.is_hc_enable = 1;
	}
  }
  
  /* Cas d'une charge en Heures Creuses Blancs :
   *    - Mode Standard peut être : 'HC BLANC',
   *    - Mode Historique peut être : 'HCJW'.
   */
  if(charge.volatile_conf->off_peak_hours.off_whites) {
	if(!memcmp(tic_data.tarif,"HC BLANC",8) ||
	   !memcmp(tic_data.tarif,"HCJW",4))
	  charge.is_hc_enable = 1;
  }
  
  /* Cas d'une charge en Heures Creuses Rouges :
   *    - Mode Standard peut être : 'HC ROUGE',
   *    - Mode Historique peut être : 'HCJR'.
   */
  if(charge.volatile_conf->off_peak_hours.off_reds) {
	if(!memcmp(tic_data.tarif,"HC ROUGE",8) ||
	   !memcmp(tic_data.tarif,"HCJR",4))
	  charge.is_hc_enable = 1;
  }
    
  /* Cas d'une charge en Heures Super Creuses :
   *    - Mode Standard peut être : 'HEURE SUPER CREUSE',
   *    - Mode Historique n'implémente rien.
   */
  if(charge.volatile_conf->off_peak_hours.off_super) {
	if(!memcmp(tic_data.tarif,"HEURE SUPER CREUSE",18))
      charge.is_hc_enable = 1;
  }
  
  /* Cas d'une charge en Heures Weekends :
   *    - Mode Standard peut être : 'HEURE WEEK-END',
   *    - Mode Historique n'implémente rien.
   */
  if(charge.volatile_conf->off_peak_hours.off_weekends) {
	if(!memcmp(tic_data.tarif,"HEURE WEEK-END",14))
      charge.is_hc_enable = 1;
  }
  
  /* Cas d'une charge en Heures Mercredis :
   *    - Mode Standard peut être : 'HP MERCREDI',
   *    - Mode Historique n'implémente rien.
   */
  if(charge.volatile_conf->off_peak_hours.off_wednesday) {
	if(!memcmp(tic_data.tarif,"HC MERCREDI",11))
      charge.is_hc_enable = 1;
  }
}

/**
 * \fn void sm_charger_update_solar_state(void)
 * \brief Fonction permettant la gestion de charge solaire
 *        au travers de la connexion WS au Module TIC
 */
static void sm_charger_update_solar_state(void) {

  charge.is_solar_enable = 0;								  // Re-initialisation
  
  if(!charge.volatile_conf->solar_active)					  // L'option solaire n'est pas active
	return;													  // Echappement immédiat
	  
  const char* now = tic_data.date + 7;			  			  // Extraction de l'heure courante du Linky
  int cmp_start = memcmp(now,								  // Comparaison de l'heure de début de charge solaire
					charge.volatile_conf->solar_start_hour,4);
  int cmp_end = memcmp(now, 					  			  // Comparaison de l'heure de fin de charge solaire
					charge.volatile_conf->solar_end_hour,4);

  if(cmp_start >= 0 && cmp_end < 0) 						  // L'heure courante est dans la plage horaire
	charge.is_solar_enable = 1;	
}

/**
 * \fn void sm_charger_process_tic_data(void)
 * \brief Fonction permettant la gestion du process de charge en utilisant les données du Module TIC
 */
static void sm_charger_process_tic_data(void) {
  
  sm_charger_update_solar_state();							  // Vérification de la Charge Solaire
  sm_charger_update_hc_state();                               // Vérification des Heurese Creuse

  int8_t available_current = 
					sm_charger_compute_available_current();   // Calcul du courant disponible sur le réseau électrique
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
  
  bool restricted_charge = charge.volatile_conf->solar_active || // L'option de charge solaire est active
						   sm_charger_get_off_peak_hours();	  // L'option Heures Creuses est active

  bool is_enable_charge = charge.flag_force_quick_charge ||	  // La charge rapide est demandée
						  charge.is_solar_enable         ||	  // La charge solaire est en cours...
						  charge.is_hc_enable;   			  // Les heures creuses sont en cours...

  // Une restriction de charge est active (Attente Heures Creuses / Solaire)
  if(restricted_charge) {	  
    if(is_enable_charge) {									  // Néanmoins, la charge est possible (HC ou Solaire en cours...)
	  if(charge.parameters.state == charge_state_wait)		  // Nous étions en attente
	    charge.parameters.state = charge_state_not_Connected; // On recalle en mode VE non connecté
	} else {												  // Autrement
	  charge.parameters.state = 							  // On attend !!!
			(ws_client_is_connected())?charge_state_wait:charge_state_wait_default;
    
	  charge.flag_scrut_evse = 0; 							  // Blocage de la lecture de l'EVSE
      charge.flag_lock_evse = 1;  							  // Blocage de l'EVSE (Charge impossible)
      return;                     							  // Sortie anticipée
	}
  } else {													  // Autrement, il n'y a pas de restriction
	if(charge.parameters.state == charge_state_wait_default ||// Mais l'IHM est en attente "défaut"
	   charge.parameters.state == charge_state_wait)		  // ou en attente normal
	  charge.parameters.state = charge_state_not_Connected;   // On recalle en mode VE non connecté
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
  hal_evse_init();
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
    charge.counter_starting_charge--;                         // On décrémente (tendre vers 0)
  
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
		  if(!charge.counter_starting_charge)
		    charge.flag_force_quick_charge = 0;				  // Interrompre le forcage de recharge rapide
          break;
        case evse_Com_Fault:
        default:
          charge.parameters.state = charge_state_default_et3k;
          break;
      }
    }
  }

  // Mise à jour du forçage de recharge rapide
  charge.parameters.force = charge.flag_force_quick_charge;

  // Mise à jour de l'IHM par WebSocket (uniquement si un changement à eu lieu)
  if(memcmp(&previous_charge_parameters,&charge.parameters,sizeof(CHARGE_PARAMETERS_t)) != 0) {
    memcpy(&previous_charge_parameters,&charge.parameters,sizeof(CHARGE_PARAMETERS_t));
	// Calcul de la puissance VE
	if(!charge.static_conf->which_voltage)					  // En monophasé
	  charge.parameters.power = COMPUTE_1PH_POWER(charge.parameters.current);
    else													  // En triphasés
	  charge.parameters.power = COMPUTE_3PH_POWER(charge.parameters.current);

    ws_server_send_broadcast(); 
  }
}

/**
 * \fn void sm_charger_set_force_charge(uint8_t value)
 */
void sm_charger_set_force_charge(uint8_t value) {
    charge.flag_force_quick_charge = value;
}

/**
 * \fn uint8_t sm_charger_get_force_charge(void)
 */
uint8_t sm_charger_get_force_charge(void) {
    return charge.flag_force_quick_charge;
}

/**
 * \fn uint8_t sm_charger_prevent_updates(void)
 */
uint8_t sm_charger_prevent_updates(void) {
    return charge.flag_prevent_updates;
}