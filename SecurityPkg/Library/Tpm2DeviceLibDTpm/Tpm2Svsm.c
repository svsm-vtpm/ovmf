/** @file
  SVSM TPM communication

Copyright (c) 2023 James.Bottomley@HansenPartnership.com

SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/Tpm2DeviceLib.h>
#include <Library/PcdLib.h>
#include <Library/CcExitLib.h>

#include "Tpm2DeviceLibDTpm.h"

#pragma pack(1)
typedef struct Tpm2SendCmdReq {
	UINT32	Cmd;
	UINT8	Locality;
	UINT32	BufSize;
	UINT8	Buf[];
} Tpm2SendCmdReq;
typedef struct Tpm2SendCmdResp {
	INT32	Size;
	UINT8	Buf[];
} Tpm2SendCmdResp;
#pragma pack()

/* These defines come from the TPM platform interface */
#define TPM_SEND_COMMAND		8
#define TPM_PLATFORM_MAX_BUFFER		4096 /* max req/resp buffer size */

STATIC UINT8 Tpm2SvsmBuffer[TPM_PLATFORM_MAX_BUFFER];

/**
  Send a command to TPM for execution and return response data.

  @param[in]      BufferIn      Buffer for command data.
  @param[in]      SizeIn        Size of command data.
  @param[in, out] BufferOut     Buffer for response data.
  @param[in, out] SizeOut       Size of response data.

  @retval EFI_SUCCESS           Operation completed successfully.
  @retval EFI_BUFFER_TOO_SMALL  Response data buffer is too small.
  @retval EFI_DEVICE_ERROR      Unexpected device behavior.
  @retval EFI_UNSUPPORTED       Unsupported TPM version

**/
EFI_STATUS
Tpm2SvsmTpmCommand (
  IN     UINT8                 *BufferIn,
  IN     UINT32                SizeIn,
  IN OUT UINT8                 *BufferOut,
  IN OUT UINT32                *SizeOut
  )
{
  Tpm2SendCmdReq  *req  = (Tpm2SendCmdReq *)Tpm2SvsmBuffer;
  Tpm2SendCmdResp *resp = (Tpm2SendCmdResp *)Tpm2SvsmBuffer;

  if (SizeIn > sizeof (Tpm2SvsmBuffer) - sizeof (*req)) {
    return EFI_BUFFER_TOO_SMALL;
  }

  req->Cmd = TPM_SEND_COMMAND;
  req->Locality = 0;
  req->BufSize = SizeIn;
  CopyMem (req->Buf, BufferIn, SizeIn);
  if (!VmgExitVTPM (Tpm2SvsmBuffer)) {
    return EFI_DEVICE_ERROR;
  }

  *SizeOut = resp->Size;
  CopyMem (BufferOut, resp->Buf, *SizeOut);

  return EFI_SUCCESS;
}

/**
  Check if an SVSM based TPM is present

  @retval TRUE    SVSM TPM is present
  @retval FALSE   no SVSM TPM is present
**/
BOOLEAN
Tpm2IsSvsm (
  VOID
  )
{
  BOOLEAN ret;
  ret = VmgExitVTPM(NULL);
  DEBUG((DEBUG_INFO, "Tpm2IsSvsm return %d\n", ret));
  return ret;
}
