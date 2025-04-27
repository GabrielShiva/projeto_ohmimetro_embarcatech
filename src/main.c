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
float adc_resolution = 4095;

// Definição de macros para o protocolo I2C (SSD1306)
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define SSD1306_ADDRESS 0x3C

// Inicialização de variáveis
int reference_resistor = 470;
float cumulative_adc_measure = 0.0f;
float average_adc_measures = 0.0f;
float unknown_resistor = 0.0;
float closest_e24_resistor = 0.0;

const char *d1 = "\n";
const char *d2 = "\n";
const char *mult = "\n";
char display_text[20];

ssd1306_t ssd;

// Definição de tabela para valores dos resistores da série e24
const float e24_resistor_values[24] = {1.0, 1.1, 1.2, 1.3, 1.5, 1.6, 1.8, 2.0, 2.2, 2.4, 2.7, 3.0, 3.3, 3.6, 3.9, 4.3, 4.7, 5.1, 5.6, 6.2, 6.8, 7.5, 8.2, 9.1};
const int num_e24_resistor_values = sizeof(e24_resistor_values) / sizeof(e24_resistor_values[0]);

const char *available_digit_colors[10] = {"preto", "marrom", "vermelho", "laranja", "amarelo", "verde", "azul", "violeta", "cinza", "branco"};
const char *resistor_band_colors[3] = {0};

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

float get_closest_e24_resistor(float resistor_value) {
  if (resistor_value <= 0) {
     return 0.0;
  }

  float normalized_resistor = resistor_value;
  float exponent = 0.0f;

  // Normaliza o valor fornecido para a faixa [0-10]
  while (normalized_resistor >= 10) {
    normalized_resistor = normalized_resistor / 10;
    exponent = exponent + 1.0;
  }

  float closest_resistor = e24_resistor_values[0];
  float min_diff = fabs(normalized_resistor - e24_resistor_values[0]);

  for (int i = 0; i < num_e24_resistor_values; i++) {
    float curr_diff = fabs(normalized_resistor - e24_resistor_values[i]);

    if (curr_diff < min_diff) {
      min_diff = curr_diff;
      closest_resistor = e24_resistor_values[i];
    }
  }

  return closest_resistor * powf(10.0, exponent);
}

void get_band_color(float *resistor_value) {
  // Cálculo das cores de cada banda do resistor (4 bandas)
  float normalized_resistor = *resistor_value;
  int exponent = -1;

  // Normaliza o valor fornecido para a faixa [0-10]
  while (normalized_resistor >= 10.0) {
    normalized_resistor = normalized_resistor / 10;
    exponent = exponent + 1;
  }

  // Obtenção do valor da primeira banda
  // EX.: 3.7 => (int)(3.7) => 3
  int first_band_value = (int)normalized_resistor;

  // Obtenção do valor da segunda banda
  // EX.: 3.7 => 3.7 * 10 => 37 => 37 % 10 => 7.0 => (int)(7.0) => 7
  int second_band_value = (int)(normalized_resistor * 10) % 10;

  // Definição da das Bandas 1, 2 e multiplicador
  resistor_band_colors[0] = available_digit_colors[first_band_value % 10];
  resistor_band_colors[1] = available_digit_colors[second_band_value % 10];
  resistor_band_colors[2] = (exponent >= 0 && exponent <= 9) ? available_digit_colors[exponent] : "erro";
}

