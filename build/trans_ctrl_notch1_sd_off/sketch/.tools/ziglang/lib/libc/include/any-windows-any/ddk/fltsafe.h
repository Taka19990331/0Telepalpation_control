#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\any-windows-any\\ddk\\fltsafe.h"
#if (NTDDI_VERSION >= NTDDI_WINXP)
struct FLOATSAFE {
  KFLOATING_SAVE FloatSave;
  NTSTATUS ntStatus;
  FLOATSAFE::FLOATSAFE(void) {
    ntStatus = KeSaveFloatingPointState(&FloatSave);
  }
  FLOATSAFE::~FLOATSAFE(void) {
    if (NT_SUCCESS(ntStatus)) {
      KeRestoreFloatingPointState(&FloatSave);
    }
  }
};
#endif
