/*
 * EFI Boot Guard
 *
 * Copyright (c) Siemens AG, 2017
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *  Andreas Reichel <andreas.reichel.ext@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * SPDX-License-Identifier:	GPL-2.0-only
 */

#include <efi.h>
#include <efilib.h>
#include <bootguard.h>
#include <utils.h>

VOID PrintC(const UINT8 color, const CHAR16 *fmt, ...)
{
	INT32 attr = ST->ConOut->Mode->Attribute;
	(VOID) ST->ConOut->SetAttribute(ST->ConOut, color);

	va_list args;
	va_start(args, fmt);
	(VOID)VPrint(fmt, args);
	va_end(args);

	(VOID) ST->ConOut->SetAttribute(ST->ConOut, attr);
}

static BOOLEAN IsOnBootMedium(EFI_DEVICE_PATH *dp)
{
	extern CHAR16 *boot_medium_path;
	CHAR16 *device_path, *tmp;
	BOOLEAN result = FALSE;

	tmp = DevicePathToStr(dp);
	device_path = GetBootMediumPath(tmp);
	FreePool(tmp);

	if (StrCmp(device_path, boot_medium_path) == 0) {
		result = TRUE;
	}
	FreePool(device_path);

	return result;
}

VOID __attribute__((noreturn)) error_exit(CHAR16 *message, EFI_STATUS status)
{
	ERROR(L"%s (%r).\n", message, status);
	(VOID) BS->Stall(3 * 1000 * 1000);
	(VOID) BS->Exit(this_image, status, 0, NULL);
	__builtin_unreachable();
}

CHAR16 *get_volume_label(EFI_FILE_HANDLE fh)
{
	EFI_FILE_SYSTEM_INFO *fsi;
	EFI_GUID fsiGuid = EFI_FILE_SYSTEM_INFO_ID;
	UINTN fsis;
	EFI_STATUS status;

	fsi = AllocatePool(MAX_INFO_SIZE);
	if (fsi == NULL) {
		return NULL;
	}
	fsis = MAX_INFO_SIZE;
	status = fh->GetInfo(fh, &fsiGuid, &fsis, (VOID *)fsi);
	if (EFI_ERROR(status) || fsis == 0) {
		FreePool(fsi);
		return NULL;
	}
	return fsi->VolumeLabel;
}

CHAR16 *get_volume_custom_label(EFI_FILE_HANDLE fh)
{
	EFI_STATUS status;
	EFI_FILE_HANDLE tmp;
	CHAR16 *buffer = AllocatePool(64);
	UINTN buffsize = 63;

	status = fh->Open(
	    fh, &tmp, L"EFILABEL", EFI_FILE_MODE_READ,
	    EFI_FILE_ARCHIVE | EFI_FILE_HIDDEN | EFI_FILE_SYSTEM);
	if (status != EFI_SUCCESS) {
		FreePool(buffer);
		return NULL;
	}
	status = tmp->Read(tmp, &buffsize, buffer);
	if (status != EFI_SUCCESS) {
		(VOID) fh->Close(tmp);
		FreePool(buffer);
		return NULL;
	}
	buffer[buffsize/sizeof(CHAR16)] = L'\0';
	(VOID) fh->Close(tmp);
	return buffer;
}

EFI_STATUS get_volumes(VOLUME_DESC **volumes, UINTN *count)
{
	EFI_STATUS status;
	EFI_HANDLE *handles = NULL;
	EFI_GUID sfspGuid = SIMPLE_FILE_SYSTEM_PROTOCOL;
	UINTN handleCount = 0;
	UINTN index, rootCount = 0;

	EFI_FILE_HANDLE tmp;

	if (!volumes || !count) {
		ERROR(L"Invalid volume enumeration.\n");
		return EFI_INVALID_PARAMETER;
	}

	status = BS->LocateHandleBuffer(
	    ByProtocol, &sfspGuid, NULL, &handleCount, &handles);
	if (EFI_ERROR(status)) {
		ERROR(L"Could not locate handle buffer.\n");
		return EFI_OUT_OF_RESOURCES;
	}
	INFO(L"Found %d handles for file IO\n\n", handleCount);

	*volumes = (VOLUME_DESC *)AllocatePool(sizeof(VOLUME_DESC) * handleCount);
	if (!*volumes) {
		ERROR(L"Could not allocate memory for volume descriptors.\n");
		return EFI_OUT_OF_RESOURCES;
	}

	for (index = 0; index < handleCount; index++) {
		EFI_FILE_IO_INTERFACE *fs = NULL;
		CHAR16 *devpathstr;

		status = BS->HandleProtocol(
		    handles[index], &sfspGuid, (VOID **)&fs);
		if (EFI_ERROR(status)) {
			/* skip failed handle and continue enumerating */
			ERROR(L"File IO handle %d does not support SIMPLE_FILE_SYSTEM_PROTOCOL, skipping.\n",
			      index);
			continue;
		}
		status = fs->OpenVolume(fs, &tmp);
		if (EFI_ERROR(status)) {
			/* skip failed handle and continue enumerating */
			ERROR(L"Could not open file system for IO handle %d, skipping.\n",
			      index);
			continue;
		}
		EFI_DEVICE_PATH *devpath = DevicePathFromHandle(handles[index]);
		if (devpath == NULL) {
			ERROR(L"Could not get device path for config partition, skipping.\n");
			continue;
		}
		devpathstr = DevicePathToStr(devpath);

		BOOLEAN onbootmedium = IsOnBootMedium(devpath);

		(*volumes)[rootCount].root = tmp;
		(*volumes)[rootCount].devpath = devpath;
		(*volumes)[rootCount].onbootmedium = onbootmedium;
		(*volumes)[rootCount].fslabel =
		    get_volume_label((*volumes)[rootCount].root);
		(*volumes)[rootCount].fscustomlabel =
		    get_volume_custom_label((*volumes)[rootCount].root);
		INFO(L"Volume %d: ", rootCount);
		if (onbootmedium) {
			INFO(L"(On boot medium) ");
		}
		INFO(L"%s, LABEL=%s, CLABEL=%s\n",
		      devpathstr, (*volumes)[rootCount].fslabel,
		      (*volumes)[rootCount].fscustomlabel);

		FreePool(devpathstr);

		rootCount++;
	}
	*count = rootCount;
	return EFI_SUCCESS;
}

