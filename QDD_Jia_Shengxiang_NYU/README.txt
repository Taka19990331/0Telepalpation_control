QDD-Based Haptic Robot Arm — Project Directory
===============================================

final/
│
├─ 01_Motor_Identification/                 Motor system identification
│  ├─ Arduino_Sketches/                     Identification programs for three motors
│  │  ├─ motor1ak80/                        Motor 1 — AK80
│  │  ├─ motor2ak80/                        Motor 2 — AK80
│  │  └─ motor3ak60/                        Motor 3 — AK60
│  ├─ Data/                                 Motor identification test data
│  │  ├─ AK80_Motor1/
│  │  ├─ AK80_Motor2/
│  │  └─ AK60_Motor3/
│  └─ MATLAB_Analysis/                      FRF calculation and model-fitting script
│
├─ 02_Friction_Identification/              Friction identification
│  ├─ Arduino_Sketches/                     AK60 and AK80 friction test programs
│  ├─ Data/                                 Raw friction test data
│  ├─ MATLAB_Analysis/                      Stribeck friction model identification
│  └─ Results/                              Saved identification results
│
├─ 03_Impedance_Control/                    Impedance control
│  ├─ Arduino_Sketches/                     AK60 and AK80 impedance control programs
│  ├─ Data/                                 Test data for different K and D values
│  └─ MATLAB_Analysis/                      K/D response comparison and plotting
│
├─ 04_RFOB_Validation/                      Reaction Force Observer validation
│  ├─ Arduino_Sketches/                     AK80 RFOB validation program
│  └─ MATLAB_Analysis/                      RFOB comparison analysis
│                                             (original test data unavailable)
│
├─ 05_Transparency_Control/                 Transparency control
│  └─ Arduino_Sketches/                     Friction-compensated RFOB programs
│                                             for all three motors
│                                             (original test data unavailable)
│
├─ 06_Integrated_Controller/                Integrated three-motor controller
│  └─ TwoMode_ThreeMotor_Project/
│     ├─ TwoMode_ThreeMotor_Project.ino     Main program
│     ├─ Mode1_Motor1.h                     Mode 1 — Motor 1
│     ├─ Mode1_Motor2.h                     Mode 1 — Motor 2
│     ├─ Mode1_Motor3.h                     Mode 1 — Motor 3
│     ├─ Mode2_Motor1.h                     Mode 2 — Motor 1
│     ├─ Mode2_Motor2.h                     Mode 2 — Motor 2
│     ├─ Mode2_Motor3.h                     Mode 2 — Motor 3
│     └─ README.txt                         Serial command instructions
│
├─ 07_Presentation/                         Project presentation
│  └─ QDD_Haptic_Robot_Project_Presentation.pptx
│
└─ README.txt                               This directory overview

Project Workflow
----------------

Motor System Identification
    ↓
Friction Identification and Compensation
    ↓
Impedance Control
    ↓
RFOB Validation and Transparency Control
    ↓
Integrated Three-Motor, Two-Mode Controller
    ↓
Project Presentation
