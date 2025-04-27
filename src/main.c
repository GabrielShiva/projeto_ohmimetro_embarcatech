#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "inc/ssd1306.h"
#include "inc/font.h"

// Definição de macros gerais
#define ADC_PIN 28
#define BTN_B_PIN 6
#define ADC_RESOLUTION 4095

// Definição de macros para o protocolo I2C (SSD1306)
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define SSD1306_ADDRESS 0x3C

// Inicialização de variáveis
int reference_resistor = 470;
int exponent = 0;

int exponent_rx = 0;
float R_x = 0.0;
float ADC_VREF = 3.30;
float normalized = 0.0;
float closest_resistor = 0.0;
float min_diff = 0.0;
float diff = 0.0;
float rx_e24_value = 0.0;
float normalized_rx = 0.0;

int digit1 = 0;
int digit2= 0;
const char *d1 = "\n";
const char *d2 = "\n";
const char *mult = "\n";

ssd1306_t ssd;

// Definição de tabela para valores dos resistores e24
const float e24_values[] = {1.0, 1.1, 1.2, 1.3, 1.5, 1.6, 1.8, 2.0, 2.2, 2.4, 2.7, 3.0, 3.3, 3.6, 3.9, 4.3, 4.7, 5.1, 5.6, 6.2, 6.8, 7.5, 8.2, 9.1};
const int num_e24 = sizeof(e24_values)/sizeof(e24_values[0]);

const char *digit_colors[] = {"preto", "marrom", "vermelho", "laranja", "amarelo", "verde", "azul", "violeta", "cinza", "branco"};
const char *multiplier_colors[] = {"preto", "marrom", "vermelho", "laranja", "amarelo", "verde", "azul", "violeta", "cinza", "branco"};

void i2c_setup(uint baud_in_kilo) {
  i2c_init(I2C_PORT, baud_in_kilo * 1000);

  gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
  gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
  gpio_pull_up(I2C_SDA);
  gpio_pull_up(I2C_SCL);
}

void ssd1306_setup(ssd1306_t *ssd_ptr) {
  ssd1306_init(ssd_ptr, WIDTH, HEIGHT, false, SSD1306_ADDRESS, I2C_PORT); // Inicializa o display
  ssd1306_config(ssd_ptr);                                                // Configura o display
  ssd1306_send_data(ssd_ptr);                                             // Envia os dados para o display

  // Limpa o display. O display inicia com todos os pixels apagados.
  ssd1306_fill(ssd_ptr, false);
  ssd1306_send_data(ssd_ptr);
}

void gpio_irq_handler(uint gpio, uint32_t events) {
  if (gpio == BTN_B_PIN) {
    reset_usb_boot(0, 0);
  }
}

float get_comercial_value(float resistor_value) {
  if (resistor_value <= 0) {
     return 0.0;
  }

  normalized = resistor_value;
  exponent = 0;

  while (normalized >= 10) {
    normalized = normalized / 10;
    exponent = exponent + 1;
  }

  closest_resistor = e24_values[0];
  min_diff = 0.0;

  if (normalized >= closest_resistor) {
    min_diff = normalized - closest_resistor;
  } else {
    min_diff = closest_resistor - normalized;
  }

  for (int i = 0; i < num_e24; i++) {
    if (normalized >= e24_values[i]) {
      diff = normalized - e24_values[i];
    } else {
      diff = e24_values[i] - normalized;
    }

    if (diff < min_diff) {
      min_diff = diff;
      closest_resistor = e24_values[i];
    }
  }

  return closest_resistor * powf(10.0, exponent);
}

const char *get_multiplier_color(uint8_t exponent) {
  if (exponent == -2) {
    return "prata";
  }

  if (exponent == -1) {
    return "ouro";
  }

  if (exponent >= 0 && exponent <= 9) {
    return multiplier_colors[exponent];
  }

  return "preto";
}