EFI_STATUS close_volumes(VOLUME_DESC *volumes, UINTN count)
{
	EFI_STATUS result = EFI_SUCCESS;
	UINTN i;

	if (!volumes) {
		ERROR(L"Invalid parameter for closing volumes.\n");
		return EFI_INVALID_PARAMETER;
	}
	for (i = 0; i < count; i++) {
		EFI_STATUS status;

		if (!volumes[i].root) {
			ERROR(L"Invalid handle for volume %d.\n", i);
			result = EFI_INVALID_PARAMETER;
			continue;
		}
		status = volumes[i].root->Close(volumes[i].root);
		if (EFI_ERROR(status)) {
			ERROR(L"Could not close volume %d.\n", i);
			result = EFI_DEVICE_ERROR;
		}
	}
	FreePool(volumes);
	return result;
}

EFI_DEVICE_PATH *FileDevicePathFromConfig(EFI_HANDLE device,
					  CHAR16 *payloadpath)
{
	UINTN prefixlen = 0;
	EFI_DEVICE_PATH *devpath = NULL;

	LABELMODE lm = NOLABEL;
	/* Check if payload path contains a
	 * L:LABEL: item to specify a FAT partition or a
	 * C:LABEL: to specify a custom labeled FAT partition */
	if (StrnCmp(payloadpath, L"L:", 2) == 0) {
		lm = DOSFSLABEL;
	} else if (StrnCmp(payloadpath, L"C:", 2) == 0) {
		lm = CUSTOMLABEL;
	}

	if (lm != NOLABEL) {
		for (UINTN i = 2; i < StrLen(payloadpath); i++) {
			if (payloadpath[i] == L':') {
				prefixlen = i - 2;
				break;
			}
		}
	}

	if (prefixlen > 0) {
		for (UINTN v = 0; v < volume_count; v++) {
			CHAR16 *src;
			switch (lm) {
			case DOSFSLABEL:
				src = volumes[v].fslabel;
				break;
			case CUSTOMLABEL:
				src = volumes[v].fscustomlabel;
				break;
			default:
				src = NULL;
				break;
			}
			if (src && (StrnCmp(src, &payloadpath[2], prefixlen) == 0)) {
				devpath = volumes[v].devpath;
				break;
			}
		}
	}

	if (!devpath) {
		/* No label prefix specified, use device of bootloader image */
		return FileDevicePath(device, payloadpath);
	}

	EFI_DEVICE_PATH *filedevpath;
	EFI_DEVICE_PATH *appendeddevpath;

	filedevpath = FileDevicePath(NULL, payloadpath + prefixlen + 3);
	appendeddevpath = AppendDevicePath(devpath, filedevpath);
	FreePool(filedevpath);

	CHAR16 *pathstr = DevicePathToStr(appendeddevpath);
	INFO(L"Full path for kernel is: %s\n", pathstr);
	FreePool(pathstr);

	return appendeddevpath;
}

CHAR16 *GetBootMediumPath(CHAR16 *input)
{
	CHAR16 *dst;
	UINTN len;

	len = StrLen(input);

	dst = AllocatePool((len + 1) * sizeof(CHAR16));
	if (!dst) {
		return NULL;
	}

	StrCpy(dst, input);

	for (UINTN i = len; i > 0; i--)
	{
		if (dst[i] == L'/') {
			dst[i] = L'\0';
			break;
		}
	}

	return dst;
}
