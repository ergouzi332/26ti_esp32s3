#include "ball.h"
#include "k230.h"
#include "stepper.h"
#include "web.h"

#include <Arduino.h>

#define PID_KP        1.9f
#define PID_KD        140.0f
#define PID_KD2       100.0f
#define PID_KD2_END   200.0f
#define PID_KI        0.15f

#define Q4_KP         1.5f
#define Q4_KD         55.0f
#define Q4_KI         0.12f
#define Q4_OUT_MAX    1000
#define Q4_SLW        70.0f
#define INTEGRAL_MAX 1500.0f
#define Q4_INT_MAX   1600.0f
#define Q4_BIAS       120.0f
#define Q4_BIAS_MS    3200
#define Q4_BIAS_PEAK_MS 1300
#define Q4_BIAS_HOLD  450
#define BALL_FILTER   0.5f
#define DIR_SIGN      1.0f

#define ARRIVE_TOL_X   36
#define ARRIVE_HOLD_MS 80
#define OUT_MAX       1000
#define SLW_MAX       120.0f
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
static bool     s_holdLock  = false; // ph3: ???????
static uint32_t s_holdT0    = 0;     // ph3: ????
static uint32_t s_doneT     = 0;     // ph3: DONE ??
static uint32_t s_phase2T   = 0;     // ph2: entry time
static int32_t  s_q4Home    = 0;     // ph4: center-hold steps
static bool     s_q4BiasDone= false;   // ph4: bias-window flag
static uint32_t s_q4T0      = 0;     // ph4: start time


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
    s_holdLock = false;
    s_holdT0 = 0;
    s_doneT = 0;
    s_phase2T = 0;
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
    s_holdLock = false;
    s_holdT0 = 0;
    s_doneT = 0;
    s_phase2T = 0;
    s_startT = millis();

    Stepper_Enable(true);
    Stepper_SetTarget(0);
    Web_Logf("[BALL] Start: O -> +5cm(%.1fcm), EN=%d\n",
                  X_CM(X_PLUS5), 1);
}

