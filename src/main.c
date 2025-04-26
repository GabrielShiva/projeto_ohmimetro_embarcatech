#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/i2c.h"
#include "inc/ssd1306.h"
#include "inc/font.h"
#include <math.h>

#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C
#define ADC_PIN 28 // GPIO para o voltímetro
#define Botao_A 5  // GPIO para botão A

int R_conhecido = 465;
int ADC_RESOLUTION = 4095;
int exponent = 0;
int exponent_rx = 0;
float R_x = 0.0;
float ADC_VREF = 3.29;
float normalized = 0.0;
float closest_resistor = 0.0;
float min_diff = 0.0;
float diff = 0.0;
float rx_e24_value = 0.0;
float normalized_rx = 0.0;

// definição de tabela para valores dos resistores e24
const float e24_values[] = {1.0, 1.1, 1.2, 1.3, 1.5, 1.6, 1.8, 2.0, 2.2, 2.4, 2.7, 3.0, 3.3, 3.6, 3.9, 4.3, 4.7, 5.1, 5.6, 6.2, 6.8, 7.5, 8.2, 9.1};
const int num_e24 = sizeof(e24_values)/sizeof(e24_values[0]);

const char *digit_colors[] = {"pret", "marr", "verm", "lara", "amar", "verde", "azul", "viol", "cinz", "bran"};
const char *multiplier_colors[] = {"pret", "marr", "verm", "lara", "amar", "verde", "azul", "viol", "cinz", "bran"};

// Trecho para modo BOOTSEL com botão B
#include "pico/bootrom.h"
#define botaoB 6

void gpio_irq_handler(uint gpio, uint32_t events) {
   reset_usb_boot(0, 0);
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
  // Para ser utilizado o modo BOOTSEL com botão B
  gpio_init(botaoB);
  gpio_set_dir(botaoB, GPIO_IN);
  gpio_pull_up(botaoB);
  gpio_set_irq_enabled_with_callback(botaoB, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
  // Aqui termina o trecho para modo BOOTSEL com botão B

  gpio_init(Botao_A);
  gpio_set_dir(Botao_A, GPIO_IN);
  gpio_pull_up(Botao_A);

  // I2C Initialisation. Using it at 400Khz.
  i2c_init(I2C_PORT, 400 * 1000);

  gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
  gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
  gpio_pull_up(I2C_SDA);                                        // Pull up the data line
  gpio_pull_up(I2C_SCL);                                        // Pull up the clock line
  ssd1306_t ssd;                                                // Inicializa a estrutura do display
  ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
  ssd1306_config(&ssd);                                         // Configura o display
  ssd1306_send_data(&ssd);                                      // Envia os dados para o display

  // Limpa o display. O display inicia com todos os pixels apagados.
  ssd1306_fill(&ssd, false);
  ssd1306_send_data(&ssd);

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

    // Fórmula simplificada: R_x = R_conhecido * ADC_encontrado /(ADC_RESOLUTION - adc_encontrado)
    R_x = (R_conhecido * media) / (ADC_RESOLUTION - media);

    rx_e24_value = get_comercial_value(R_x);
    normalized_rx = rx_e24_value;
    exponent_rx = 0;

    while (normalized_rx >= 10.0) {
      normalized_rx = normalized_rx / 10;
      exponent_rx = exponent_rx + 1;
    }

    int digit1 = (int)normalized_rx;
    int digit2 = (int)(normalized_rx * 10) % 10;
    const char *d1 = digit_colors[digit1 % 10];
    const char *d2 = digit_colors[digit2 % 10];
    const char *mult = get_multiplier_color(exponent_rx - 1);

    ssd1306_fill(&ssd, false);
    // ssd1306_draw_string(&ssd, "Rx= ", 2, 2);

    // sprintf(display_text, "%.0f ohms", rx_e24_value);
    // ssd1306_draw_string(&ssd, display_text, 30, 2);

    // desenho dos contornos do layout do display
    ssd1306_rect(&ssd, 1, 1, 126, 62, 1, 0);
    ssd1306_line(&ssd, 1, 15, 126, 15, 1);

    snprintf(display_text, sizeof(display_text), "faixa 1 %s", d1);
    ssd1306_draw_string(&ssd, display_text, 2, 20);

    snprintf(display_text, sizeof(display_text), "faixa 2 %s", d2);
    ssd1306_draw_string(&ssd, display_text, 2, 30);

    snprintf(display_text, sizeof(display_text), "multipl %s", mult);
    ssd1306_draw_string(&ssd, display_text, 2, 40);

    ssd1306_draw_string(&ssd, "toleran Au", 2, 50);

    ssd1306_send_data(&ssd);
    sleep_ms(700);

    // sprintf(str_x, "%1.0f", media); // Converte o inteiro em string
    // sprintf(str_y, "%1.0f", R_x);   // Converte o float em string

    // cor = !cor;
    //  Atualiza o conteúdo do display com animações
    // ssd1306_fill(&ssd, !cor);                          // Limpa o display
    // ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor);      // Desenha um retângulo
    // ssd1306_line(&ssd, 3, 25, 123, 25, cor);           // Desenha uma linha
    // ssd1306_line(&ssd, 3, 37, 123, 37, cor);           // Desenha uma linha
    // ssd1306_draw_string(&ssd, "CEPEDI   TIC37", 8, 6); // Desenha uma string
    // ssd1306_draw_string(&ssd, "EMBARCATECH", 20, 16);  // Desenha uma string
    // ssd1306_draw_string(&ssd, "  Ohmimetro", 10, 28);  // Desenha uma string
    // ssd1306_draw_string(&ssd, "ADC", 13, 41);          // Desenha uma string
    // ssd1306_draw_string(&ssd, "Resisten.", 50, 41);    // Desenha uma string
    // ssd1306_line(&ssd, 44, 37, 44, 60, cor);           // Desenha uma linha vertical
    // ssd1306_draw_string(&ssd, str_x, 8, 52);           // Desenha uma string
    // ssd1306_draw_string(&ssd, str_y, 59, 52);          // Desenha uma string
    // ssd1306_send_data(&ssd);                           // Atualiza o display
    // sleep_ms(700);
  }
}
