#include "reboot_utils.h"

//< Déclaration des variables globales
static REBOOT_t reboot;

/**
 * \fn void reboot_init(unsigned long ms)
 */
void reboot_init(unsigned long ms) {
  memset(&reboot,0,sizeof(REBOOT_t));
  reboot.timers[reboot_timeout] = ms;
  reboot.previous_state = HIGH;
  reboot.triggered = false;
  reboot.flag = false;
}

/**
 * \fn bool reboot_get_button_state(void)
 */
bool reboot_get_button_state(void) {
  bool state = hal_get_reboot();                                          // Lecture de l'état de la broche
  if(state != reboot.previous_state) {                                    // Un changement à eu lieu ?
    reboot.timers[reboot_previous] = millis();
    reboot.triggered = true;
  }    

  // Mécanisme d'anti-rebond temporel
  if(millis() - reboot.timers[reboot_previous] >= reboot.timers[reboot_timeout]) {
    if(state == LOW && reboot.triggered == true) {
      reboot.previous_state = state;
      reboot.triggered = false;
      return true;
    }
  }
  reboot.previous_state = state;
  return false;
}

/**
 * \fn void reboot_set(void)
 */
void reboot_set(void) {
  reboot.timers[reboot_previous] = millis();
  reboot.flag = true;
}

/**
 * \fn void reboot_handler(void)
 */
void reboot_handler(void) {
  if(reboot.flag == false)                                                // Aucun reboot n'a été demandé ?
    return;                                                               // Echappement immédiat
  // Autrement, attente du timer de reboot, est-il échue ?
  if(millis() - reboot.timers[reboot_previous] < reboot.timers[reboot_timeout])
    return;                                                               // Echappement immédiat

  WiFi.disconnect();                                                      // Déconnexion propre du Wifi
  ESP.restart();                                                          // Reboot de l'ESP_8266
}
