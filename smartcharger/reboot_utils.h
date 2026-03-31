#ifndef __REBOOT_UTILS__
#define __REBOOT_UTILS__

#include "common_utils.h"

//< Enumération des Timers REBOOT
enum {
  reboot_button_previous,
  reboot_previous,
  reboot_timeout,
  reboot_nb_timers
};

//< Structure des données REBOOT
typedef struct {
  unsigned long timers[reboot_nb_timers];
  bool previous_state;
  bool triggered;
  bool flag;
} REBOOT_t;

/**
 * \fn void reboot_init(unsigned long ms)
 * \brief Fonction permettant d'initialiser le module REBOOT
 * \param in, le temps avant reboot effectif
 */
void reboot_init(unsigned long ms);

/**
 * \fn bool reboot_get_button_state(void)
 * \brief Getter sur l'état de la demande de reboot
 * \return
 *      True  -> Une demande de reboot est effective par bouton
 *      False -> Aucune demande effectuée sur le bouton
 */
bool reboot_get_button_state(void);

/**
 * \fn void reboot_set(void)
 * \brief Fonction permettant d'effectuer une commande de reboot
 */
void reboot_set(void);

/**
 * \fn void reboot_handler(void)
 * \brief Handler du module reboot à appeler régulièrement
 */
void reboot_handler(void);

#endif