void Ball_StartQ4() {
    s_phase = 4;
    s_targetX = X_CENTER;
    s_ballF = 0;
    s_ballValid = false;
    s_int = 0;
    s_lastErr = 0;
    s_arrived = false;
    s_freshD = true;
    s_lostT0 = 0;
    s_lastOut = 0;
    s_holdLock = false;
    s_holdT0 = 0;
    s_doneT = 0;
    s_phase2T = 0;
    s_q4Home = Stepper_GetSteps();
    s_q4T0 = millis();
    s_q4BiasDone = false;

    Stepper_Enable(true);
    Stepper_SetTarget(Stepper_GetSteps());
    Web_Logf("[BALL] Q4 hold center");
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

    if (s_phase == 0) {
#if BALL_DEBUG
        if (dbg) Serial.printf("[BALL] ph=%d X=%d(%.1fcm) idle\n",
                               s_phase, ballX, X_CM(ballX));
#endif
        return;
    }
    if (s_phase == 3 && s_holdLock) {
#if BALL_DEBUG
        if (dbg) Serial.printf("[BALL] ph=3 LOCK X=%d(%.1fcm) pos=%d\n",
                               ballX, X_CM(ballX), (int)Stepper_GetSteps());
#endif
        return;
    }

    bool lost = (ballX == K230_LOST);
    if (lost) {
        if (s_lostT0 == 0) s_lostT0 = millis();
        if (s_phase == 4) {
            uint32_t el = millis() - s_q4T0;
            float bf = 0.0f;
            if (el < Q4_BIAS_PEAK_MS) bf = 1.0f;
            else if (el < Q4_BIAS_MS) bf = 1.0f - (float)(el - Q4_BIAS_PEAK_MS) / (float)(Q4_BIAS_MS - Q4_BIAS_PEAK_MS);
            if (bf > 0.0f)
                Stepper_SetTarget((int32_t)(Q4_BIAS_HOLD * bf));
            else
                Stepper_SetTarget(s_q4Home);
        }

        bool nearTarget = s_ballValid &&
                          (fabsf(s_ballF - s_targetX) <= 60.0f);
        if (nearTarget && !s_arrived) {
            s_arrived = true;
            s_arriveT0 = millis();
        } else if (!nearTarget &&
                   millis() - s_lostT0 >= BALL_LOST_LEVEL_MS) {
            if (s_phase != 4 && s_phase != 3) Stepper_SetTarget(0);
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

    bool inTol = !lost && (fabsf(xf - s_targetX) <= ARRIVE_TOL_X);
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
                s_phase2T = millis();
                Web_Logf("[BALL] +5cm OK @%ums -> go -5cm\n",
                              (unsigned)(millis() - s_startT));
            } else if (s_phase == 2) {
                s_phase = 3;
                s_doneT = millis();
                s_int = 0;
                Web_Logf("[BALL] DONE @%ums, stable at -5cm\n",
                              (unsigned)(millis() - s_startT));
            }
        }
    } else {
        s_arrived = false;
    }

    if (s_phase == 2 && (millis() - s_phase2T) >= 3000) {
        s_phase = 3;
        s_doneT = millis();
        s_int = 0;
        Web_Logf("[BALL] DONE @%ums (timeout)", (unsigned)(millis() - s_startT));
    }

    float err = 0.0f, out = 0.0f;
    if (!lost) {
        err = (float)(s_targetX) - xf;
        if (s_phase == 4) {
            uint32_t el = millis() - s_q4T0;
            if (el >= Q4_BIAS_MS) {
                if (!s_q4BiasDone) { s_q4BiasDone = true; s_int = 0.0f; }
                if (fabsf(err) < 10.0f) { err = 0.0f; s_q4Home = (int32_t)s_lastOut; }
            }
            if (el < Q4_BIAS_PEAK_MS) err += Q4_BIAS;
            else if (el < Q4_BIAS_MS) err += Q4_BIAS * (1.0f - (float)(el - Q4_BIAS_PEAK_MS) / (float)(Q4_BIAS_MS - Q4_BIAS_PEAK_MS));
        }
        if (s_phase == 3) {
            if (fabsf(err) <= 40.0f) s_int += err;
            else s_int *= 0.9f;
        } else if (fabsf(err) < 220.0f) {
            s_int += err;
        } else {
            s_int *= 0.9f;
        }
        float imax = (s_phase == 4) ? Q4_INT_MAX : INTEGRAL_MAX;
        if (s_int >  imax) s_int =  imax;
        if (s_int < -imax) s_int = -imax;

        float derr = freshD ? 0.0f : (err - s_lastErr);
        if (derr >  DERIV_MAX) derr =  DERIV_MAX;
        if (derr < -DERIV_MAX) derr = -DERIV_MAX;

        float kp = (s_phase == 4) ? Q4_KP : PID_KP;
        float kd = (s_phase == 4) ? Q4_KD
                 : ((s_phase == 2 && fabsf(err) > PID_KD2_END) ? PID_KD2 : PID_KD);
        float ki = (s_phase == 4) ? Q4_KI : PID_KI;
        out = DIR_SIGN * (kp * err + kd * derr + ki * s_int);
        s_lastErr = err;

        float slw = (s_phase == 4) ? Q4_SLW : SLW_MAX;
        if (out >  s_lastOut + slw) out = s_lastOut + slw;
        if (out <  s_lastOut - slw) out = s_lastOut - slw;
        float omax = (s_phase == 4) ? Q4_OUT_MAX : OUT_MAX;
        if (out >  omax) out =  omax;
        if (out < -omax) out = -omax;
        s_lastOut = out;

#if !BALL_NO_MOTOR
        Stepper_SetTarget((int32_t)out);
#endif
    }

    if (s_phase == 3 && !s_holdLock) {
        bool inBand = !lost && (fabsf(err) <= 35.0f);
        if (inBand) {
            if (s_holdT0 == 0) s_holdT0 = millis();
            if (millis() - s_holdT0 >= 800) {
                s_holdLock = true;
                Stepper_SetTarget((int32_t)s_lastOut);
                Web_Logf("[BALL] HOLD locked @ -5cm, motor still");
            }
        } else {
            s_holdT0 = 0;
        }
        if (millis() - s_doneT >= 3000) {
            s_holdLock = true;
            Stepper_SetTarget((int32_t)s_lastOut);
            Web_Logf("[BALL] HOLD locked (force), motor still");
        }
    }

#if BALL_DEBUG
    if (dbg) {
        if (lost) {
            Web_Logf("[BALL] ph=%d X=LOST%s\n", s_phase,
                          s_arrived ? " (arrived-grace)" : " hold");
        } else {
            Web_Logf("[BALL] ph=%d X=%d(%.1fcm) T=%d(%.1fcm) err=%+.1f out=%+.0f pos=%d\n",
                          s_phase, ballX, X_CM(ballX),
                          s_targetX, X_CM(s_targetX), err, out,
                          (int)Stepper_GetSteps());
        }
    }
#endif
}

uint8_t Ball_GetPhase() {
    return s_phase;
}

bool Ball_IsDone() {
    return s_phase == 3;
}

int16_t Ball_GetTargetX() {
    return s_targetX;
}
