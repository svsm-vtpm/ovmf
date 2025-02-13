/**@file
  Platform PEI driver

  Copyright (c) 2006 - 2016, Intel Corporation. All rights reserved.<BR>
  Copyright (c) 2011, Andrei Warkentin <andreiw@motorola.com>

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

//
// The package level header files this module uses
//
#include <PiPei.h>

//
// The Library classes this module consumes
//
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/PciLib.h>
#include <Library/PeimEntryPoint.h>
#include <Library/PeiServicesLib.h>
#include <Library/QemuFwCfgLib.h>
#include <Library/QemuFwCfgS3Lib.h>
#include <Library/QemuFwCfgSimpleParserLib.h>
#include <Library/ResourcePublicationLib.h>
#include <Ppi/MasterBootMode.h>
#include <IndustryStandard/I440FxPiix4.h>
#include <IndustryStandard/Microvm.h>
#include <IndustryStandard/Pci22.h>
#include <IndustryStandard/Q35MchIch9.h>
#include <IndustryStandard/QemuCpuHotplug.h>
#include <Library/MemEncryptSevLib.h>
#include <OvmfPlatforms.h>

#include "Platform.h"

EFI_HOB_PLATFORM_INFO  mPlatformInfoHob = { 0 };

EFI_PEI_PPI_DESCRIPTOR  mPpiBootMode[] = {
  {
    EFI_PEI_PPI_DESCRIPTOR_PPI | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST,
    &gEfiPeiMasterBootModePpiGuid,
    NULL
  }
};

VOID
MemMapInitialization (
  IN OUT EFI_HOB_PLATFORM_INFO  *PlatformInfoHob
  )
{
  RETURN_STATUS  PcdStatus;

  PlatformMemMapInitialization (PlatformInfoHob);

  if (PlatformInfoHob->HostBridgeDevId == 0xffff /* microvm */) {
    return;
  }

  PcdStatus = PcdSet64S (PcdPciMmio32Base, PlatformInfoHob->PcdPciMmio32Base);
  ASSERT_RETURN_ERROR (PcdStatus);
  PcdStatus = PcdSet64S (PcdPciMmio32Size, PlatformInfoHob->PcdPciMmio32Size);
  ASSERT_RETURN_ERROR (PcdStatus);

  PcdStatus = PcdSet64S (PcdPciIoBase, PlatformInfoHob->PcdPciIoBase);
  ASSERT_RETURN_ERROR (PcdStatus);
  PcdStatus = PcdSet64S (PcdPciIoSize, PlatformInfoHob->PcdPciIoSize);
  ASSERT_RETURN_ERROR (PcdStatus);
}

VOID
NoexecDxeInitialization (
  VOID
  )
{
  RETURN_STATUS  Status;

  Status = PlatformNoexecDxeInitialization (&mPlatformInfoHob);
  if (!RETURN_ERROR (Status)) {
    Status = PcdSetBoolS (PcdSetNxForStack, mPlatformInfoHob.PcdSetNxForStack);
    ASSERT_RETURN_ERROR (Status);
  }
}

static const UINT8  EmptyFdt[] = {
  0xd0, 0x0d, 0xfe, 0xed, 0x00, 0x00, 0x00, 0x48,
  0x00, 0x00, 0x00, 0x38, 0x00, 0x00, 0x00, 0x48,
  0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x11,
  0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x09,
};

VOID
MicrovmInitialization (
  VOID
  )
{
  FIRMWARE_CONFIG_ITEM  FdtItem;
  UINTN                 FdtSize;
  UINTN                 FdtPages;
  EFI_STATUS            Status;
  UINT64                *FdtHobData;
  VOID                  *NewBase;

  Status = QemuFwCfgFindFile ("etc/fdt", &FdtItem, &FdtSize);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "%a: no etc/fdt found in fw_cfg, using dummy\n", __FUNCTION__));
    FdtItem = 0;
    FdtSize = sizeof (EmptyFdt);
  }

  FdtPages = EFI_SIZE_TO_PAGES (FdtSize);
  NewBase  = AllocatePages (FdtPages);
  if (NewBase == NULL) {
    DEBUG ((DEBUG_INFO, "%a: AllocatePages failed\n", __FUNCTION__));
    return;
  }

  if (FdtItem) {
    QemuFwCfgSelectItem (FdtItem);
    QemuFwCfgReadBytes (FdtSize, NewBase);
  } else {
    CopyMem (NewBase, EmptyFdt, FdtSize);
  }

  FdtHobData = BuildGuidHob (&gFdtHobGuid, sizeof (*FdtHobData));
  if (FdtHobData == NULL) {
    DEBUG ((DEBUG_INFO, "%a: BuildGuidHob failed\n", __FUNCTION__));
    return;
  }

  DEBUG ((
    DEBUG_INFO,
    "%a: fdt at 0x%x (size %d)\n",
    __FUNCTION__,
    NewBase,
    FdtSize
    ));
  *FdtHobData = (UINTN)NewBase;
}

