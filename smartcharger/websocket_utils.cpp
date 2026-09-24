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
  char value[17];

  // Mode Historique
  if(strstr(str,"\"ISOUSC\"")) {
    if(sscanf(str,"{\"ISOUSC\":\"%2[^\"]\"}",value) == 1)
      ws.tic_data->i_max = (uint8_t)atoi(value);
  }else if(strstr(str,"\"IINST\"")) {
    if(sscanf(str,"{\"IINST\":\"%3[^\"]\"}",value) == 1)
      ws.tic_data->i_inst[0] = (uint8_t)atoi(value);
  }else if(strstr(str,"\"IINST1\"")) {
    if(sscanf(str,"{\"IINST1\":\"%3[^\"]\"}",value) == 1)
      ws.tic_data->i_inst[0] = (uint8_t)atoi(value);
  }else if(strstr(str,"\"IINST2\"")) {
    if(sscanf(str,"{\"IINST2\":\"%3[^\"]\"}",value) == 1)
      ws.tic_data->i_inst[1] = (uint8_t)atoi(value);
  }else if(strstr(str,"\"IINST3\"")) {
    if(sscanf(str,"{\"IINST3\":\"%3[^\"]\"}",value) == 1)
      ws.tic_data->i_inst[2] = (uint8_t)atoi(value);
  }else if(strstr(str,"\"PTEC\"")) {
    if(sscanf(str, "{\"PTEC\":\"%4[^\"]\"}",value) == 1)
      strcpy(ws.tic_data->tarif, value);
  } 
  // Mode Standard
  else if(strstr(str,"\"IRMS1\"")) {
    if(sscanf(str, "{\"IRMS1\":\"%3[^\"]\"}",value) == 1)
      ws.tic_data->i_inst[0] = (uint8_t)atoi(value);
  }else if(strstr(str,"\"IRMS2\"")) {
    if(sscanf(str, "{\"IRMS2\":\"%3[^\"]\"}",value) == 1)
      ws.tic_data->i_inst[1] = (uint8_t)atoi(value);
  }else if(strstr(str,"\"IRMS3\"")) {
    if(sscanf(str, "{\"IRMS3\":\"%3[^\"]\"}",value) == 1)
      ws.tic_data->i_inst[2] = (uint8_t)atoi(value);
  }else if(strstr(str,"\"SINSTI\"")) {
    if(sscanf(str, "{\"SINSTI\":\"%5[^\"]\"}",value) == 1)
      ws.tic_data->p_injectee = (uint16_t)atoi(value);
  }else if(strstr(str,"\"LTARF\"")) {
    if(sscanf(str, "{\"LTARF\":\"%16[^\"]\"}",value) == 1)
      strcpy(ws.tic_data->tarif, value);
  }else if(strstr(str,"\"PCOUP\"")) {
    if(sscanf(str, "{\"PCOUP\":\"%2[^\"]\"}",value) == 1) {
      if(ws.static_conf) {
        ws.tic_data->i_max = (!ws.static_conf->which_voltage) ?
                              ((uint8_t)atoi(value)*10)/2 : ((uint8_t)atoi(value)*10)/6;
      }
    }
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
      // Si la session socket doit rester active mais que la connexion a été perdue,
      // on relance automatiquement une tentative après un délai de TIMEOUT_RECONNECT_RETRY.
      if (ws.is_client_socket_active && (millis() - 
	      ws.timers_client[ws_client_data_received] >= TIMEOUT_RECONNECT_RETRY)) {
        ws.client_state = ws_client_connect;
      }
      break;

    case ws_client_connect:
      ws_client_socket.begin(ws.tic->ip_or_hostname,ws.tic->portWs);  // Connexion au Module TIC
      ws.timers_client[ws_client_data_received] = millis();          // Armement du timer pour le timeout de connexion
      ws.client_state = ws_client_connecting;
      break;
        
    case ws_client_connecting:
      ws_client_socket.loop();                                        // Traitement réseau pendant la tentative
      if(ws_client_socket.isConnected()) {                            // La WS est-elle connectée ?
        ws.client_state = ws_client_connected;                        // La connexion est établie
        ws.timers_client[ws_client_data_received] = millis();         // Armement du timer de réception de données
      }
      else if(millis() - ws.timers_client[ws_client_data_received] >= TIMEOUT_CONNECTING) {
        ws.client_state = ws_client_disconnect;                       // Abandon propre si le module ne répond pas
      }
      break;

    case ws_client_connected:
      // Le flux de données provenant du ModuleTIC s'est arrêté depuis plus de TIMEOUT_ALIVE
      if(millis() - ws.timers_client[ws_client_data_received] >= TIMEOUT_ALIVE) {
        // Le dernier ping date d'avant TIMEOUT_PING
        if(millis() - ws.timers_client[ws_client_last_ping] >= TIMEOUT_PING) {
          ws.timers_client[ws_client_last_ping] = millis();           // Réinitialisation du timer de ping
          ws_client_socket.sendTXT("KeepAlive");                      // Emission d'un ping
        }
      }
      // Toujours aucun flux de données provenant du ModuleTIC malgré les pings !
      if(millis() - ws.timers_client[ws_client_data_received] >= TIMEOUT_DATA_RECV) {
        ws.client_state = ws_client_disconnect;                       // Déconnexion immédiate
      }
      ws_client_socket.loop();                                        // Handler du service WebSocket Client
      break;
        
    case ws_client_disconnect:
      ws.timers_client[ws_client_data_received] = millis();           // Armement du chrono de pause avant la prochaine tentative
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
 * \fn void ws_client_init(TIC_CONF_FIELDS_t *tic_conf, STATIC_CONF_FIELDS_t *static_conf) {
 */
void ws_client_init(TIC_CONF_FIELDS_t *tic_conf, STATIC_CONF_FIELDS_t *static_conf) {
  ws_client_socket.onEvent(ws_client_on_event);                       // Configuration du handler OnEvent
  ws.client_state = ws_client_idle;                                   // Initialisation de l'état WS Client
  ws.static_conf = static_conf;                                       // Initialisation du pointeur vers la configuration
  ws.is_client_active = 1;                                            // Service WS Client actif
  ws.is_client_socket_active = 0;                                     // Maintien automatique de la socket inactif par défaut
  ws.tic = tic_conf;                                                  // Initialisation du pointeur de données de configurations
}

/**
 * \fn void ws_client_connect_on_tic_module(TIC_DATA_t *tic)
 */
void ws_client_connect_on_tic_module(TIC_DATA_t *tic) {
  ws.tic_data = tic;                                                  // Initialisation du pointeur de données TIC
  ws.is_client_socket_active = 1;                                     // Active le maintien automatique de la connexion
  if(ws.client_state == ws_client_idle) {
    ws.client_state = ws_client_connect;                              // Demande de connexion
  }
}

/**
 * \fn void ws_client_disconnect_from_tic_module(void)
 */
void ws_client_disconnect_from_tic_module(void) {
  ws.is_client_socket_active = 0;                                     // Désactive le maintien automatique de la connexion
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
  snprintf(data_to_send, sizeof(data_to_send),                        // Stringification des données
           "{\"State\":\"%d\",\"Current\":\"%d\"}",
           ws.charge_parameters->state,
           ws.charge_parameters->current);
  ws_server_socket->broadcastTXT(data_to_send);                       // Envois en broadcast sur la WS Server
}

/**
 * \fn void ws_handler(void)
 */
void ws_handler(void) {
  ws_server_socket->loop();											  // Handler du service WebSocket Server
  if(ws.is_client_active) {                                           // Une WS Cliente est-elle active ?
    ws_client_handler();                                              // Handler du service WebSocket Client
  }
  yield();                                                            // Sécurité pour la pile Wi-Fi de l'ESP8266
}