void draw_display_layout(ssd1306_t *ssd_ptr) {
  // desenho dos contornos do layout do display
  ssd1306_rect(ssd_ptr, 1, 1, 126, 62, 1, 0);
  //cima
  ssd1306_line(ssd_ptr, 5, 5, 5, 11, 1);
  ssd1306_line(ssd_ptr, 6, 4, 10, 4, 1);
  ssd1306_line(ssd_ptr, 10, 5, 20, 5, 1);
  //baixo
  ssd1306_line(ssd_ptr, 6, 12, 10, 12, 1);
  ssd1306_line(ssd_ptr, 10, 11, 20, 11, 1);
  //primeira faixa
  ssd1306_line(ssd_ptr, 8, 4, 8, 12, 1);
  ssd1306_line(ssd_ptr, 9, 4, 9, 12, 1);
  //segunda faixa
  ssd1306_line(ssd_ptr, 13, 5, 13, 11, 1);
  ssd1306_line(ssd_ptr, 14, 5, 14, 11, 1);
  //multiplicador
  ssd1306_line(ssd_ptr, 17, 5, 17, 11, 1);
  ssd1306_line(ssd_ptr, 18, 5, 18, 11, 1);
  //tolerancia
  ssd1306_line(ssd_ptr, 21, 5, 21, 11, 1);
  ssd1306_line(ssd_ptr, 22, 5, 22, 11, 1);
  //cima
  ssd1306_line(ssd_ptr, 20, 4, 24, 4, 1);
  ssd1306_line(ssd_ptr, 20, 12, 24, 12, 1);
  ssd1306_line(ssd_ptr, 25, 5, 25, 11, 1);

  // seta para a tolerancia
  ssd1306_line(ssd_ptr, 21, 15, 21, 23, 1);
  ssd1306_line(ssd_ptr, 22, 15, 22, 23, 1);

  ssd1306_line(ssd_ptr, 22, 22, 50, 22, 1);
  ssd1306_line(ssd_ptr, 22, 23, 50, 23, 1);

  ssd1306_line(ssd_ptr, 50, 20, 50, 25, 1);
  ssd1306_line(ssd_ptr, 51, 21, 51, 24, 1);
  ssd1306_line(ssd_ptr, 52, 22, 52, 23, 1);

  // seta para o multiplicador
  ssd1306_line(ssd_ptr, 17, 15, 17, 35, 1);
  ssd1306_line(ssd_ptr, 18, 15, 18, 35, 1);

  ssd1306_line(ssd_ptr, 18, 34, 50, 34, 1);
  ssd1306_line(ssd_ptr, 18, 35, 50, 35, 1);

  ssd1306_line(ssd_ptr, 50, 32, 50, 37, 1);
  ssd1306_line(ssd_ptr, 51, 33, 51, 36, 1);
  ssd1306_line(ssd_ptr, 52, 34, 52, 35, 1);

  // seta para a segunda faixa
  ssd1306_line(ssd_ptr, 13, 15, 13, 47, 1);
  ssd1306_line(ssd_ptr, 14, 15, 14, 47, 1);

  ssd1306_line(ssd_ptr, 14, 46, 50, 46, 1);
  ssd1306_line(ssd_ptr, 14, 47, 50, 47, 1);

  ssd1306_line(ssd_ptr, 50, 44, 50, 49, 1);
  ssd1306_line(ssd_ptr, 51, 45, 51, 48, 1);
  ssd1306_line(ssd_ptr, 52, 46, 52, 47, 1);

  // seta para a primeira faixa
  ssd1306_line(ssd_ptr, 8, 15, 8, 57, 1);
  ssd1306_line(ssd_ptr, 9, 15, 9, 57, 1);

  ssd1306_line(ssd_ptr, 9, 56, 50, 56, 1);
  ssd1306_line(ssd_ptr, 9, 57, 50, 57, 1);

  ssd1306_line(ssd_ptr, 50, 54, 50, 59, 1);
  ssd1306_line(ssd_ptr, 51, 55, 51, 58, 1);
  ssd1306_line(ssd_ptr, 52, 56, 52, 57, 1);
}

int main() {
  // [INÍCIO] modo BOOTSEL associado ao botão B (apenas para desenvolvedores)
  gpio_init(BTN_B_PIN);
  gpio_set_dir(BTN_B_PIN, GPIO_IN);
  gpio_pull_up(BTN_B_PIN);
  gpio_set_irq_enabled_with_callback(BTN_B_PIN, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
  // [FIM] modo BOOTSEL associado ao botão B (apenas para desenvolvedores)

  // Inicialização do protocolo I2C com 400Khz e inicialização do display
  i2c_setup(400);
  ssd1306_setup(&ssd);

  // Inicialização do ADC para o pino 28
  adc_init();
  adc_gpio_init(ADC_PIN);

  while (true) {
    // Seleciona o ADC para pino 28 como entrada analógica
    adc_select_input(2);

    // Obtenção de várias leituras seguidas e média
    cumulative_adc_measure = 0.0f;

    for (int i = 0; i < 500; i++) {
      cumulative_adc_measure += adc_read();
      sleep_ms(1);
    }

    average_adc_measures = cumulative_adc_measure / 500.0f;

    // Cálculo da resistencia em ohms e obtenção do valor comercial mais próximo
    unknown_resistor = (reference_resistor * average_adc_measures) / (adc_resolution - average_adc_measures);
    closest_e24_resistor = get_closest_e24_resistor(unknown_resistor);

    get_band_color(&closest_e24_resistor);

    // Limpeza do display
    ssd1306_fill(&ssd, false);
    draw_display_layout(&ssd);

    // Exibição do valor comercial da resistência mais próxima
    sprintf(display_text, "%.0f ohms", closest_e24_resistor);
    ssd1306_draw_string(&ssd, display_text, 29, 5);

    // Exibição das cores de cada banda (Tolerância Multiplicador Faixa_2 Faixa_1)
    ssd1306_draw_string(&ssd, "Au 5%", 60, 20);
    ssd1306_draw_string(&ssd, resistor_band_colors[2], 60, 31);
    ssd1306_draw_string(&ssd, resistor_band_colors[1], 60, 42);
    ssd1306_draw_string(&ssd, resistor_band_colors[0], 60, 52);

    ssd1306_send_data(&ssd);
    sleep_ms(700);
  }

  return 0;
}
