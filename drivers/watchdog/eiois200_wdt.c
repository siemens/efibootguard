/*
 * EFI Boot Guard
 *
 * Copyright (c) 2023 Advantech Co., Ltd.
 * Copyright (c) Sonatest AP Inc, 2024
 *
 * Authors:
 *  Marc Ferland <marc.ferland@sonatest.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * SPDX-License-Identifier:	GPL-2.0-only
 */

#include <efi.h>
#include <efilib.h>
#include <pci/header.h>
#include <sys/io.h>
#include <mmio.h>
#include "smbios.h"
#include "utils.h"

/* #define EIO200IS_WDT_DEBUG */

#ifdef EIO200IS_WDT_DEBUG
#define DBG(fmt, ...) Print(fmt, ##__VA_ARGS__)
#else
#define DBG(fmt, ...) {}
#endif

#define EIOIS200_MODE_ENTER		0x87
#define EIOIS200_MODE_EXIT		0xaa
#define EIOIS200_CHIPID1		0x20
#define EIOIS200_CHIPID2		0x21
#define EIOIS200_200_CHIPID		0x9610
#define EIOIS200_211_CHIPID		0x9620
#define EIOIS200_SIOCTRL		0x23
#define EIOIS200_SIOCTRL_SIOEN		BIT(0)
#define EIOIS200_SIOCTRL_SWRST		BIT(1)
#define EIOIS200_IRQCTRL		0x70

#define EIOIS200_PMC_STATUS_IBF		BIT(1)
#define EIOIS200_PMC_STATUS_OBF		BIT(0)
#define EIOIS200_LDAR			0x30
#define EIOIS200_LDAR_LDACT		BIT(0)
#define EIOIS200_IOBA0H			0x60
#define EIOIS200_IOBA0L			0x61
#define EIOIS200_IOBA1H			0x62
#define EIOIS200_IOBA1L			0x63
#define EIOIS200_FLAG_PMC_READ		BIT(0)

#define PMC_WDT_CMD_WRITE		0x2a
#define PMC_WDT_CMD_READ		0x2b
#define PMC_WDT_CTRL_START		0x01
#define PMC_WDT_MIN_TIMEOUT_MS		1000
#define PMC_WDT_MAX_TIMEOUT_MS		32767000

#define WDT_STA_AVAILABLE		BIT(0)
#define WDT_STA_RESET			BIT(7)

#define WDT_REG_STATUS			0x00
#define WDT_REG_CONTROL			0x02
#define WDT_REG_RESET_EVT_TIME		0x14

/* Logical device selection */
#define EIOIS200_LDN			0x07 /* index */
#define EIOIS200_LDN_PMC0		0x0c /* value */
#define EIOIS200_LDN_PMC1		0x0d /* value */

#define MAX_STATUS_RETRY		25
#define SMBIOS_TYPE_2			2

enum eiois200_port_id {
	EIOIS200_PNP,
	EIOIS200_PNP_COUNT
};

struct eiois200_dev_port {
	UINT16 index_port;
	UINT16 data_port;
};

struct pmc_port {
	UINT16 cmd;
	UINT16 data;
};

static const CHAR8 adv_manuf[] = "Advantech Co Ltd";
static const CHAR8 adv_product[] = "SOM-6872";

/* We currently only support the EIOIS200 as implemented on the
 * SOM-6872 COM-Express from Advantech. Other boards might have
 * different port adresses. */
static const struct eiois200_dev_port pnp_port[EIOIS200_PNP_COUNT] = {
	[EIOIS200_PNP] = {
		.index_port = 0x0299,
		.data_port  = 0x029a,
	},
};

static BOOLEAN probed_before = FALSE;

static void eio200_enter(const struct eiois200_dev_port *p)
{
	UINT16 iport = p->index_port;

	/* unlock EC io port */
	outb(EIOIS200_MODE_ENTER, iport);
	outb(EIOIS200_MODE_ENTER, iport);
}

static void eio200_exit(const struct eiois200_dev_port *p)
{
	UINT16 iport = p->index_port;

	/* lock EC io port */
	outb(EIOIS200_MODE_EXIT, iport);
}

static UINT8 eio200_read(const struct eiois200_dev_port *p, UINT8 index)
{
	UINT16 iport = p->index_port;
	UINT16 dport = p->data_port;

	outb(index, iport);
	return inb(dport);
}

static void eio200_write(const struct eiois200_dev_port *p,
			 UINT8 index, UINT8 val)
{
	UINT16 iport = p->index_port;
	UINT16 dport = p->data_port;

	outb(index, iport);
	outb(val, dport);
}

static const struct eiois200_dev_port * eio200_find(void)
{
	UINTN n;

	for (n = 0; n < EIOIS200_PNP_COUNT; ++n) {
		const struct eiois200_dev_port *p = &pnp_port[n];
		UINT16 chipid;

		eio200_enter(p);

		chipid = eio200_read(p, EIOIS200_CHIPID1) << 8;
		chipid |= eio200_read(p, EIOIS200_CHIPID2);

		DBG(L"ChipID: 0x%02x\n", chipid);


		if (chipid != EIOIS200_200_CHIPID &&
		    chipid != EIOIS200_211_CHIPID) {
			eio200_exit(p);
			continue;
		}

		/* let the caller lock the port on exit */
		return p;
	}

	return NULL;
}

