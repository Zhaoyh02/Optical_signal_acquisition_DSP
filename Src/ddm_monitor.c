/**
  * @brief  DDM (Digital Diagnostics Monitoring) 光模块数字诊断监控
  *         模拟 SFP 光模块的 5 项诊断参数: 温度、电压、偏置电流、发射功率、接收功率
  *         当前版本为简化模拟 (固定温度/电压, TX/RX功率由占空比和信号强度推算)
  */
#include "ddm_monitor.h"
#include "laser_tx.h"
#include "laser_rx.h"

static ADC_HandleTypeDef *ddm_hadc;
static DDM_Params ddm_params;

void DDM_Init(ADC_HandleTypeDef *hadc) {
    ddm_hadc = hadc;
    ddm_params.temperature = (int16_t)(25.0f * 256.0f);  // 25°C, 以 1/256°C 为单位
    ddm_params.vcc = 33000;      // 3.3V, 以 100uV 为单位
    ddm_params.tx_bias = 0;      // 偏置电流(未实现)
    ddm_params.tx_power = 0;     // 发射功率(未实现)
    ddm_params.rx_power = 0;     // 接收功率(未实现)
}

// 更新DDM数据: 从 TX/RX 模块获取实时值
void DDM_Update(void) {
    ddm_params.temperature = (int16_t)(25.0f * 256.0f);
    ddm_params.vcc = 33000;

    // 发射功率: 由PWM占空比估算 (占空比50% → ~1000uW)
    uint8_t duty = LaserTx_GetDutyCycle();
    ddm_params.tx_power = (uint16_t)(duty * 20);

    // 接收功率: 由RMS信号电压映射到 0~65535 范围 (对应于 0~3.3V)
    float sig = LaserRx_GetSignalLevel();
    ddm_params.rx_power = (uint16_t)(sig / 3.3f * 65535.0f);
}

void DDM_GetParams(DDM_Params *out) {
    if (out) *out = ddm_params;
}

// 获取实际温度值 (°C)
float DDM_GetTemperatureCelsius(void) {
    return ddm_params.temperature / 256.0f;
}

// 获取实际电压值 (V)
float DDM_GetVCC(void) {
    return ddm_params.vcc * 100e-6f;
}
