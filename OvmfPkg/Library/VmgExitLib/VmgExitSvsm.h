/** @file
  SVSM Support Library common header file

  Copyright (C) 2022, Advanced Micro Devices, Inc. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _VMGEXIT_LIB_SVSM_H_
#define _VMGEXIT_LIB_SVSM_H_

#include <Base.h>
#include <Uefi.h>

//
// The SVSM definitions are part of the SNP Secrets Page:
//   An SVSM is considered present if the SvsmSize field is non-zero.
//
typedef PACKED struct {
  UINT8   Reserved1[320];

  UINT64  SvsmBase;
  UINT64  SvsmSize;
  UINT64  SvsmCaa;
  UINT32  SvsmMaxVersion;
  UINT8   SvsmGuestVmpl;
  UINT8   Reserved2[3];
} SVSM_INFORMATION;

typedef PACKED struct {
  UINT8  SvsmCallPending;
  UINT8  SvsmMemAvailable;
  UINT8  Reserved1[6];

  //
  // The remainder of the CAA 4KB area can be used for argument
  // passing to the SVSM.
  //
  UINT8  SvsmBuffer[SIZE_4KB - 8];
} SVSM_CAA;

typedef union {
  struct {
    UINT64  PageSize   : 2;
    UINT64  Action     : 1;
    UINT64  IgnoreCf   : 1;
    UINT64  Reserved_2 : 8;
    UINT64  Address    : 52;
  };
  UINT64  Uint64;
} SVSM_PVALIDATE_ENTRY;

typedef PACKED struct {
  UINT16  Entries;
  UINT16  Next;

  UINT8   Reserved_1[4];

  SVSM_PVALIDATE_ENTRY  Entry[];
} SVSM_PVALIDATE_REQUEST;

typedef union {
  SVSM_PVALIDATE_REQUEST  PvalidateRequest;
} SVSM_REQUEST;

typedef union {
  struct {
    UINT32  CallId;
    UINT32  Protocol;
  };

  UINT64  Uint64;
} SVSM_FUNCTION;

/**
  Return the address of SVSM Call Area (CAA).

  Determines the address of the SVSM CAA.

  @return             The address of the SVSM CAA

**/
SVSM_CAA *
SvsmGetCaa (
  VOID
  );

/**
  Issue an SVSM request.

  Invokes the SVSM to process a request on behalf of the guest.

  @param[in]  Caa     Pointer to the call area
  @param[in]  Rax     Contents to be set in RAX at VMGEXIT
  @param[in]  Rcx     Contents to be set in RCX at VMGEXIT
  @param[in]  Rdx     Contents to be set in RDX at VMGEXIT
  @param[in]  R8      Contents to be set in R8 at VMGEXIT

  @return             Contents of RAX upon return from VMGEXIT
**/
UINTN
SvsmMsrProtocol (
  IN SVSM_CAA  *Caa,
  IN UINT64    Rax,
  IN UINT64    Rcx,
  IN UINT64    Rdx,
  IN UINT64    R8,
  IN UINT64    R9
  );

#endif