static void eio200_enable(const struct eiois200_dev_port *p)
{
	UINT8 reg;

	/* set the enable flag */
	reg = eio200_read(p, EIOIS200_SIOCTRL);
	reg |= EIOIS200_SIOCTRL_SIOEN;
	eio200_write(p, EIOIS200_SIOCTRL, reg);
}

static void eio200_read_pmc_ports(const struct eiois200_dev_port *p,
				  struct pmc_port *pmc_port)
{
	/* switch to logical device pmc1 */
	eio200_write(p, EIOIS200_LDN, EIOIS200_LDN_PMC1);

	/* activate device */
	eio200_write(p, EIOIS200_LDAR, EIOIS200_LDAR_LDACT);

	/* read pmc cmd and data port */
	pmc_port->data = eio200_read(p, EIOIS200_IOBA0H) << 8;
	pmc_port->data |= eio200_read(p, EIOIS200_IOBA0L);
	pmc_port->cmd = eio200_read(p, EIOIS200_IOBA1H) << 8;
	pmc_port->cmd |= eio200_read(p, EIOIS200_IOBA1L);

	/* disable irq */
	eio200_write(p, EIOIS200_IRQCTRL, 0);
}

static EFI_STATUS pmc_wait_iobf(const struct pmc_port *p, UINTN iobf)
{
	UINTN n;

	for (n = 0; n < MAX_STATUS_RETRY; ++n) {
		UINT8 status = inb(p->cmd);

		if (iobf == EIOIS200_PMC_STATUS_IBF) {
			if (!(status & EIOIS200_PMC_STATUS_IBF))
				return EFI_SUCCESS;
		} else {
			if (status & EIOIS200_PMC_STATUS_OBF)
				return EFI_SUCCESS;
		}

		BS->Stall(200);	/* 200 usec */
	}

	return EFI_DEVICE_ERROR;
}

static EFI_STATUS pmc_outb(const struct pmc_port *p, UINT8 value, UINT16 port)
{
	EFI_STATUS err;

	err = pmc_wait_iobf(p, EIOIS200_PMC_STATUS_IBF);
	if (EFI_ERROR(err))
		return err;

	outb(value, port);

	return EFI_SUCCESS;
}

static EFI_STATUS pmc_inb(const struct pmc_port *p, UINT16 port, UINT8 *value)
{
	EFI_STATUS err;

	err = pmc_wait_iobf(p, EIOIS200_PMC_STATUS_OBF);
	if (EFI_ERROR(err))
		return err;

	*value = inb(port);

	return EFI_SUCCESS;
}

static EFI_STATUS pmc_write_data(const struct pmc_port *p, UINT8 value)
{
	return pmc_outb(p, value, p->data);
}

static EFI_STATUS pmc_write_cmd(const struct pmc_port *p, UINT8 cmd)
{
	return pmc_outb(p, cmd, p->cmd);
}

static EFI_STATUS pmc_read_data(const struct pmc_port *p, UINT8 *value)
{
	return pmc_inb(p, p->data, value);
}

static EFI_STATUS pmc_clear(const struct pmc_port *p)
{
	UINT8 status, dummy;

	status = inb(p->cmd);

	if (!(status & EIOIS200_PMC_STATUS_IBF))
		return EFI_SUCCESS;

	/* read previous garbage */
	dummy = inb(p->data);
	(void) dummy;

	BS->Stall(100);

	return EFI_SUCCESS;
}

static EFI_STATUS pmc_cmd_exec(const struct pmc_port *p, UINT8 cmd, UINT8 ctl,
			       UINT8 devid, UINT8 *payload, UINT8 size)
{
	EFI_STATUS err;
	BOOLEAN is_read;
	UINTN n;

	err = pmc_clear(p);
	if (EFI_ERROR(err))
		goto fail;

	err = pmc_write_cmd(p, cmd);
	if (EFI_ERROR(err))
		goto fail;

	err = pmc_write_data(p, ctl);
	if (EFI_ERROR(err))
		goto fail;

	err = pmc_write_data(p, devid);
	if (EFI_ERROR(err))
		goto fail;

	err = pmc_write_data(p, size);
	if (EFI_ERROR(err))
		goto fail;

	is_read = cmd & EIOIS200_FLAG_PMC_READ;

	for (n = 0; n < size; ++n) {
		if (is_read)
			err = pmc_read_data(p, &payload[n]);
		else
			err = pmc_write_data(p, payload[n]);
		if (EFI_ERROR(err))
			goto fail;
	}

	return EFI_SUCCESS;

fail:
	ERROR(L"pmc err: cmd=0x%x ctl=0x%x devid=0x%x size=0x%x\n",
	      cmd, ctl, devid, size);
	return err;
}

