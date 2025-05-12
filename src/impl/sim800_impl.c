#include "main.h"

void sim800_pin_pwr_key_on(void) {
    HAL_GPIO_WritePin(SIM800_RESET_GPIO_Port, SIM800_RESET_Pin, GPIO_PIN_SET);
}

void sim800_pin_pwr_key_off(void) {
    HAL_GPIO_WritePin(SIM800_RESET_GPIO_Port, SIM800_RESET_Pin, GPIO_PIN_RESET);
}

void     sim800_pin_module_power_on() {
    HAL_GPIO_WritePin(SIM_OFF_GPIO_Port, SIM_OFF_Pin, GPIO_PIN_RESET);
}
void     sim800_pin_module_power_off() {
    HAL_GPIO_WritePin(SIM_OFF_GPIO_Port, SIM_OFF_Pin, GPIO_PIN_SET);
}

void sim800_rx_buffer_flush(void) { circular_buf_reset(cbuf_rx); }

void sim800_tx_buffer_flush(void) { circular_buf_reset(cbuf_tx); }

bool sim800_tx_buffer_full() { return circular_buf_full(cbuf_tx); }

void sim800_tx_buffer_put(char c) { circular_buf_put(cbuf_tx, c); }

void sim800_txe_irq_enable() {
    if (!LL_USART_IsEnabledIT_TXE(USART2)) {
        LL_USART_EnableIT_TXE(USART2);
    }
}

void sim800_rxne_irq_disable() { LL_USART_DisableIT_RXNE(USART2); }

void sim800_rxne_irq_enable() { LL_USART_EnableIT_RXNE(USART2); }

bool sim800_rx_buffer_empty() { return circular_buf_empty(cbuf_rx); }

uint8_t sim800_rx_buffer_get() {
    uint8_t c;
    circular_buf_get(cbuf_rx, &c);
    return c;
}

uint32_t sim800_get_tick() { return HAL_GetTick(); }

void sim800_delay_ms(uint32_t time_ms) { HAL_Delay(time_ms); }

