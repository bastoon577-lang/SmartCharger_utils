#include "led_utils.h"

//< Déclaration des variables globales
static LED_t led;

/**
 * \fn void led_init(unsigned long ms)
 */
void led_init(unsigned long ms) {
  memset(&led,0,sizeof(LED_t));
  led.timers[led_timeout] = ms;
}

/**
 * \fn void led_change_mode(unsigned long ms)
 */
void led_change_mode(unsigned long ms) {
  led.timers[led_timeout] = ms;
}

/**
 * \fn void led_handler(void)
 */
void led_handler(void) {
  if(millis() - led.timers[led_privious] < led.timers[led_timeout])   // Le Timer est échue ?
    return;                                                           // Echappement immédiat

  led.timers[led_privious] = millis();                                // Réinitialisation du Timer
  hal_toggle_led();                                                   // Toggle de la LED
}