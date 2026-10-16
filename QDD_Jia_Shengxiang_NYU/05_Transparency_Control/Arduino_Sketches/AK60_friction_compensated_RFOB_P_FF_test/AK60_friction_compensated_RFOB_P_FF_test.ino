#include <Arduino.h>
#include <FlexCAN_T4.h>
#include <math.h>

// ============================================================
// AK60 RFOB ZERO-TORQUE + FRICTION COMPENSATION
//
// Motor ID = 3
//
// Base controller:
//
//   tau_ref = 0 Nm
//   tau_reac_hat = RFOB estimate
//
//   e_tau = 0 - tau_reac_hat
//
//   tau_RFOB = Kp * e_tau
//
// Added friction compensation:
//
//   Positive:
//     tau_fric = Fc_p * tanh(Ks*w) + B_p*w
//
//   Negative:
//     tau_fric = Fc_n * tanh(Ks*w) + B_n*w
//
// Friction-compensated RFOB:
//
//   tau_reac_hat = Q[ Kt*Iq - J*alpha - tau_f_model ]
//
// Final command:
//
//   tau_cmd = tau_RFOB_control + FRICTION_GAIN*tau_f_model
// s = enable RFOB + friction compensation
// x = disable / zero torque
// ============================================================


// ============================================================
// CAN
// ============================================================

FlexCAN_T4<CAN1, RX_SIZE_256, TX_SIZE_16> CanBus;

constexpr uint32_t CAN_BAUD = 1000000;
constexpr uint8_t MOTOR_ID = 3;

constexpr uint32_t MIT_COMMAND_ID =
    (8UL << 8) | MOTOR_ID;       // 0x803

constexpr uint32_t FEEDBACK_ID =
    (0x29UL << 8) | MOTOR_ID;    // 0x2903


// ============================================================
// AK60 MIT RANGE
// ============================================================

constexpr float P_MIN  = -12.56f;
constexpr float P_MAX  =  12.56f;

constexpr float V_MIN  = -60.0f;
constexpr float V_MAX  =  60.0f;

constexpr float T_MIN  = -12.0f;
constexpr float T_MAX  =  12.0f;

constexpr float KP_MIN = 0.0f;
constexpr float KP_MAX = 500.0f;

constexpr float KD_MIN = 0.0f;
constexpr float KD_MAX = 5.0f;


// ============================================================
// MOTOR / RFOB PARAMETERS
// ============================================================

// Identified output-side inertia
constexpr float J_NOMINAL =
    0.0042243000f;

// Torque constant
constexpr float KT_NOMINAL =
    0.5994f;

constexpr float MOTOR_POLE_PAIRS =
    14.0f;

constexpr float GEAR_RATIO =
    6.0f;


// ============================================================
// IDENTIFIED FRICTION PARAMETERS
// ============================================================

// Static / breakaway friction.
//
// Retained for reference.
// NOT directly injected in this version.
constexpr float TS_P_NM =
    0.203796f;

constexpr float TS_N_NM =
    0.209790f;


// Dynamic Coulomb friction
constexpr float FC_P_NM =
    0.1388340675f;

constexpr float FC_N_NM =
    0.1358578172f;


// Dynamic viscous friction
constexpr float B_P_NMS_RAD =
    0.0014878030f;

constexpr float B_N_NMS_RAD =
    0.0014533901f;


// ============================================================
// FRICTION COMPENSATION SETTINGS
// ============================================================

// Smooth sign transition.
//
// At w = 0.05 rad/s:
// tanh(20*0.05) ~= 0.762
//
// At w = 0.10 rad/s:
// tanh(20*0.10) ~= 0.964
constexpr float FRICTION_KS =
    20.0f;


// Start with full identified dynamic friction.
//
// If necessary this can later be adjusted,
// but first test the identified model itself.
constexpr float FRICTION_GAIN =
    1.0f;


// Very small zero-speed deadband.
//
// Prevents encoder-speed noise from producing alternating
// compensation torque at exact standstill.
constexpr float FRICTION_ZERO_SPEED_RAD_S =
    0.005f;


// Individual friction compensation clamp.
constexpr float FRICTION_LIMIT_NM =
    0.25f;


// ============================================================
// RFOB FILTER
// ============================================================