int main() {
  // [INÍCIO] modo BOOTSEL associado ao botão B (apenas para desenvolvedores)
  gpio_init(BTN_B_PIN);
  gpio_set_dir(BTN_B_PIN, GPIO_IN);
  gpio_pull_up(BTN_B_PIN);
  gpio_set_irq_enabled_with_callback(BTN_B_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
  // [FIM] modo BOOTSEL associado ao botão B (apenas para desenvolvedores)

  // Inicialização do protocolo I2C para o display com 400Khz
  i2c_setup(400);

  // Iniciliazação do display
  ssd1306_setup(&ssd);

  adc_init();
  adc_gpio_init(ADC_PIN); // GPIO 28 como entrada analógica

  float tensao;
  char display_text[20]; // Buffer para armazenar a string

  bool cor = true;
  while (true) {
    adc_select_input(2); // Seleciona o ADC para eixo X. O pino 28 como entrada analógica

    float soma = 0.0f;
    for (int i = 0; i < 500; i++) {
      soma += adc_read();
      sleep_ms(1);
    }

    float media = soma / 500.0f;

    // Calcula da resistencia em ohms e obtenção do valor comercial mais próximo
    R_x = (reference_resistor * media) / (ADC_RESOLUTION - media);
    rx_e24_value = get_comercial_value(R_x);

    // Calculo das cores de cada banda
    normalized_rx = rx_e24_value;
    exponent_rx = 0;

    while (normalized_rx >= 10.0) {
      normalized_rx = normalized_rx / 10;
      exponent_rx = exponent_rx + 1;
    }

    digit1 = (int)normalized_rx;
    digit2 = (int)(normalized_rx * 10) % 10;
    d1 = digit_colors[digit1 % 10];
    d2 = digit_colors[digit2 % 10];
    mult = get_multiplier_color(exponent_rx - 1);

    // Limpeza do display
    ssd1306_fill(&ssd, false);
    // desenho dos contornos do layout do display
    ssd1306_rect(&ssd, 1, 1, 126, 62, 1, 0);
    // ssd1306_line(&ssd, 1, 15, 126, 15, 1);
    // ssd1306_line(&ssd, 1, 16, 126, 16, 1);
    //cima
    ssd1306_line(&ssd, 5, 5, 5, 11, 1);
    ssd1306_line(&ssd, 6, 4, 10, 4, 1);
    ssd1306_line(&ssd, 10, 5, 20, 5, 1);
    //baixo
    ssd1306_line(&ssd, 6, 12, 10, 12, 1);
    ssd1306_line(&ssd, 10, 11, 20, 11, 1);
    //primeira faixa
    ssd1306_line(&ssd, 8, 4, 8, 12, 1);
    ssd1306_line(&ssd, 9, 4, 9, 12, 1);
    //segunda faixa
    ssd1306_line(&ssd, 13, 5, 13, 11, 1);
    ssd1306_line(&ssd, 14, 5, 14, 11, 1);
    //multiplicador
    ssd1306_line(&ssd, 17, 5, 17, 11, 1);
    ssd1306_line(&ssd, 18, 5, 18, 11, 1);
    //tolerancia
    ssd1306_line(&ssd, 21, 5, 21, 11, 1);
    ssd1306_line(&ssd, 22, 5, 22, 11, 1);
    //cima
    ssd1306_line(&ssd, 20, 4, 24, 4, 1);
    ssd1306_line(&ssd, 20, 12, 24, 12, 1);
    ssd1306_line(&ssd, 25, 5, 25, 11, 1);

    // seta para a tolerancia
    ssd1306_line(&ssd, 21, 15, 21, 23, 1);
    ssd1306_line(&ssd, 22, 15, 22, 23, 1);

    ssd1306_line(&ssd, 22, 22, 50, 22, 1);
    ssd1306_line(&ssd, 22, 23, 50, 23, 1);

    ssd1306_line(&ssd, 50, 20, 50, 25, 1);
    ssd1306_line(&ssd, 51, 21, 51, 24, 1);
    ssd1306_line(&ssd, 52, 22, 52, 23, 1);

    // seta para o multiplicador
    ssd1306_line(&ssd, 17, 15, 17, 35, 1);
    ssd1306_line(&ssd, 18, 15, 18, 35, 1);

    ssd1306_line(&ssd, 18, 34, 50, 34, 1);
    ssd1306_line(&ssd, 18, 35, 50, 35, 1);

    ssd1306_line(&ssd, 50, 32, 50, 37, 1);
    ssd1306_line(&ssd, 51, 33, 51, 36, 1);
    ssd1306_line(&ssd, 52, 34, 52, 35, 1);

    // seta para a segunda faixa
    ssd1306_line(&ssd, 13, 15, 13, 47, 1);
    ssd1306_line(&ssd, 14, 15, 14, 47, 1);

    ssd1306_line(&ssd, 14, 46, 50, 46, 1);
    ssd1306_line(&ssd, 14, 47, 50, 47, 1);

    ssd1306_line(&ssd, 50, 44, 50, 49, 1);
    ssd1306_line(&ssd, 51, 45, 51, 48, 1);
    ssd1306_line(&ssd, 52, 46, 52, 47, 1);

    // seta para a primeira faixa
    ssd1306_line(&ssd, 8, 15, 8, 57, 1);
    ssd1306_line(&ssd, 9, 15, 9, 57, 1);

    ssd1306_line(&ssd, 9, 56, 50, 56, 1);
    ssd1306_line(&ssd, 9, 57, 50, 57, 1);

    ssd1306_line(&ssd, 50, 54, 50, 59, 1);
    ssd1306_line(&ssd, 51, 55, 51, 58, 1);
    ssd1306_line(&ssd, 52, 56, 52, 57, 1);

    sprintf(display_text, "%.0f ohms", rx_e24_value);
    ssd1306_draw_string(&ssd, display_text, 29, 5);

    ssd1306_draw_string(&ssd, "Au(tol.)", 60, 20);

    snprintf(display_text, sizeof(display_text), "%s", mult);
    ssd1306_draw_string(&ssd, display_text, 60, 31);

    snprintf(display_text, sizeof(display_text), "%s", d2);
    ssd1306_draw_string(&ssd, display_text, 60, 42);

    snprintf(display_text, sizeof(display_text), "%s", d1);
    ssd1306_draw_string(&ssd, display_text, 60, 52);

    ssd1306_send_data(&ssd);
    sleep_ms(700);
  }

  return 0;
}