VOID
MiscInitializationForMicrovm (
  IN EFI_HOB_PLATFORM_INFO  *PlatformInfoHob
  )
{
  RETURN_STATUS  PcdStatus;

  ASSERT (PlatformInfoHob->HostBridgeDevId == 0xffff);

  DEBUG ((DEBUG_INFO, "%a: microvm\n", __FUNCTION__));
  //
  // Disable A20 Mask
  //
  IoOr8 (0x92, BIT1);

  //
  // Build the CPU HOB with guest RAM size dependent address width and 16-bits
  // of IO space. (Side note: unlike other HOBs, the CPU HOB is needed during
  // S3 resume as well, so we build it unconditionally.)
  //
  BuildCpuHob (PlatformInfoHob->PhysMemAddressWidth, 16);

  MicrovmInitialization ();
  PcdStatus = PcdSet16S (
                PcdOvmfHostBridgePciDevId,
                MICROVM_PSEUDO_DEVICE_ID
                );
  ASSERT_RETURN_ERROR (PcdStatus);
}

VOID
MiscInitialization (
  IN EFI_HOB_PLATFORM_INFO  *PlatformInfoHob
  )
{
  RETURN_STATUS  PcdStatus;

  PlatformMiscInitialization (PlatformInfoHob);

  PcdStatus = PcdSet16S (PcdOvmfHostBridgePciDevId, PlatformInfoHob->HostBridgeDevId);
  ASSERT_RETURN_ERROR (PcdStatus);
}

VOID
BootModeInitialization (
  IN OUT EFI_HOB_PLATFORM_INFO  *PlatformInfoHob
  )
{
  EFI_STATUS  Status;

  if (PlatformCmosRead8 (0xF) == 0xFE) {
    PlatformInfoHob->BootMode = BOOT_ON_S3_RESUME;
  }

  PlatformCmosWrite8 (0xF, 0x00);

  Status = PeiServicesSetBootMode (PlatformInfoHob->BootMode);
  ASSERT_EFI_ERROR (Status);

  Status = PeiServicesInstallPpi (mPpiBootMode);
  ASSERT_EFI_ERROR (Status);
}

VOID
ReserveEmuVariableNvStore (
  )
{
  EFI_PHYSICAL_ADDRESS  VariableStore;
  RETURN_STATUS         PcdStatus;

  //
  // Allocate storage for NV variables early on so it will be
  // at a consistent address.  Since VM memory is preserved
  // across reboots, this allows the NV variable storage to survive
  // a VM reboot.
  //
  VariableStore =
    (EFI_PHYSICAL_ADDRESS)(UINTN)
    AllocateRuntimePages (
      EFI_SIZE_TO_PAGES (2 * PcdGet32 (PcdFlashNvStorageFtwSpareSize))
      );
  DEBUG ((
    DEBUG_INFO,
    "Reserved variable store memory: 0x%lX; size: %dkb\n",
    VariableStore,
    (2 * PcdGet32 (PcdFlashNvStorageFtwSpareSize)) / 1024
    ));
  PcdStatus = PcdSet64S (PcdEmuVariableNvStoreReserved, VariableStore);
  ASSERT_RETURN_ERROR (PcdStatus);
}

VOID
S3Verification (
  VOID
  )
{
 #if defined (MDE_CPU_X64)
  if (mPlatformInfoHob.SmmSmramRequire && mPlatformInfoHob.S3Supported) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: S3Resume2Pei doesn't support X64 PEI + SMM yet.\n",
      __FUNCTION__
      ));
    DEBUG ((
      DEBUG_ERROR,
      "%a: Please disable S3 on the QEMU command line (see the README),\n",
      __FUNCTION__
      ));
    DEBUG ((
      DEBUG_ERROR,
      "%a: or build OVMF with \"OvmfPkgIa32X64.dsc\".\n",
      __FUNCTION__
      ));
    ASSERT (FALSE);
    CpuDeadLoop ();
  }

 #endif
}

VOID
Q35BoardVerification (
  VOID
  )
{
  if (mPlatformInfoHob.HostBridgeDevId == INTEL_Q35_MCH_DEVICE_ID) {
    return;
  }

  DEBUG ((
    DEBUG_ERROR,
    "%a: no TSEG (SMRAM) on host bridge DID=0x%04x; "
    "only DID=0x%04x (Q35) is supported\n",
    __FUNCTION__,
    mPlatformInfoHob.HostBridgeDevId,
    INTEL_Q35_MCH_DEVICE_ID
    ));
  ASSERT (FALSE);
  CpuDeadLoop ();
}