constexpr float ACCEL_CUTOFF_HZ =
    15.0f;

constexpr float RFOB_CUTOFF_HZ =
    20.0f;


// ============================================================
// ZERO-TORQUE CONTROLLER
// ============================================================

constexpr float ZERO_TORQUE_KP =
    1.0f;


// Final motor torque command limit.
//
// This applies AFTER:
//   RFOB torque + friction compensation
constexpr float TORQUE_LIMIT_NM =
    1.0f;


// If increasing Kp makes motor harder to back-drive,
// change +1 to -1.
constexpr float CONTROL_SIGN =
    1.0f;


// ============================================================
// TIMING
// ============================================================

constexpr uint32_t COMMAND_PERIOD_US =
    1000;

constexpr uint32_t FEEDBACK_TIMEOUT_US =
    20000;


// ============================================================
// MOTOR STATE
// ============================================================

struct MotorState
{
    bool valid = false;

    float positionDeg = 0.0f;
    float positionRad = 0.0f;

    float speedErpm = 0.0f;
    float speedRadS = 0.0f;

    float iqA = 0.0f;

    uint8_t errorCode = 0;

    uint32_t lastFeedbackUs = 0;
};

MotorState motor;


// ============================================================
// RFOB STATE
// ============================================================

bool rfobInitialized = false;

float previousVelocityRadS =
    0.0f;

float rawAccelerationRadS2 =
    0.0f;

float filteredAccelerationRadS2 =
    0.0f;

float motorTorqueNm =
    0.0f;

float rawReactionTorqueNm =
    0.0f;

float estimatedReactionTorqueNm =
    0.0f;

uint32_t previousRfobUs =
    0;


// ============================================================
// CONTROL STATE
// ============================================================

bool controlEnabled =
    false;

float torqueReferenceNm =
    0.0f;

float torqueErrorNm =
    0.0f;


// RFOB P-controller contribution
float rfobControlTorqueNm =
    0.0f;


// Physical identified friction-model torque.
//
// This is the quantity subtracted inside the RFOB model.
// IMPORTANT:
// It is NOT multiplied by FRICTION_GAIN.
float frictionModelTorqueNm =
    0.0f;


// Existing raw-friction log value.
// Kept for compatibility with your current CSV/logging.
float frictionRawTorqueNm =
    0.0f;


// Friction feedforward after gain / clamp
float frictionCompTorqueNm =
    0.0f;


// Final MIT torque command
float torqueCommandNm =
    0.0f;


// ============================================================
// TIMERS
// ============================================================

elapsedMicros commandTimer;

uint32_t lastPrintUs =
    0;


// ============================================================
// HELPERS
// ============================================================

float degToRadF(float deg)
{
    return deg
        * PI
        / 180.0f;
}


float erpmToOutputRadS(float erpm)
{
    const float outputRpm =
        erpm
        / MOTOR_POLE_PAIRS
        / GEAR_RATIO;

    return outputRpm
        * (2.0f * PI / 60.0f);
}


float lowPassAlpha(
    float cutoffHz,
    float dt)
{
    const float tau =
        1.0f
        / (2.0f * PI * cutoffHz);

    return dt
        / (tau + dt);
}


uint16_t floatToUint(
    float x,
    float xMin,
    float xMax,
    uint8_t bits)
{
    x =
        constrain(
            x,
            xMin,
            xMax);

    const float span =
        xMax - xMin;

    const uint32_t maxInt =
        (1UL << bits) - 1UL;

    return static_cast<uint16_t>(
        (x - xMin)
        * static_cast<float>(maxInt)
        / span);
}


// ============================================================
// MIT COMMAND
//
// UNCHANGED FROM YOUR WORKING CODE
// ============================================================

