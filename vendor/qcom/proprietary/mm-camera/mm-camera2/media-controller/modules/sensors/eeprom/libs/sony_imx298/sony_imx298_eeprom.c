/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
#include "sony_imx298_eeprom.h"

/** sony_imx298_eeprom_get_calibration_items:
 *    @e_ctrl: point to sensor_eeprom_data_t of the eeprom device
 *
 * Get calibration capabilities and mode items.
 *
 * This function executes in eeprom module context
 *
 * Return: void.
 **/
void sony_imx298_eeprom_get_calibration_items(void *e_ctrl)
{
  sensor_eeprom_data_t *ectrl = (sensor_eeprom_data_t *)e_ctrl;
  eeprom_calib_items_t *e_items = &(ectrl->eeprom_data.items);

  e_items->is_wbc = awb_present ? TRUE : FALSE;
  e_items->is_afc = af_present ? TRUE : FALSE;
  e_items->is_lsc = lsc_present ? TRUE : FALSE;
  e_items->is_dpc = FALSE;
  e_items->is_insensor = spc_present ? TRUE : FALSE;
  e_items->is_ois = FALSE;

  SLOW("is_wbc:%d,is_afc:%d,is_lsc:%d,is_dpc:%d,is_insensor:%d,\
  is_ois:%d",e_items->is_wbc,e_items->is_afc,
    e_items->is_lsc,e_items->is_dpc,e_items->is_insensor,
    e_items->is_ois);
}

/** sony_imx298_eeprom_format_wbdata:
 *    @e_ctrl: point to sensor_eeprom_data_t of the eeprom device
 *
 * Format the data structure of white balance calibration
 *
 * This function executes in eeprom module context
 *
 * Return: void.
 **/
static void sony_imx298_eeprom_format_wbdata(
        sensor_eeprom_data_t *e_ctrl)
{
  unsigned char flag;
  module_info_t *module_info;
  awb_data_t    *wb;
  float         r_over_gr, b_over_gb, gr_over_gb;
  int           i;

  SLOW("Enter");
  /* Check validity */
  flag = e_ctrl->eeprom_params.buffer[AWB_FLAG_OFFSET];
  if (flag != VALID_FLAG) {
    awb_present = FALSE;
    SERR("AWB : empty or invalid data");
    return;
  }
  awb_present = TRUE;

  /* Print module info */
  flag = e_ctrl->eeprom_params.buffer[MODULE_INFO_FLAG_OFFSET];
  if (flag == VALID_FLAG) {
    module_info = (module_info_t*)&e_ctrl->eeprom_params.buffer[MODULE_INFO_OFFSET];
    SLOW("Module ID: %d", module_info->module_id);
    SLOW("Lens ID: %d", module_info->lens_id);
    SLOW("Vcm ID: %d", module_info->vcm_id);
    SLOW("Driver IC ID: %d", module_info->driver_ic_id);
    SLOW("Y/M/D: %d/%d/%d", module_info->year + 2000, module_info->month, module_info->day);
  } else {
    SHIGH("MODULE_INFO : empty or invalid data");
  }

  /* Get AWB data */
  wb = (awb_data_t *)(e_ctrl->eeprom_params.buffer + AWB_OFFSET);

  r_over_gr = ((float)((wb->r_over_gr_h << 8) | wb->r_over_gr_l)) / QVALUE;
  b_over_gb = ((float)((wb->b_over_gb_h << 8) | wb->b_over_gb_l)) / QVALUE;
  gr_over_gb = ((float)((wb->gr_over_gb_h << 8) | wb->gr_over_gb_l)) / QVALUE;

  SLOW("AWB : r/gr = %f", r_over_gr);
  SLOW("AWB : b/gb = %f", b_over_gb);
  SLOW("AWB : gr/gb = %f", gr_over_gb);

  for (i = 0; i < AGW_AWB_MAX_LIGHT; i++) {
    e_ctrl->eeprom_data.wbc.r_over_g[i] = r_over_gr;
    e_ctrl->eeprom_data.wbc.b_over_g[i] = b_over_gb;
  }
  e_ctrl->eeprom_data.wbc.gr_over_gb = gr_over_gb;
  SLOW("Exit");
}


/** sony_imx298_eeprom_format_afdata:
 *    @e_ctrl: point to sensor_eeprom_data_t of the eeprom device
 *
 * Format the data structure of white balance calibration
 *
 * This function executes in eeprom module context
 *
 * Return: void.
 **/
