#line 1 "C:\\0Telepalpation_control\\controller_implementation\\trans_ctrl_notch1\\.tools\\ziglang\\lib\\libc\\include\\any-windows-any\\i_cryptasn1tls.h"
/**
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is part of the mingw-w64 runtime package.
 * No warranty is given; refer to the file DISCLAIMER.PD within this package.
 */
#ifndef __I_CRYPTASN1TLS_H__
#define __I_CRYPTASN1TLS_H__

#ifdef __cplusplus
extern "C" {
#endif

  typedef DWORD HCRYPTASN1MODULE;
  typedef void *ASN1module_t;
  typedef void *ASN1encoding_t;
  typedef void *ASN1decoding_t;

  HCRYPTASN1MODULE WINAPI I_CryptInstallAsn1Module(ASN1module_t pMod,DWORD dwFlags,void *pvReserved);
  WINBOOL WINAPI I_CryptUninstallAsn1Module(HCRYPTASN1MODULE hAsn1Module);
  ASN1encoding_t WINAPI I_CryptGetAsn1Encoder(HCRYPTASN1MODULE hAsn1Module);
  ASN1decoding_t WINAPI I_CryptGetAsn1Decoder(HCRYPTASN1MODULE hAsn1Module);

#ifdef __cplusplus
}
#endif
#endif
