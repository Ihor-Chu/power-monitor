#include <stdint.h>

#ifdef __cplusplus  
extern "C" {  
#endif
typedef struct calibrate {
  uint16_t adc;
  uint16_t mvolt;
} calibrate_s;

#define MAX_CALIBRATE 16
#define EEPROM_DATA_ID 1

typedef struct calibrate_eeprom {
  uint32_t crc32;
  uint16_t size;
  calibrate_s data[MAX_CALIBRATE];
  uint8_t id;
} calibrate_eeprom_s;

typedef struct rtc_flag {
  uint32_t crc32;
  uint32_t flag;
} rtc_flag_s;

#define ADC_DATA_WINDOW_SIZE 32
typedef struct adc_data_window {
  uint8_t next_free_el;
  uint16_t data[ADC_DATA_WINDOW_SIZE];
} adc_data_window_s;

typedef struct rtc_data {
  uint32_t crc32;
  adc_data_window_s adc_data;
} rtc_data_s;

typedef struct appr_data
{
	uint16_t adc;
} appr_data_s;
#define STEPX 4 * 60

uint16_t get_average_adc(adc_data_window_s * adc_data_);
void add_measure_to_adc_data(uint16_t measure, adc_data_window_s * adc_data_);
int adc_to_mv(uint16_t adc, calibrate_eeprom_s * calibrate_eeprom_data_);
uint16_t mv_to_adc(int mv, calibrate_eeprom_s * calibrate_data_);
void do_add_calibrate(calibrate_eeprom_s * calibrate_eeprom_data_, bool * is_dirty, uint16_t adc, uint16_t mvolt);
void do_rm_calibrate(calibrate_eeprom_s * calibrate_eeprom_data_, bool * is_dirty, uint8_t idx);

void getApprox(uint16_t * data, double *a, double *b, int n);

#ifdef __cplusplus  
}
#endif