static void sony_imx298_eeprom_format_afdata(
         sensor_eeprom_data_t *e_ctrl)
{
  unsigned char    flag;
  af_data_t        *af;
  SLOW("Enter");
  /* Check validity */
  flag = e_ctrl->eeprom_params.buffer[AF_FLAG_OFFSET];
  if (flag != VALID_FLAG) {
  /* need to call autofocus caliberation to boost up code_per_step
     invalid AF EEPROM data will not be consumed by af parameters
  */
    af_present = FALSE;
    SERR("AF : empty or invalid data");
    return;
  }
  af_present = TRUE;
  /* Get AF data */
  af = (af_data_t *)(e_ctrl->eeprom_params.buffer + AF_OFFSET);

  e_ctrl->eeprom_data.afc.macro_dac = ((af->macro_h << 8) | af->macro_l);
  e_ctrl->eeprom_data.afc.infinity_dac = ((af->infinity_h << 8) | af->infinity_l);
  e_ctrl->eeprom_data.afc.starting_dac = e_ctrl->eeprom_data.afc.infinity_dac;

  SHIGH("AF : macro %d infinity %d (no starting DAC set to infinity)",
    e_ctrl->eeprom_data.afc.macro_dac, e_ctrl->eeprom_data.afc.infinity_dac);
  SLOW("Exit");
}

/** sony_imx298_eeprom_format_lscdata:
 *    @e_ctrl: point to sensor_eeprom_data_t of the eeprom device
 *
 * Format the data structure of lens sharding calibration
 *
 * This function executes in eeprom module context
 *
 * Return: void.
 **/
static void sony_imx298_eeprom_format_lscdata(
        sensor_eeprom_data_t *e_ctrl)
{
  unsigned char  flag = 0;
  unsigned char  *lsc = NULL;
  unsigned int   index = 0;

  SLOW("Enter");
  /* Check validity */
  flag = e_ctrl->eeprom_params.buffer[LSC_FLAG_OFFSET];
  if (flag != VALID_FLAG) {
    lsc_present = FALSE;
    SERR("LSC : empty or invalid data");
    return;
  }
  lsc_present = TRUE;

  /* Get LSC data */
  lsc = e_ctrl->eeprom_params.buffer + LSC_OFFSET;

  g_reg_array[g_reg_setting.size].reg_addr = 0x0100;
  g_reg_array[g_reg_setting.size].reg_data = 0x00;
  g_reg_setting.size++;

  g_reg_array[g_reg_setting.size].reg_addr = 0x0b00;
  g_reg_array[g_reg_setting.size].reg_data = 0x01;
  g_reg_setting.size++;

  g_reg_array[g_reg_setting.size].reg_addr = 0x30b0;
  g_reg_array[g_reg_setting.size].reg_data = 0x02;
  g_reg_setting.size++;

  for(index = 0; index < LSC_GRID_SIZE; index ++) {
    g_reg_array[g_reg_setting.size].reg_addr = 0xE800 + index;

    if(0 == index % 2)
      g_reg_array[g_reg_setting.size].reg_data = lsc[index] & 0x03;
    else
      g_reg_array[g_reg_setting.size].reg_data = lsc[index];

    g_reg_setting.size++;
  }

  g_reg_array[g_reg_setting.size].reg_addr = 0x30b1;
  g_reg_array[g_reg_setting.size].reg_data = 0x00;
  g_reg_setting.size++;
}

static int sony_imx298_eeprom_get_raw_data(void *e_ctrl, void *data)
{
  SLOW("Enter");
  if (e_ctrl && data)
    memcpy(data, &g_reg_setting, sizeof(g_reg_setting));
  else {
    SERR("failed Null pointer");
    return SENSOR_FAILURE;
  }
  SLOW("Exit");
  return SENSOR_SUCCESS;
}

/** sony_imx298_eeprom_format_spcdata:
 *    @e_ctrl: point to sensor_eeprom_data_t of the eeprom device
 *
 * Format the data structure of pdaf calibration:
 *   shield pixel correction (SPC)
 *
 * This function executes in eeprom module context
 *
 * Return: void.
 **/
static void sony_imx298_eeprom_format_spcdata(sensor_eeprom_data_t *e_ctrl)
{
  unsigned char  flag;
  unsigned char *buffer = e_ctrl->eeprom_params.buffer;
  unsigned int spc_offset = SPC_OFFSET;
  unsigned int i = 0;
  SLOW("Enter");
  /* Check validity */
  flag = e_ctrl->eeprom_params.buffer[SPC_FLAG_OFFSET];
  if (flag != VALID_FLAG) {
    spc_present = FALSE;
    SERR("SPC : empty or invalid data");
    return;
  }
  spc_present = TRUE;

  for(i = 0; i < SPC_SETTING_SIZE; i++) {
    g_reg_array[i].reg_addr = SPC_ADDR + i;
    g_reg_array[i].reg_data = buffer[spc_offset + i];
    g_reg_array[i].delay = 0;
    g_reg_setting.size++;

    SLOW("OTP: SPCData[%d]: addr: 0x%X, data: 0x%X",
      i, g_reg_array[i].reg_addr, g_reg_array[i].reg_data);
  }
  SLOW("Exit");
}