bool sendMIT(
    float p,
    float v,
    float kp,
    float kd,
    float torque)
{
    const uint16_t pInt =
        floatToUint(
            p,
            P_MIN,
            P_MAX,
            16);

    const uint16_t vInt =
        floatToUint(
            v,
            V_MIN,
            V_MAX,
            12);

    const uint16_t kpInt =
        floatToUint(
            kp,
            KP_MIN,
            KP_MAX,
            12);

    const uint16_t kdInt =
        floatToUint(
            kd,
            KD_MIN,
            KD_MAX,
            12);

    const uint16_t tInt =
        floatToUint(
            torque,
            T_MIN,
            T_MAX,
            12);


    CAN_message_t msg{};

    msg.id =
        MIT_COMMAND_ID;

    msg.flags.extended =
        1;

    msg.flags.remote =
        0;

    msg.len =
        8;


    msg.buf[0] =
        kpInt >> 4;

    msg.buf[1] =
        ((kpInt & 0x0F) << 4)
        | ((kdInt >> 8) & 0x0F);

    msg.buf[2] =
        kdInt & 0xFF;

    msg.buf[3] =
        pInt >> 8;

    msg.buf[4] =
        pInt & 0xFF;

    msg.buf[5] =
        vInt >> 4;

    msg.buf[6] =
        ((vInt & 0x0F) << 4)
        | ((tInt >> 8) & 0x0F);

    msg.buf[7] =
        tInt & 0xFF;


    return CanBus.write(msg);
}


void sendZeroTorque()
{
    sendMIT(
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        0.0f);
}


// ============================================================
// FRICTION COMPENSATION
//
// Uses dynamic friction parameters.
//
// Positive:
//
//   tau_f = Fc+ * tanh(Ks*w) + B+*w
//
// Negative:
//
//   tau_f = Fc- * tanh(Ks*w) + B-*w
//
// Because tanh() has the same sign as velocity,
// compensation torque acts in the motion direction.
// ============================================================

float calculateFrictionModelTorque(
    float velocityRadS)
{
    if (
        fabsf(velocityRadS)
        <= FRICTION_ZERO_SPEED_RAD_S)
    {
        return 0.0f;
    }


    if (velocityRadS > 0.0f)
    {
        return
            FC_P_NM
            * tanhf(
                FRICTION_KS
                * velocityRadS)
            +
            B_P_NMS_RAD
            * velocityRadS;
    }


    return
        FC_N_NM
        * tanhf(
            FRICTION_KS
            * velocityRadS)
        +
        B_N_NMS_RAD
        * velocityRadS;
}


float calculateFrictionCompensation(
    float frictionModelNm)
{
    float tau =
        FRICTION_GAIN
        * frictionModelNm;


    tau =
        constrain(
            tau,
            -FRICTION_LIMIT_NM,
             FRICTION_LIMIT_NM);


    return tau;
}


// ============================================================
// ZERO-TORQUE RFOB + FRICTION CONTROLLER
// ============================================================

void updateZeroTorqueControl()
{
    if (!controlEnabled)
    {
        torqueReferenceNm =
            0.0f;

        torqueErrorNm =
            0.0f;

        rfobControlTorqueNm =
            0.0f;

        frictionModelTorqueNm =
            0.0f;

        frictionRawTorqueNm =
            0.0f;

        frictionCompTorqueNm =
            0.0f;

        torqueCommandNm =
            0.0f;

        return;
    }


    // --------------------------------------------------------
    // Professor's zero-torque reference
    // --------------------------------------------------------

    torqueReferenceNm =
        0.0f;


    // --------------------------------------------------------
    // RFOB torque error
    //
    // e_tau = tau_ref - tau_RFOB
    // --------------------------------------------------------

    torqueErrorNm =
        torqueReferenceNm
        - estimatedReactionTorqueNm;


    // --------------------------------------------------------
    // Pure P RFOB controller
    // --------------------------------------------------------

    rfobControlTorqueNm =
        CONTROL_SIGN
        * ZERO_TORQUE_KP
        * torqueErrorNm;


    // --------------------------------------------------------
    // Friction compensation
    // --------------------------------------------------------

    // Physical friction model is calculated in updateRFOB()
    // using the same current velocity as the observer.
    //
    // Observer:
    //   uses frictionModelTorqueNm directly.
    //
    // Motor feedforward:
    //   uses FRICTION_GAIN * frictionModelTorqueNm.
    frictionCompTorqueNm =
        calculateFrictionCompensation(
            frictionModelTorqueNm);


    // --------------------------------------------------------
    // FINAL COMMAND
    //
    // tau_cmd =
    //     tau_RFOB_control
    //     + tau_friction
    // --------------------------------------------------------

    torqueCommandNm =
        rfobControlTorqueNm
        + frictionCompTorqueNm;


    // --------------------------------------------------------
    // Final motor command saturation
    // --------------------------------------------------------

    torqueCommandNm =
        constrain(
            torqueCommandNm,
            -TORQUE_LIMIT_NM,
             TORQUE_LIMIT_NM);
}


