#include "eeprom_in_flash.h"

void
eeprom_in_flash_page_erase(uint32_t address) {
  uint32_t PageError = 0;

  FLASH_EraseInitTypeDef EraseInitStruct = {
      .TypeErase = FLASH_TYPEERASE_PAGES,
      .PageAddress = address,
      .NbPages = 1
    };

  HAL_FLASH_Unlock();
  HAL_FLASHEx_Erase(&EraseInitStruct, &PageError);
  HAL_FLASH_Lock();
}

void
eeprom_in_flash_write_word(uint32_t address, uint32_t u32) {
  HAL_FLASH_Unlock();
  HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, (uint64_t)u32);
  HAL_FLASH_Lock();

}

void
eeprom_in_flash_write_half_word(uint32_t address, uint16_t u16) {
  HAL_FLASH_Unlock();
  HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, address, (uint64_t)u16);
  HAL_FLASH_Lock();
}
