#include "ball.h"
#include "k230.h"
#include "stepper.h"
#include <Arduino.h>

#define PID_KP        1.8f
#define PID_KD        140.0f
#define PID_KI        0.15f
#define INTEGRAL_MAX 1500.0f
#define BALL_FILTER   0.5f
#define DIR_SIGN      1.0f

#define ARRIVE_TOL_X   22
#define ARRIVE_HOLD_MS 100
#define OUT_MAX       800
#define SLW_MAX       80.0f
#define DERIV_MAX     3000.0f
#define BALL_LOST_LEVEL_MS 600

static int16_t  s_targetX   = 0;
static uint8_t  s_phase     = 0;
static float    s_ballF     = 0;
static bool     s_ballValid = false;
static float    s_int       = 0;
static float    s_lastErr   = 0;
static uint32_t s_arriveT0  = 0;
static bool     s_arrived   = false;
static uint32_t s_startT    = 0;
static bool     s_freshD    = true;
static uint32_t s_lostT0    = 0;
static float    s_lastOut   = 0.0f;

void Ball_Init() {
    s_phase = 0;
    s_targetX = X_CENTER;
    s_ballF = 0;
    s_ballValid = false;
    s_int = 0;
    s_lastErr = 0;
    s_arriveT0 = 0;
    s_arrived = false;
    s_freshD = true;
    s_lostT0 = 0;
    s_lastOut = 0;
    Stepper_SetTarget(0);
}

void Ball_Start() {
    s_phase = 1;
    s_targetX = X_PLUS5;
    s_ballF = 0;
    s_ballValid = false;
    s_int = 0;
    s_lastErr = 0;
    s_arrived = false;
    s_freshD = true;
    s_lostT0 = 0;
    s_lastOut = 0;
    s_startT = millis();

    Stepper_Enable(true);
    Stepper_SetTarget(0);
    Serial.printf("[BALL] Start: O -> +5cm(%.1fcm), EN=%d\n",
                  X_CM(X_PLUS5), 1);
}

void Ball_Stop() {
    s_phase = 0;
    Stepper_SetTarget(0);
}

void Ball_Update(int16_t ballX) {
#if BALL_DEBUG
    static uint32_t s_dbgT = 0;
    bool dbg = false;
    if (millis() - s_dbgT >= 200) { s_dbgT = millis(); dbg = true; }
#endif

    if (s_phase == 0 || s_phase == 3) {
#if BALL_DEBUG
        if (dbg) Serial.printf("[BALL] ph=%d X=%d(%.1fcm) idle\n",
                               s_phase, ballX, X_CM(ballX));
#endif
        return;
    }

    bool lost = (ballX == K230_LOST);
    if (lost) {
        if (s_lostT0 == 0) s_lostT0 = millis();

        if (millis() - s_lostT0 >= BALL_LOST_LEVEL_MS) {
            Stepper_SetTarget(0);
        }
        s_freshD = true;
    } else {
        s_lostT0 = 0;
    }
    bool freshD = s_freshD;
    s_freshD = false;

    float xf = s_ballF;
    if (!lost) {
        if (!s_ballValid || freshD) {
            s_ballValid = true;
            s_ballF = (float)ballX;
            xf = s_ballF;
        } else {
            s_ballF += BALL_FILTER * ((float)ballX - s_ballF);
            xf = s_ballF;
        }
    }

    bool inTol = !lost && (fabsf((float)ballX - s_targetX) <= ARRIVE_TOL_X);
    bool grace = lost && s_arrived;
    if (inTol || grace) {
        if (!s_arrived) {
            s_arrived = true;
            s_arriveT0 = millis();
        } else if (millis() - s_arriveT0 >= ARRIVE_HOLD_MS) {
            if (s_phase == 1) {
                s_phase = 2;
                s_targetX = X_MINUS5;
                s_int = 0;
                s_lastErr = 0;
                s_arrived = false;
                s_freshD = true;
                Serial.printf("[BALL] +5cm OK @%ums -> go -5cm\n",
                              (unsigned)(millis() - s_startT));
            } else if (s_phase == 2) {
                s_phase = 3;
                Serial.printf("[BALL] DONE @%ums, stable at -5cm\n",
                              (unsigned)(millis() - s_startT));
                Stepper_SetTarget(0);
            }
        }
    } else {
        s_arrived = false;
    }

    float err = 0.0f, out = 0.0f;
    if (!lost) {
        err = (float)(s_targetX) - xf;
        if (fabsf(err) < 220.0f) s_int += err;
        else s_int *= 0.9f;
        if (s_int >  INTEGRAL_MAX) s_int =  INTEGRAL_MAX;
        if (s_int < -INTEGRAL_MAX) s_int = -INTEGRAL_MAX;

        float derr = freshD ? 0.0f : (err - s_lastErr);
        if (derr >  DERIV_MAX) derr =  DERIV_MAX;
        if (derr < -DERIV_MAX) derr = -DERIV_MAX;

        out = DIR_SIGN * (PID_KP * err + PID_KD * derr + PID_KI * s_int);
        s_lastErr = err;

        if (out >  s_lastOut + SLW_MAX) out = s_lastOut + SLW_MAX;
        if (out <  s_lastOut - SLW_MAX) out = s_lastOut - SLW_MAX;
        if (out >  OUT_MAX) out =  OUT_MAX;
        if (out < -OUT_MAX) out = -OUT_MAX;
        s_lastOut = out;

#if !BALL_NO_MOTOR
        Stepper_SetTarget((int32_t)out);
#endif
    }

#if BALL_DEBUG
    if (dbg) {
        if (lost) {
            Serial.printf("[BALL] ph=%d X=LOST%s\n", s_phase,
                          s_arrived ? " (arrived-grace)" : " hold");
        } else {
            Serial.printf("[BALL] ph=%d X=%d(%.1fcm) T=%d(%.1fcm) err=%+.1f out=%+.0f pos=%d\n",
                          s_phase, ballX, X_CM(ballX),
                          s_targetX, X_CM(s_targetX), err, out,
                          (int)Stepper_GetSteps());
        }
    }
#endif
}

bool Ball_IsDone() {
    return s_phase == 3;
}

int16_t Ball_GetTargetX() {
    return s_targetX;
}
