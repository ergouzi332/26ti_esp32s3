#ifndef _BALL_H_
#define _BALL_H_

#include <stdint.h>

// ============================================================
// ==== 坐标标定 2026-07-31 =====
// 摆杆总长25cm，中心 O 为 0cm（图片像素 X）
//   中心O(12.5cm) -> X = -57
//   +5cm(17.5cm)  -> X = -269
//   -5cm(7.5cm)   -> X = +151
// X_CM(x) 把像素换算成 cm（以标定点线性插值）
// ============================================================
#define X_CENTER    (-49)
#define X_PLUS5     (-272)
#define X_MINUS5    ( 162)

#define X_CM(x)  (((float)(x) - X_CENTER) / ((float)(X_PLUS5 - X_MINUS5) / 10.0f))

// ============================================================
// ==== 工作模式开关 ====
// STEPPER_TEST_MODE 1 = 步进电机手动测试(串口 a/s/z/e/d) 0 = 跑第三问控制
// BALL_DEBUG        1 = 每200ms打印控制量            0 = 关闭
// BALL_NO_MOTOR     1 = 只采集不转电机(调试用)       0 = 正常控制
// BALL_AUTO_START   1 = 上电自动开始(联调用)         0 = 等 TI/USB 命令
// ============================================================
#define STEPPER_TEST_MODE 0
#define BALL_DEBUG       1
#define BALL_NO_MOTOR    0
#define BALL_AUTO_START  0

void Ball_Init();
void Ball_Start();               // 第三问：O -> +5cm -> -5cm
void Ball_StartQ4();
void Ball_Stop();                // 停止（摆杆回水平）
void Ball_Update(int16_t ballX); // 每20ms调用：钢球位置闭环
bool Ball_IsDone();              // 是否已稳定在 -5cm
int16_t Ball_GetTargetX();       // 当前目标像素位置

uint8_t Ball_GetPhase();

#endif
