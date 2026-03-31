#ifndef __LED_UTILS__
#define __LED_UTILS__

#include "common_utils.h"

//< Enumération des Timers LED
enum {
  led_timeout,
  led_privious,
  led_nb_timers
};

//< Structure de données LED
typedef struct {
  unsigned long timers[led_nb_timers];
} LED_t;

/**
 * \fn void led_init(unsigned long ms)
 * \brief Fonction permettant d'initialiser le service LED
 * \param in le temps de clignotement (Toggle de la LED)
 */
void led_init(unsigned long ms);

/**
 * \fn void led_change_mode(unsigned long ms)
 * \brief Fonction permettant de changer le mode de clignotement de la LED
 * \param in le temps de clignotement (Toggle de la LED)
 */
void led_change_mode(unsigned long ms);

/**
 * \fn void led_handler(void)
 * \brief Handler du service LED à appeler régulièrement
 */
void led_handler(void);

#endif
