#ifndef _STEPPER_H_
#define _STEPPER_H_

#include <stdint.h>

// ============ 脉冲接口（接 ZDT 闭环驱动板 PUL/DIR/EN/COM）============
#define PIN_STP   6   // PUL 脉冲
#define PIN_DIR   7   // DIR 方向
#define PIN_EN    15  // EN 使能（本工程按低电平使能接线，与驱动板菜单 En=L 对应）
#define PIN_COM   16  // 脉冲共阳/共阴公共端，按实际接线固定电平

// ============ 行程保护 ============
#define STEP_MIN   (-5000)
#define STEP_MAX   ( 5000)

// ============ 最大脉冲频率 ============
// 驱动板 16 细分 = 3200 脉冲/圈。此值是定时器 ISR 的脉冲上限：
//   2000 = 0.6 圈/秒(慢)  4000 = 1.25 圈/秒(默认)  8000 = 2.5 圈/秒(快)
// 调大前确认机构能承受，太快会丢步/撞限位。
#define STEP_HZ    4000

// 硬件定时器自动发脉冲：调用 Stepper_SetTarget() 后无需再调用 Update()
void Stepper_Init();
void Stepper_Enable(bool en);            // true=使能(锁轴)  false=失能(松轴)
int32_t Stepper_GetSteps();              // 当前已发出脉冲数(带方向)
void Stepper_SetTarget(int32_t target);  // 目标位置(脉冲数)，内部限幅
void Stepper_Update();                   // 兼容旧接口（定时器模式为空操作）
bool Stepper_AtTarget();                 // 是否已到位

#endif