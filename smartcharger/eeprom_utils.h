#ifndef __EEPROM_UTILS__
#define __EEPROM_UTILS__

#include "common_utils.h"

#define EEPROM_START        0
#define EEPROM_SIZE         512

//< Définition des adresses de bases en EEPROM
#define BASE_ADDR_STATIC_CFG      EEPROM_START
#define BASE_ADDR_TIC_CFG         BASE_ADDR_STATIC_CFG + sizeof(STATIC_CONF_FIELDS_t)
#define BASE_ADDR_VOLATILE_CFG    BASE_ADDR_TIC_CFG + sizeof(TIC_CONF_FIELDS_t)

/**
 * \fn void eeprom_init(void)
 * \brief Fonction permettant d'initialiser le service EEPROM
 */
void eeprom_init(void);

/**
 * \fn void eeprom_write_data(void* data, EepromDataType type)
 * \brief Fonction permettant l'écriture de données en mémoire EEPROM
 * \param in, la donnée à stocker en mémoire EEPROM
 * \param in, le type de donnée
 */
void eeprom_write_data(void* data, EepromDataType type);

/**
 * \fn void eeprom_read_data(void* data, EepromDataType type)
 * \brief Fonction permettant la lecture de données à partir de la mémoire EEPROM
 * \param out, le pointeur de données extraites de la mémoire EEPROM
 * \param in, le type de donnée
 */
void eeprom_read_data(void* data, EepromDataType type);

#endif