// ============================================================
// RFOB
// ============================================================

void updateRFOB()
{
    const uint32_t nowUs =
        micros();


    if (!rfobInitialized)
    {
        previousVelocityRadS =
            motor.speedRadS;

        previousRfobUs =
            nowUs;

        rawAccelerationRadS2 =
            0.0f;

        filteredAccelerationRadS2 =
            0.0f;

        rawReactionTorqueNm =
            0.0f;

        estimatedReactionTorqueNm =
            0.0f;

        rfobInitialized =
            true;

        return;
    }


    const float dt =
        static_cast<float>(
            nowUs - previousRfobUs)
        * 1.0e-6f;


    if (
        dt <= 0.0f
        || dt > 0.05f)
    {
        previousVelocityRadS =
            motor.speedRadS;

        previousRfobUs =
            nowUs;

        return;
    }


    // --------------------------------------------------------
    // Acceleration estimate
    // --------------------------------------------------------

    rawAccelerationRadS2 =
        (
            motor.speedRadS
            - previousVelocityRadS
        )
        / dt;


    const float accelAlpha =
        lowPassAlpha(
            ACCEL_CUTOFF_HZ,
            dt);


    filteredAccelerationRadS2 +=
        accelAlpha
        * (
            rawAccelerationRadS2
            - filteredAccelerationRadS2
        );


    // --------------------------------------------------------
    // Motor torque
    //
    // tau_m = Kt * Iq
    // --------------------------------------------------------

    motorTorqueNm =
        KT_NOMINAL
        * motor.iqA;


    // --------------------------------------------------------
    // Physical friction model
    //
    // tau_f_model = identified Coulomb + viscous friction
    //
    // IMPORTANT:
    // This is the physical identified model only.
    // FRICTION_GAIN is NOT applied inside the observer.
    // --------------------------------------------------------

    frictionModelTorqueNm =
        calculateFrictionModelTorque(
            motor.speedRadS);


    // Keep current logging field compatible.
    frictionRawTorqueNm =
        frictionModelTorqueNm;


    // --------------------------------------------------------
    // FRICTION-COMPENSATED RFOB
    //
    // tau_reac_hat_raw =
    //     tau_motor
    //     - J * acceleration
    //     - tau_f_model
    //
    // This prevents the RFOB/P loop from treating identified
    // internal friction as external reaction torque.
    //
    // IMPORTANT:
    // subtract frictionModelTorqueNm here,
    // NOT frictionCompTorqueNm.
    // --------------------------------------------------------

    rawReactionTorqueNm =
        motorTorqueNm
        - J_NOMINAL
        * filteredAccelerationRadS2
        - frictionModelTorqueNm;


    // --------------------------------------------------------
    // RFOB Q filter
    // --------------------------------------------------------

    const float rfobAlpha =
        lowPassAlpha(
            RFOB_CUTOFF_HZ,
            dt);


    estimatedReactionTorqueNm +=
        rfobAlpha
        * (
            rawReactionTorqueNm
            - estimatedReactionTorqueNm
        );


    previousVelocityRadS =
        motor.speedRadS;

    previousRfobUs =
        nowUs;


    // Use newest RFOB + velocity immediately.
    updateZeroTorqueControl();
}


// ============================================================
// CAN FEEDBACK
//
// UNCHANGED FROM YOUR WORKING CODE
// ============================================================