/**
  Fetch the boot CPU count and the possible CPU count from QEMU, and expose
  them to UefiCpuPkg modules. Set the MaxCpuCount field in PlatformInfoHob.
**/
VOID
MaxCpuCountInitialization (
  IN OUT EFI_HOB_PLATFORM_INFO  *PlatformInfoHob
  )
{
  RETURN_STATUS  PcdStatus;

  PlatformMaxCpuCountInitialization (PlatformInfoHob);

  PcdStatus = PcdSet32S (PcdCpuBootLogicalProcessorNumber, PlatformInfoHob->PcdCpuBootLogicalProcessorNumber);
  ASSERT_RETURN_ERROR (PcdStatus);
  PcdStatus = PcdSet32S (PcdCpuMaxLogicalProcessorNumber, PlatformInfoHob->PcdCpuMaxLogicalProcessorNumber);
  ASSERT_RETURN_ERROR (PcdStatus);
}

/**
 * @brief Builds PlatformInfo Hob
 */
VOID
BuildPlatformInfoHob (
  VOID
  )
{
  BuildGuidDataHob (&gUefiOvmfPkgPlatformInfoGuid, &mPlatformInfoHob, sizeof (EFI_HOB_PLATFORM_INFO));
}

/**
  Perform Platform PEI initialization.

  @param  FileHandle      Handle of the file being invoked.
  @param  PeiServices     Describes the list of possible PEI Services.

  @return EFI_SUCCESS     The PEIM initialized successfully.

**/
EFI_STATUS
EFIAPI
InitializePlatform (
  IN       EFI_PEI_FILE_HANDLE  FileHandle,
  IN CONST EFI_PEI_SERVICES     **PeiServices
  )
{
  EFI_STATUS  Status;

  DEBUG ((DEBUG_INFO, "Platform PEIM Loaded\n"));

  mPlatformInfoHob.SmmSmramRequire     = FeaturePcdGet (PcdSmmSmramRequire);
  mPlatformInfoHob.SevEsIsEnabled      = MemEncryptSevEsIsEnabled ();
  mPlatformInfoHob.SevSnpIsEnabled     = MemEncryptSevSnpIsEnabled ();
  mPlatformInfoHob.PcdPciMmio64Size    = PcdGet64 (PcdPciMmio64Size);
  mPlatformInfoHob.DefaultMaxCpuNumber = PcdGet32 (PcdCpuMaxLogicalProcessorNumber);

  PlatformDebugDumpCmos ();

  if (QemuFwCfgS3Enabled ()) {
    DEBUG ((DEBUG_INFO, "S3 support was detected on QEMU\n"));
    mPlatformInfoHob.S3Supported = TRUE;
    Status                       = PcdSetBoolS (PcdAcpiS3Enable, TRUE);
    ASSERT_EFI_ERROR (Status);
  }

  S3Verification ();
  BootModeInitialization (&mPlatformInfoHob);

  //
  // Query Host Bridge DID
  //
  mPlatformInfoHob.HostBridgeDevId = PciRead16 (OVMF_HOSTBRIDGE_DID);
  AddressWidthInitialization (&mPlatformInfoHob);

  MaxCpuCountInitialization (&mPlatformInfoHob);

  if (mPlatformInfoHob.SmmSmramRequire) {
    Q35BoardVerification ();
    Q35TsegMbytesInitialization ();
    Q35SmramAtDefaultSmbaseInitialization ();
  }

  PublishPeiMemory ();

  PlatformQemuUc32BaseInitialization (&mPlatformInfoHob);

  InitializeRamRegions (&mPlatformInfoHob);

  if (mPlatformInfoHob.BootMode != BOOT_ON_S3_RESUME) {
    if (!mPlatformInfoHob.SmmSmramRequire) {
      ReserveEmuVariableNvStore ();
    }

    PeiFvInitialization ();
    MemTypeInfoInitialization ();
    MemMapInitialization (&mPlatformInfoHob);
    NoexecDxeInitialization ();
  }

  InstallClearCacheCallback ();
  AmdSevInitialize ();
  if (mPlatformInfoHob.HostBridgeDevId == 0xffff) {
    MiscInitializationForMicrovm (&mPlatformInfoHob);
  } else {
    MiscInitialization (&mPlatformInfoHob);
  }

  IntelTdxInitialize ();
  InstallFeatureControlCallback ();
  BuildPlatformInfoHob ();

  return EFI_SUCCESS;
}