/** sony_imx298_eeprom_format_dccdata:
 *    @e_ctrl: point to sensor_eeprom_data_t of the eeprom device
 *
 * Format the data structure of pdaf calibration:
 *   defocus conversion coefficient  (DCC)
 *
 * This function executes in eeprom module context
 *
 * Return: void.
 **/
static void sony_imx298_eeprom_format_dccdata(sensor_eeprom_data_t *e_ctrl)
{
  unsigned char  flag;
  unsigned char *buffer = e_ctrl->eeprom_params.buffer;
  unsigned int pdaf_offset = DCC_OFFSET;
  unsigned int i = 0;
  pdafcalib_data_t *pdaf_data = &e_ctrl->eeprom_data.pdafc;

  SLOW("Enter");
  /* Check validity */
  flag = e_ctrl->eeprom_params.buffer[DCC_FLAG_OFFSET];
  if (flag != VALID_FLAG) {
    dcc_present = FALSE;
    SERR("DCC : empty or invalid data");
    return;
  }
  dcc_present = TRUE;

  pdaf_data->XKnotNumSlopeOffset = PD_KNOT_X;
  pdaf_data->YKnotNumSlopeOffset = PD_KNOT_Y;

  for(i = 0;i < PD_KNOT_X * PD_KNOT_Y;i++) {
    /*SlopeData format: u6.10 */
    pdaf_data->SlopeData[i] = (float)(buffer[pdaf_offset + i * 2] << 8 |
      buffer[pdaf_offset + i * 2 + 1]) / 1024.0;
    pdaf_data->OffsetData[i] = 0;
    SLOW("OTP SlopeData[%d]: u6.10: 0x%x, float: %f", i,
     (buffer[pdaf_offset + i * 2] << 8 | buffer[pdaf_offset + i * 2 + 1]),
     pdaf_data->SlopeData[i]);
  }

  for(i = 0;i < PD_KNOT_X;i++)
    pdaf_data->XAddressKnotSlopeOffset[i] = PD_OFFSET_X + PD_AREA_X * i;

  for(i = 0;i < PD_KNOT_Y;i++)
    pdaf_data->YAddressKnotSlopeOffset[i] = PD_OFFSET_Y + PD_AREA_Y * i;
  SLOW("Exit");
}

/** sony_imx298_eeprom_format_calibration_data:
 *    @e_ctrl: point to sensor_eeprom_data_t of the eeprom device
 *
 * Format all the data structure of calibration
 *
 * This function executes in eeprom module context and generate
 *   all the calibration registers setting of the sensor.
 *
 * Return: void.
 **/
void sony_imx298_eeprom_format_calibration_data(void *e_ctrl)
{
  sensor_eeprom_data_t * ctrl = (sensor_eeprom_data_t *)e_ctrl;
  if(ctrl != NULL)
    SLOW("Total bytes in OTP buffer: %d", ctrl->eeprom_params.num_bytes);
  else {
    SERR("failed, e_ctrl is NULL");
    return;
  }

  if (!ctrl->eeprom_params.buffer || !ctrl->eeprom_params.num_bytes) {
    SERR("Buff pointer %p buffer size %d", ctrl->eeprom_params.buffer,
      ctrl->eeprom_params.num_bytes);
    return;
  }

  g_reg_setting.addr_type = CAMERA_I2C_WORD_ADDR;
  g_reg_setting.data_type = CAMERA_I2C_BYTE_DATA;
  g_reg_setting.reg_setting = &g_reg_array[0];
  g_reg_setting.size = 0;
  g_reg_setting.delay = 0;

  sony_imx298_eeprom_format_wbdata(ctrl);
  sony_imx298_eeprom_format_afdata(ctrl);
  sony_imx298_eeprom_format_lscdata(ctrl);
  sony_imx298_eeprom_format_spcdata(ctrl);
  sony_imx298_eeprom_format_dccdata(ctrl);
}


/** sony_imx298_eeprom_eeprom_open_lib:
 *
 * Get the funtion pointer of this lib.
 *
 * This function executes in eeprom module context.
 *
 * Return: eeprom_lib_func_t point to the function pointer.
 **/
void* sony_imx298_eeprom_open_lib(void) {
  return &sony_imx298_eeprom_lib_func_ptr;
}
