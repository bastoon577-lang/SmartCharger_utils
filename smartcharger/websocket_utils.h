#ifndef __WEBSOCKET_UTILS__
#define __WEBSOCKET_UTILS__

#include "common_sm.h"

/*
 * Le module WebSocket permet l'échange de données en mode évênementiel des différentes
 * données entre les équippements.
 * Construit autour de 2 services WebSocket Serveur et Client, chacun d'entre eux permet: 
 *    - Serveur     permet un affichage dynamique des données avec l'interface Web du SmartCharger
 *    - Client      permet une communication avec le Module TIC permettant d'extraire les données de consommation Linky
 * 
 * PS :
 *    - La WS Server est forcément instanciée
 *    - La WS Client est facultative (dépendant de la configuration de l'équippement)
 */

//< Defines des TIMEOUT
#define TIMEOUT_RETRY_CONNECT   30000	// Timer de tentative de reconnexion
#define TIMEOUT_DATA_RECV     	20000	// Timer d'absence de donnée
#define TIMEOUT_CONNEXION   	2000	// Timer de tentative de connexion
 
//< Enumération des Timers WebSockets Client
enum {
  ws_client_retry_connect,
  ws_client_data_received,
  ws_client_connexion,
  ws_client_nb_timers
};

//< Enumération des états de la WebSockets Client
typedef enum {
  ws_client_idle,
  ws_client_connect,
  ws_client_connecting,
  ws_client_connected,
  ws_client_disconnect
} WS_CLIENT_STATE_e;

//< Structure des données WebSocket
typedef struct {
  unsigned long timers_client[ws_client_nb_timers];			  // Timers de fonctionnement internes
  CHARGE_PARAMETERS_t *charge_parameters;					  // Pointeur vers les paramètres de charge VE
  STATIC_CONF_FIELDS_t *static_conf;        				  // Pointeur vers la structure STATIC_CONF_FIELDS_t
  WS_CLIENT_STATE_e client_state;							  // Etat de connexion de la WS client
  TIC_CONF_FIELDS_t *tic;									  // Pointeur vers les paramètres de configuration TIC
  TIC_DATA_t *tic_data;										  // Pointeur vers la structure TIC_DATA
  uint8_t is_client_active			: 1;					  // Bitfield de client actif
  uint8_t is_client_socket_active	: 1;					  // Bitfield de demande de connexion client
  uint8_t RUF						: 6;					  // Réservé usage future
} WS_t;

/**
 * \fn void ws_server_init(uint16_t port)
 * \brief Fonction permettant d'initialiser le service WebSocket Server
 *        Ce service permet le raffraichissement des données sur l'interface WEB
 *        du SmartCharger et doit donc être initialisé tout le temps
 * 
 * \param in, le port de service WS à utiliser
 */
void ws_server_init(uint16_t port);

/**
 * \fn void ws_client_init(TIC_CONF_FIELDS_t *tic_conf)
 * \brief Fonction permettant d'initialiser le service WebSocket Client
 *        Ce service permet la lecture des données TIC au travers du service 
 *        WenSocket offert par un Module TIC sur le réseau.
 *        Facultatif en fonction de la configuation de l'utilisateur
 * 
 * \param in, le pointeur vers la structure de configuration TIC
 * \param in, le pointeur vers la structure de configuration SmartCharger
 */
void ws_client_init(TIC_CONF_FIELDS_t *tic_conf, STATIC_CONF_FIELDS_t *static_conf);

/**
 * \fn void ws_client_connect_on_tic_module(void)
 * \brief Fonction permettant d'établir une connexion WS avec le Module TIC
 * 
 * \param in, le pointeur vers la structure de données TIC_DATA
 */
void ws_client_connect_on_tic_module(TIC_DATA_t *tic);

/**
 * \fn void ws_client_disconnect_from_tic_module(void)
 * \brief Fonction permettant de déconnecter la WS avec le Module TIC
 */
void ws_client_disconnect_from_tic_module(void);

/**
 * \fn bool ws_client_is_connected(void)
 * \brief Fonction permettant de vérifier l'état de connexion avec le Module TIC
 * \return
 *    true  -> Connecté au Module TIC
 *    false -> Déconnecté du Module TIC
 */
bool ws_client_is_connected(void);

/**
 * \fn void ws_server_set_charge_parameters(CHARGE_PARAMETERS_t *data)
 * \brief Fonction permettant de renseigner le pointeur de 
 *        la structure CHARGE_PARAMETERS_t
 * \param in, le pointeur vers la structure CHARGE_PARAMETERS_t
 */
void ws_server_set_charge_parameters(CHARGE_PARAMETERS_t *data);

/**
 * \fn void ws_server_send_broadcast(void)
 * \brief Fonction permettant d'émettre en broadcast les informations
 *        aux clients WS connectés
 */
void ws_server_send_broadcast(void);

/**
 * \fn void ws_handler(void)
 * \brief Handler du module WebSocket Server & Client à appeler régulièrement
 */
void ws_handler(void);

#endif
