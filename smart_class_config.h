/*
 * SMART CLASS CONFIG - Konfigurasi Pin, Tetapan, WHO/Kemenkes
 */
#ifndef SMART_CLASS_CONFIG_H
#define SMART_CLASS_CONFIG_H

// ============ PIN ============
#define DHTPIN       18
#define DHTTYPE      DHT22
#define FLAME_DO_PIN 27
#define FLAME_AO_PIN 33
#define BUZZER1_PIN  26   // Buzzer 1 (alarm/bel)
#define BUZZER2_PIN  25   // Buzzer 2 (passive - musik/piano)
#define MQ135_PIN    35
#define LDR_PIN      34   // LDR AO (analog)
#define LDR_DO_PIN   36   // LDR DO (digital Gelap/Terang)
#define SOUND_PIN    32
#define ULTRASONIC_TRIG_PIN 5
#define ULTRASONIC_ECHO_PIN 19

// ============ TELEGRAM ============
#define TELEGRAM_BOT_TOKEN "8324067380:AAHfMtWfLdtoYByjnrm2sgy90z3y01V6C-I"
#define TELEGRAM_USER_DEFAULT "6383896382"

// ============ WHO / KEMENKES STANDAR (Kelas Ideal) ============
#define WHO_TEMP_MIN     18.0f
#define WHO_TEMP_MAX     24.0f
#define KEMENKES_TEMP_MIN 18.0f
#define KEMENKES_TEMP_MAX 26.0f

#define WHO_HUMID_MIN    40.0f
#define WHO_HUMID_MAX    70.0f
#define KEMENKES_HUMID_MIN 40.0f
#define KEMENKES_HUMID_MAX 70.0f

#define WHO_NOISE_MAX    55.0f   // dB - kelas belajar
#define KEMENKES_NOISE_MAX 55.0f

#define WHO_LUX_MIN      300.0f
#define WHO_LUX_MAX      500.0f
#define KEMENKES_LUX_MIN  250.0f
#define KEMENKES_LUX_MAX  750.0f

#define GAS_RAW_BAIK     1500
#define GAS_RAW_MAX      2500

#endif
