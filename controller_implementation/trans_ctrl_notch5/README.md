# trans_ctrl_notch5

`trans_ctrl_notch5.ino` is the Arduino sketch. This folder follows `trans_ctrl_notch4` and adds separate compile-time friction model selectors for the RFOB observer and friction feed-forward path.

## Friction model settings

Edit `control_config.h`:

- `RFOB_FRICTION_MODEL` selects the model used in the RFOB observer input.
- `FF_FRICTION_MODEL` selects the model used to calculate the friction feed-forward torque.
- Each selector accepts `FRICTION_STRIBECK` or `FRICTION_TANH`.
- Both calculations use the same `POSITIVE`, `NEGATIVE`, and `OMEGA_0` parameter settings.

The defaults select Stribeck for both paths. For example, the RFOB can use Stribeck while FF uses tanh by setting `RFOB_FRICTION_MODEL=FRICTION_STRIBECK` and `FF_FRICTION_MODEL=FRICTION_TANH`.

Logged `friction_model_Nm` remains the RFOB observer friction value. Binary log metadata records both selected models as `#rfob_friction_model` and `#ff_friction_model`.

Other controller, CAN, notch, and logging behavior follows notch4. Build with `build_verify.ps1` for Teensy 4.1. Verification files copied from notch4 are historical artifacts and should be interpreted as notch4 records.


`Teensy_program_ver1.txt` and `trans_ctrl_notch5_all_code.txt` are copied archival snapshots, not the notch5 source of truth; use the `.ino` and `.h` files for current code.
