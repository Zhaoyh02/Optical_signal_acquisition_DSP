/**
  * @brief  APC (Automatic Power Control) 自动功率控制 PI 控制器
  *         用于稳定激光发射功率: 根据实测功率与目标功率的偏差,
  *         通过 PI 算法调整 PWM 占空比输出
  *         公式: output = Kp * error + Ki * integral(error * dt)
  *         输出范围限制在 [output_min, output_max] (0~90%占空比)
  */
#include "apc_control.h"

void APC_Init(APC_Controller *apc, float kp, float ki, float setpoint) {
    apc->kp = kp;
    apc->ki = ki;
    apc->setpoint = setpoint;              // 目标功率
    apc->integral = 0.0f;
    apc->integral_max = 30.0f;             // 积分限幅，防止积分饱和
    apc->output_min = 0.0f;                // 输出下限 (PWM占空比=0%)
    apc->output_max = 90.0f;               // 输出上限 (PWM占空比=90%, 保护激光器)
    apc->enabled = 0;
}

// PI 控制器更新: 输入实测值和步长 dt(秒)，返回 PWM 占空比
float APC_Update(APC_Controller *apc, float measured, float dt) {
    if (!apc->enabled) return apc->setpoint;

    float error = apc->setpoint - measured;
    apc->integral += apc->ki * error * dt;
    // 积分限幅，防止 wind-up 饱和
    if (apc->integral > apc->integral_max) apc->integral = apc->integral_max;
    if (apc->integral < -apc->integral_max) apc->integral = -apc->integral_max;

    float output = apc->kp * error + apc->integral;
    // 输出限幅
    if (output > apc->output_max) output = apc->output_max;
    if (output < apc->output_min) output = apc->output_min;

    return output;
}

void APC_Enable(APC_Controller *apc)  { apc->enabled = 1; }
void APC_Disable(APC_Controller *apc) { apc->enabled = 0; }
void APC_SetSetpoint(APC_Controller *apc, float setpoint) { apc->setpoint = setpoint; }
void APC_Reset(APC_Controller *apc) { apc->integral = 0.0f; }
