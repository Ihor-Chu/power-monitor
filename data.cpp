#include <cstdint>
#ifdef __cplusplus  
extern "C" {  
#endif
#include "data.h"

uint16_t get_average_adc(adc_data_window_s * adc_data_) {
  if (adc_data_->next_free_el == 0) return 0;
#define  AVG_SIZE 4
uint16_t avg = 0;
uint8_t cnt = 0;
  for (uint8_t i = adc_data_->next_free_el - 1; (i >= 0) && (cnt < AVG_SIZE);  i--) {
    avg += adc_data_->data[i];
    cnt++;
  };
  return avg / cnt;
}

void add_measure_to_adc_data(uint16_t measure, adc_data_window_s * adc_data_) {
  if (adc_data_->next_free_el >= ADC_DATA_WINDOW_SIZE)
  {
    // shift data
    for (uint8_t i = 0; i < adc_data_->next_free_el-1; i++) {
        adc_data_->data[i] = adc_data_->data[i+1];
    };
    adc_data_->next_free_el = ADC_DATA_WINDOW_SIZE - 1;
  };
  adc_data_->data[adc_data_->next_free_el] = measure;
  if (adc_data_->next_free_el <= ADC_DATA_WINDOW_SIZE - 1)
    adc_data_->next_free_el++;
}

int line_approx(int adc, int x1, int x2, int y1, int y2) {
  if (adc == x1) return y1;
  return (y2 - y1) * (adc - x1) / (x2 - x1) + y1;
}

int adc_to_mv(uint16_t adc, calibrate_eeprom_s * calibrate_eeprom_data_) 
{
  // 750 == 12 000 mv
  // return adc * 12000 / 750;
  uint8_t right_point = calibrate_eeprom_data_->size - 1;
  for (uint8_t i = right_point; i > 0; i--) {
    if (adc < calibrate_eeprom_data_->data[i].adc) {
      right_point = i;
    } else {
      break;
    }
  };
/*  
  Serial.printf("use points %d and %d to line_approx (%d:%d) (%d:%d)", right_point - 1, right_point, calibrate_eeprom_data_->data[right_point - 1].adc,
                calibrate_eeprom_data_->data[right_point - 1].mvolt, calibrate_eeprom_data_->data[right_point].adc, calibrate_eeprom_data.data[right_point].mvolt);
  Serial.println();
  */
  return line_approx(adc, calibrate_eeprom_data_->data[right_point - 1].adc, calibrate_eeprom_data_->data[right_point].adc,
                     calibrate_eeprom_data_->data[right_point - 1].mvolt, calibrate_eeprom_data_->data[right_point].mvolt);
}

uint16_t mv_to_adc(int mv, calibrate_eeprom_s * calibrate_data_) {
  uint16_t from_adc = 500;
  uint16_t to_adc = 1023;
  while (1) {
    uint16_t middle_adc = (from_adc + to_adc) / 2;
    if (middle_adc == from_adc || middle_adc == to_adc) return middle_adc;
    int middle_mv = adc_to_mv(middle_adc, calibrate_data_);
    int from_mv = adc_to_mv(from_adc, calibrate_data_);
    int to_mv = adc_to_mv(to_adc, calibrate_data_);
    if (mv > middle_mv) {
      from_adc = middle_adc;
      continue;
    };
    if (mv < middle_mv) {
      to_adc = middle_adc;
      continue;
    };
    return middle_adc;
  };
}

void do_add_calibrate(calibrate_eeprom_s * calibrate_eeprom_data_, bool * is_dirty, uint16_t adc, uint16_t mvolt) {
  for (uint8_t i = 0; i < calibrate_eeprom_data_->size; i++) {
    if (adc == calibrate_eeprom_data_->data[i].adc) {
      if (calibrate_eeprom_data_->data[i].mvolt != mvolt) *is_dirty = true;
      calibrate_eeprom_data_->data[i].mvolt = mvolt;
      return;
    };
    if (adc < calibrate_eeprom_data_->data[i].adc) {
      // insert here
      if (calibrate_eeprom_data_->size >= MAX_CALIBRATE) return;
      for (uint8_t j = calibrate_eeprom_data_->size; j > i; j--) {
        calibrate_eeprom_data_->data[j] = calibrate_eeprom_data_->data[j - 1];
      };
      calibrate_eeprom_data_->data[i] = { adc, mvolt };
      calibrate_eeprom_data_->size++;
      *is_dirty = true;
      return;
    }
  };
  // append to end
  if (calibrate_eeprom_data_->size >= MAX_CALIBRATE) return;
  calibrate_eeprom_data_->data[calibrate_eeprom_data_->size] = { adc, mvolt };
  calibrate_eeprom_data_->size++;
  *is_dirty = true;
}

void do_rm_calibrate(calibrate_eeprom_s * calibrate_eeprom_data_, bool * is_dirty, uint8_t idx) {
  if (calibrate_eeprom_data_->size <= 2) return;
  if (idx >= calibrate_eeprom_data_->size) return;
  for (uint8_t i = idx; i < calibrate_eeprom_data_->size - 1; i++) {
    calibrate_eeprom_data_->data[i] = calibrate_eeprom_data_->data[i + 1];
  };
  calibrate_eeprom_data_->size--;
  *is_dirty = true;
}

void getApprox(uint16_t * data, double *a, double *b, int n) {
  double sumx = 0;
  double sumy = 0;
  double sumx2 = 0;
  double sumxy = 0;
  for (int i = 0; i<n; i++) {
	  int ts = (i-n+1)*STEPX;
    sumx += (double)ts;
    sumy += (double)data[i];
    sumx2 += (double)ts * (double)ts;
    sumxy += (double)ts * (double)data[i];
  }
  *a = (n*sumxy - (sumx*sumy)) / (n*sumx2 - sumx*sumx);
  *b = (sumy - *a*sumx) / n;
  return;
}

#ifdef __cplusplus  
}
#endif
