#include <EEPROM.h>

#include "eeprom_utils.h"

/**
 * \fn void eeprom_init(void)
 */
void eeprom_init(void) {
  EEPROM.begin(EEPROM_SIZE);
}

/**
 * \fn void eeprom_write_data(void* data, EepromDataType type)
 */
void eeprom_write_data(void* data, EepromDataType type) {
  hal_disable_interrupt();
  switch (type) {
    case EEPROM_STATIC_CFG:
      EEPROM.put(BASE_ADDR_STATIC_CFG, *(STATIC_CONF_FIELDS_t*)data);
      break;

    case EEPROM_VOLATILE_CFG:
      EEPROM.put(BASE_ADDR_VOLATILE_CFG, *(VOLATILE_CONF_FIELDS_t*)data);
      break;

    case EEPROM_TIC_CFG:
      EEPROM.put(BASE_ADDR_TIC_CFG, *(TIC_CONF_FIELDS_t*)data);
      break;

    default:
      return;
  }
  EEPROM.commit();
  hal_enable_interrupt();
}

/**
 * \fn void eeprom_read_data(void* data, EepromDataType type)
 */
void eeprom_read_data(void* data, EepromDataType type) {
  switch (type) {

    case EEPROM_STATIC_CFG:
      EEPROM.get(BASE_ADDR_STATIC_CFG, *(STATIC_CONF_FIELDS_t*)data);
      break;

    case EEPROM_VOLATILE_CFG:
      EEPROM.get(BASE_ADDR_VOLATILE_CFG, *(VOLATILE_CONF_FIELDS_t*)data);
      break;

    case EEPROM_TIC_CFG:
      EEPROM.get(BASE_ADDR_TIC_CFG, *(TIC_CONF_FIELDS_t*)data);
      break;

    default :
      return;
  }
}