static EFI_STATUS pmc_wdt_read(const struct pmc_port *p, UINT8 ctl,
			       UINT8 *payload, UINT8 size)
{
	return pmc_cmd_exec(p, PMC_WDT_CMD_READ, ctl, 0, payload, size);
}

static EFI_STATUS pmc_wdt_write(const struct pmc_port *p, UINT8 ctl,
				UINT8 *payload, UINT8 size)
{
	return pmc_cmd_exec(p, PMC_WDT_CMD_WRITE, ctl, 0, payload, size);
}

static EFI_STATUS pmc_wdt_set_reset_timeout(const struct pmc_port *p,
					    UINT32 msec)
{
	UINT8 payload[4];

	if (msec < PMC_WDT_MIN_TIMEOUT_MS)
		msec = PMC_WDT_MIN_TIMEOUT_MS;
	else if (msec > PMC_WDT_MAX_TIMEOUT_MS)
		msec = PMC_WDT_MAX_TIMEOUT_MS;

	payload[0] = msec & 0xff;
	payload[1] = (msec >> 8) & 0xff;
	payload[2] = (msec >> 16) & 0xff;
	payload[3] = (msec >> 24) & 0xff;

	return pmc_wdt_write(p, WDT_REG_RESET_EVT_TIME,
			     payload, sizeof(payload));
}

static EFI_STATUS pmc_wdt_start(const struct pmc_port *p)
{
	UINT8 payload = PMC_WDT_CTRL_START;

	return pmc_wdt_write(p, WDT_REG_CONTROL, &payload, 1);
}

static EFI_STATUS pmc_wdt_status(const struct pmc_port *p, UINT8 *status)
{
	EFI_STATUS err;

	err = pmc_wdt_read(p, WDT_REG_STATUS, status, 1);
	if (EFI_ERROR(err))
		return err;

	return EFI_SUCCESS;
}

static EFI_STATUS init(EFI_PCI_IO __attribute__((unused)) * pci_io,
		       UINT16 __attribute__((unused)) pci_vendor_id,
		       UINT16 __attribute__((unused)) pci_device_id,
		       UINTN timeout)
{
	SMBIOS_STRUCTURE_TABLE *smbios_table;
	SMBIOS_STRUCTURE_POINTER smbios_struct;
	CHAR8 *smbios_string;
	const struct eiois200_dev_port *eport;
	struct pmc_port pmc;
	UINT8 status;
	EFI_STATUS err;

	/* We do not use PCI, and machines may have many PCI devices */
	if (probed_before)
		return EFI_UNSUPPORTED;
	probed_before = TRUE;

	err = LibGetSystemConfigurationTable(&SMBIOSTableGuid,
					     (VOID **)&smbios_table);
	if (EFI_ERROR(err))
		return err;

	smbios_struct = smbios_find_struct(smbios_table, SMBIOS_TYPE_2);
	if (smbios_struct.Raw == NULL)
		return EFI_UNSUPPORTED;

	/* get manufacturer string */
	smbios_string = LibGetSmbiosString(&smbios_struct, 1);
	if (!smbios_string)
		return EFI_UNSUPPORTED;

	DBG(L"Base board manufacturer: %a\n", smbios_string);

	if (CompareMem(adv_manuf, smbios_string, sizeof(adv_manuf)) != 0)
		return EFI_UNSUPPORTED;

	/* get product name string */
	smbios_string = LibGetSmbiosString(&smbios_struct, 2);
	if (!smbios_string)
		return EFI_UNSUPPORTED;

	DBG(L"Base board product name: %a\n", smbios_string);

	if (CompareMem(adv_product, smbios_string, sizeof(adv_product)) != 0)
		return EFI_UNSUPPORTED;

	eport = eio200_find();
	if (!eport)
		return EFI_UNSUPPORTED;

	/* from here, port is unlocked */

	DBG(L"EIO200 EC detected at index=0x%x,data=0x%x\n",
	    eport->index_port, eport->data_port);

	eio200_enable(eport);

	DBG(L"EIO200 EC enabled\n");

	eio200_read_pmc_ports(eport, &pmc);

	DBG(L"EIO200 PMC at cmd=0x%x,data=0x%x\n",
	    pmc.cmd, pmc.data);

	err = pmc_wdt_status(&pmc, &status);
	if (EFI_ERROR(err))
		goto end;

	if (status & WDT_STA_AVAILABLE &&
	    status & WDT_STA_RESET)
		INFO(L"Detected EIO200 WDT (status: 0x%02x)\n",
		     status);
	else {
		ERROR(L"Detected Unknown EIO200 WDT\n");
		err = EFI_UNSUPPORTED;
		goto end;
	}

	err = pmc_wdt_set_reset_timeout(&pmc, timeout * 1000);
	if (EFI_ERROR(err))
		goto end;

	err = pmc_wdt_start(&pmc);
end:
	eio200_exit(eport);

	return err;
}

WATCHDOG_REGISTER(init);