void decodeFeedback(
    const CAN_message_t &msg)
{
    if (!msg.flags.extended)
        return;

    if (msg.id != FEEDBACK_ID)
        return;

    if (msg.len != 8)
        return;


    const int16_t positionRaw =
        static_cast<int16_t>(
            (
                static_cast<uint16_t>(
                    msg.buf[0])
                << 8
            )
            | msg.buf[1]);


    const int16_t speedRaw =
        static_cast<int16_t>(
            (
                static_cast<uint16_t>(
                    msg.buf[2])
                << 8
            )
            | msg.buf[3]);


    const int16_t currentRaw =
        static_cast<int16_t>(
            (
                static_cast<uint16_t>(
                    msg.buf[4])
                << 8
            )
            | msg.buf[5]);


    motor.positionDeg =
        static_cast<float>(
            positionRaw)
        * 0.1f;


    motor.positionRad =
        degToRadF(
            motor.positionDeg);


    motor.speedErpm =
        static_cast<float>(
            speedRaw)
        * 10.0f;


    motor.speedRadS =
        erpmToOutputRadS(
            motor.speedErpm);


    motor.iqA =
        static_cast<float>(
            currentRaw)
        * 0.01f;


    motor.errorCode =
        msg.buf[7];


    motor.lastFeedbackUs =
        micros();


    motor.valid =
        true;


    // Update RFOB once per feedback sample.
    updateRFOB();
}


void readCAN()
{
    CAN_message_t msg{};

    while (CanBus.read(msg))
    {
        decodeFeedback(msg);
    }
}


// ============================================================
// LOGGING
// ============================================================

void printHeader()
{
    Serial.println(
        "Time_ms,"
        "Control_Enabled,"
        "Position_deg,"
        "Velocity_rad_s,"
        "Acceleration_rad_s2,"
        "Iq_A,"
        "Motor_Torque_Nm,"
        "RFOB_Raw_Nm,"
        "RFOB_Estimated_Nm,"
        "Torque_Error_Nm,"
        "RFOB_Control_Nm,"
        "Friction_Raw_Nm,"
        "Friction_Comp_Nm,"
        "Torque_Command_Nm,"
        "Zero_Torque_Kp,"
        "Friction_Gain,"
        "Error_Code");
}


void printData()
{
    Serial.print(
        millis());

    Serial.print(",");


    Serial.print(
        controlEnabled ? 1 : 0);

    Serial.print(",");


    Serial.print(
        motor.positionDeg,
        4);

    Serial.print(",");


    Serial.print(
        motor.speedRadS,
        5);

    Serial.print(",");


    Serial.print(
        filteredAccelerationRadS2,
        5);

    Serial.print(",");


    Serial.print(
        motor.iqA,
        5);

    Serial.print(",");


    Serial.print(
        motorTorqueNm,
        5);

    Serial.print(",");


    Serial.print(
        rawReactionTorqueNm,
        5);

    Serial.print(",");


    Serial.print(
        estimatedReactionTorqueNm,
        5);

    Serial.print(",");


    Serial.print(
        torqueErrorNm,
        5);

    Serial.print(",");


    Serial.print(
        rfobControlTorqueNm,
        5);

    Serial.print(",");


    Serial.print(
        frictionRawTorqueNm,
        5);

    Serial.print(",");


    Serial.print(
        frictionCompTorqueNm,
        5);

    Serial.print(",");


    Serial.print(
        torqueCommandNm,
        5);

    Serial.print(",");


    Serial.print(
        ZERO_TORQUE_KP,
        3);

    Serial.print(",");


    Serial.print(
        FRICTION_GAIN,
        3);

    Serial.print(",");


    Serial.println(
        motor.errorCode);
}


// ============================================================
// START / STOP
// ============================================================

void startControl()
{
    if (!motor.valid)
    {
        Serial.println(
            "Cannot start: no feedback.");

        return;
    }


    // --------------------------------------------------------
    // Reset observer transient
    // --------------------------------------------------------

    rfobInitialized =
        false;


    torqueReferenceNm =
        0.0f;

    torqueErrorNm =
        0.0f;

    rfobControlTorqueNm =
        0.0f;

    frictionModelTorqueNm =
        0.0f;

    frictionRawTorqueNm =
        0.0f;

    frictionCompTorqueNm =
        0.0f;

    torqueCommandNm =
        0.0f;


    controlEnabled =
        true;


    Serial.println(
        "RFOB + FRICTION COMPENSATION ON");
}


void stopControl()
{
    controlEnabled =
        false;


    torqueReferenceNm =
        0.0f;

    torqueErrorNm =
        0.0f;

    rfobControlTorqueNm =
        0.0f;

    frictionModelTorqueNm =
        0.0f;

    frictionRawTorqueNm =
        0.0f;

    frictionCompTorqueNm =
        0.0f;

    torqueCommandNm =
        0.0f;


    sendZeroTorque();


    Serial.println(
        "RFOB + FRICTION COMPENSATION OFF");
}


