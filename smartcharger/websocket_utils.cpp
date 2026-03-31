#include <WebSocketsServer_Generic.h>
#include <WebSocketsClient_Generic.h>

#include "websocket_utils.h"

//< Déclaration des variables globales
static WebSocketsServer* ws_server_socket;
WebSocketsClient ws_client_socket;
static WS_t ws;

/**
 * \fn void void ws_client_tic_parser(uint8_t *payload)
 * \brief Fonction permettant de remplir la structure TIC_DATA_t a partir des données 
 *        reçues par le Module TIC au travers du service WS Client
 * 
 * \param in, la chaine de carractères reçues sur la WS issues du Module TIC sur le réseau
 */
static void ws_client_tic_parser(uint8_t *payload) {
  char *str = (char *)payload;
  char value[16];

  /*
   * Extraction et mise à jour des données uniquement intéressantes ; à savoir:
   *  - ISOUSC    le courant maximal pouvant être atteint avant que le Linky disjoncte  (Tous les cas)
   *  - IINST     le courant instantané absorbé par l'installation                      (Cas du monophasé)
   *  - IINST1    le courant instantané phase 1 absorbé par l'installation              (Cas du triphasé)
   *  - IINST2    le courant instantané phase 2 absorbé par l'installation              (Cas du triphasé)
   *  - IINST3    le courant instantané phase 3 absorbé par l'installation              (Cas du triphasé)
   *  - PAPP      la puissance instantanée absorbée                                     (Tous les cas)
   *  - PTEC      l'heure courante (Heure creuse ou pleine)                             (Tous les cas)
   *  - ...       A suivre, si besoin
   */
  if(strstr(str,"\"ISOUSC\"")) {
    if(sscanf(str,"{\"ISOUSC\":\"%15[^\"]\"}",value) == 1)
      ws.tic_data->isousc = (uint8_t)atoi(value);
  } else if(strstr(str,"\"IINST\"")) {
    if(sscanf(str,"{\"IINST\":\"%15[^\"]\"}",value) == 1)
      ws.tic_data->iinst = (uint8_t)atoi(value);
  } else if(strstr(str,"\"IINST1\"")) {
    if(sscanf(str,"{\"IINST1\":\"%15[^\"]\"}",value) == 1)
      ws.tic_data->iinst_1 = (uint8_t)atoi(value);
  } else if(strstr(str,"\"IINST2\"")) {
    if(sscanf(str,"{\"IINST2\":\"%15[^\"]\"}",value) == 1)
      ws.tic_data->iinst_2 = (uint8_t)atoi(value);
  } else if(strstr(str,"\"IINST3\"")) {
    if(sscanf(str,"{\"IINST3\":\"%15[^\"]\"}",value) == 1)
      ws.tic_data->iinst_3 = (uint8_t)atoi(value);
  } else if(strstr(str,"\"PAPP\"")) {
    if (sscanf(str,"{\"PAPP\":\"%15[^\"]\"}",value) == 1)
      ws.tic_data->papp = (uint16_t)atoi(value);
  } else if(strstr(str,"\"PTEC\"")) {
    if(sscanf(str, "{\"PTEC\":\"%4[^\"]\"}",value) == 1)
      strcpy(ws.tic_data->ptec, value);
  }
}

/**
 * \fn void ws_client_on_event(WStype_t type, uint8_t * payload, size_t length)
 * \brief Fonction permettant des gerer les évênements lies aux WS Client (Module TIC -> SmartCharger)
 * 
 * \param in, le type d'évènement reçu en WS
 * \param in, la chaine de caractères reçue sur la WS
 * \param in, la longeur de la chaine reçue sur la WS
 */
static void ws_client_on_event(WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_TEXT:
      ws.timers_client[ws_client_data_received] = millis();           // Réarmement du timer de réception de données
      ws_client_tic_parser(payload);                                  // Parsage et mise a jour de la structure TIC
      break;
    case WStype_DISCONNECTED:
    case WStype_CONNECTED:
    default:
      break;
  }
}

/**
 * \fn void ws_server_on_event(uint8_t num, WStype_t type, uint8_t* payload, size_t length)
 * \brief Fonction permettant des gerer les évênements lies aux WS Serveur
 * 
 * \param in, le numéro de l'évènement reçu en WS
 * \param in, le type d'évènement reçu en WS
 * \param in, la chaine de caractères reçue sur la WS
 * \param in, la longeur de la chaine reçue sur la WS
 */