// ============================================================
// SETUP
// ============================================================

void setup()
{
    Serial.begin(
        115200);


    delay(
        1000);


    // --------------------------------------------------------
    // SAME CAN INITIALIZATION AS WORKING CODE
    // --------------------------------------------------------

    CanBus.begin();


    CanBus.setBaudRate(
        CAN_BAUD);


    delay(
        500);


    sendZeroTorque();


    Serial.println();

    Serial.println(
        "AK60 RFOB Zero-Torque + Friction Compensation");


    Serial.println();

    Serial.println(
        "RFOB settings:");

    Serial.print(
        "Zero torque Kp = ");

    Serial.println(
        ZERO_TORQUE_KP,
        3);


    Serial.print(
        "J = ");

    Serial.println(
        J_NOMINAL,
        8);


    Serial.print(
        "Kt = ");

    Serial.println(
        KT_NOMINAL,
        6);


    Serial.println();

    Serial.println(
        "Friction parameters:");


    Serial.print(
        "Ts+ = ");

    Serial.println(
        TS_P_NM,
        6);


    Serial.print(
        "Ts- = ");

    Serial.println(
        TS_N_NM,
        6);


    Serial.print(
        "Fc+ = ");

    Serial.println(
        FC_P_NM,
        6);


    Serial.print(
        "Fc- = ");

    Serial.println(
        FC_N_NM,
        6);


    Serial.print(
        "B+ = ");

    Serial.println(
        B_P_NMS_RAD,
        8);


    Serial.print(
        "B- = ");

    Serial.println(
        B_N_NMS_RAD,
        8);


    Serial.print(
        "Friction gain = ");

    Serial.println(
        FRICTION_GAIN,
        3);

    Serial.println(
        "RFOB subtracts physical friction model before Q-filter");


    Serial.println();


    Serial.println(
        "s = enable RFOB + friction compensation");


    Serial.println(
        "x = disable / zero torque");


    Serial.println();


    printHeader();
}


// ============================================================
// LOOP
// ============================================================

void loop()
{
    // --------------------------------------------------------
    // SAME AS YOUR WORKING PROGRAM
    // --------------------------------------------------------

    CanBus.events();


    readCAN();


    // --------------------------------------------------------
    // Serial start / stop
    // --------------------------------------------------------

    if (Serial.available())
    {
        const char c =
            Serial.read();


        if (
            c == 's'
            || c == 'S')
        {
            startControl();
        }


        else if (
            c == 'x'
            || c == 'X')
        {
            stopControl();
        }
    }


    // --------------------------------------------------------
    // 1 kHz motor command
    // --------------------------------------------------------

    while (
        commandTimer
        >= COMMAND_PERIOD_US)
    {
        commandTimer -=
            COMMAND_PERIOD_US;


        // ----------------------------------------------------
        // Feedback lost / motor error -> zero torque
        // ----------------------------------------------------

        if (
            !motor.valid
            || (
                micros()
                - motor.lastFeedbackUs
                > FEEDBACK_TIMEOUT_US
            )
            || motor.errorCode != 0)
        {
            rfobControlTorqueNm =
                0.0f;

            frictionRawTorqueNm =
                0.0f;

            frictionCompTorqueNm =
                0.0f;

            torqueCommandNm =
                0.0f;


            sendZeroTorque();


            continue;
        }


        // ----------------------------------------------------
        // Controller ON
        // ----------------------------------------------------

        if (controlEnabled)
        {
            // Recalculate with latest RFOB + velocity.
            updateZeroTorqueControl();


            // No position target.
            // No MIT position Kp/Kd.
            //
            // Only:
            //
            //   RFOB zero-torque control
            //   +
            //   identified friction compensation

            sendMIT(
                0.0f,
                0.0f,
                0.0f,
                0.0f,
                torqueCommandNm);
        }


        // ----------------------------------------------------
        // Controller OFF
        // ----------------------------------------------------

        else
        {
            sendZeroTorque();
        }
    }


    // --------------------------------------------------------
    // 100 Hz logging
    // --------------------------------------------------------

    if (
        micros()
        - lastPrintUs
        >= 10000)
    {
        lastPrintUs =
            micros();


        printData();
    }
}