static void ws_server_on_event(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  char data_to_send[64];
  switch (type) {
    case WStype_CONNECTED:                                            // Un nouveau client s'est connecté !
      snprintf(data_to_send, sizeof(data_to_send),                    // Stringification des données
         "{\"State\":\"%d\",\"Current\":\"%d\"}",
         ws.charge_parameters->state,
         ws.charge_parameters->current);
      ws_server_socket->sendTXT(num,data_to_send);                    // Envois des données au client nouvellement connecté 
      break;
    case WStype_TEXT:
      ws_server_socket->sendTXT(num,payload);                         // Réalisation d'un echo !
      break;
    case WStype_DISCONNECTED:
    default:
      break;
  }
}

/**
 * \fn void ws_client_handler(void)
 * \brief Handler de WS Client à appeler régulièrement
 *        Cette procédure permet une connexion WebSocket client 
 *        propre et assynchrone avec le Module TIC       
 */
static void ws_client_handler(void) {
  switch(ws.client_state)
  {
    case ws_client_idle:
    default:
      // Ne rien faire
      break;

    case ws_client_connect:
      ws_client_socket.begin(ws.tic->ip_or_hostname,ws.tic->portWs);  // Connexion au Module TIC
      ws.client_state = ws_client_connecting;
      break;
      
    case ws_client_connecting:
      if(ws_client_socket.isConnected())                              // La WS est-elle connectée ?
        ws.client_state = ws_client_connected;                        // La connexion est établie
      ws.timers_client[ws_client_data_received] = millis();           // Armement du timer de réception de données
      // Pas de BREAK, c'est normal car il est nécessaire d'exécuter le loop() !
    case ws_client_connected:
      if(millis() - ws.timers_client[ws_client_data_received] >= TIMEOUT_DATA_RECV) // Le Timer de dernière données reçue est échue ?
        ws.client_state = ws_client_disconnect;                       // Déconnexion immédiate
      ws_client_socket.loop();                                        // Handler du service WebSocket Client
      break;
      
    case ws_client_disconnect:
      ws.client_state = ws_client_idle;
      ws_client_socket.disconnect();                                  // Déconnexion de la WS
      break;
  }
}

/**
 * \fn void ws_server_init(uint16_t port)
 */
void ws_server_init(uint16_t port) {
  ws_server_socket = new WebSocketsServer(port);                      // Configuration du serveur de WS
  ws_server_socket->onEvent(ws_server_on_event);                      // Configuration du handler OnEvent
  ws_server_socket->begin();                                          // Activation du serveur de WS
  ws.is_client_active = 0;                                            // Initialisation du service WS client
}

/**
 * \fn void ws_client_init(TIC_CONF_FIELDS_t *tic_conf)
 */
void ws_client_init(TIC_CONF_FIELDS_t *tic_conf) {
  ws_client_socket.onEvent(ws_client_on_event);                       // Configuration du handler OnEvent
  ws.client_state = ws_client_idle;                                   // Initialisation de l'état WS Client
  ws.is_client_active = 1;                                            // Initialisation du service WS Client
  ws.tic = tic_conf;                                                  // Initialisation du pointeur de données de configurations
}

/**
 * \fn void ws_client_connect_on_tic_module(TIC_DATA_t *tic)
 */
void ws_client_connect_on_tic_module(TIC_DATA_t *tic) {
  ws.client_state = ws_client_connect;                                // Forçage de la connexion
  ws.tic_data = tic;                                                  // Initialisation du pointeur de données TIC
}

/**
 * \fn void ws_client_disconnect_from_tic_module(void)
 */
void ws_client_disconnect_from_tic_module(void) {
  ws.client_state = ws_client_disconnect;                             // Forçage de la déconnexion
}

/**
 * \fn bool ws_client_is_connected(void)
 */
bool ws_client_is_connected(void) {
  return ws_client_socket.isConnected();
}

/**
 * \fn void ws_server_set_charge_parameters(CHARGE_PARAMETERS_t *data)
 */
void ws_server_set_charge_parameters(CHARGE_PARAMETERS_t *data) {
  ws.charge_parameters = data;
}

/**
 * \fn void ws_server_send_broadcast(void)
 */
void ws_server_send_broadcast(void) {
    char data_to_send[64];
    snprintf(data_to_send, sizeof(data_to_send),                      // Stringification des données
         "{\"State\":\"%d\",\"Current\":\"%d\"}",
         ws.charge_parameters->state,
         ws.charge_parameters->current);
    ws_server_socket->broadcastTXT(data_to_send);                     // Envois en broadcast sur la WS Server
}

/**
 * \fn void ws_handler(void)
 */
void ws_handler(void) {
   ws_server_socket->loop();                                          // Handler du service WebSocket Server
   if(ws.is_client_active) {                                          // Une WS Cliente est-elle active ?
    ws_client_handler();                                              // Handler du service WebSocket Client
   }
}
