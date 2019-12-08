/*
 * Bus driver for the GRLIB AHBJTAG core.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by Jiri Gaisler <jiri@gaisler.se>, 2018.
 * Based on fjmem.c
 *
 */

#include <sysdep.h>

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <urjtag/log.h>
#include <urjtag/part.h>
#include <urjtag/bus.h>
#include <urjtag/chain.h>
#include <urjtag/cmd.h>
#include <urjtag/tap.h>
#include <urjtag/data_register.h>
#include <urjtag/tap_register.h>
#include <urjtag/part_instruction.h>

#include "buses.h"
#include "generic_bus.h"

#define AHBJTAG_ADDR_NAME "AINST"
#define AHBJTAG_AREG_NAME "ADDR"
#define AHBJTAG_DATA_NAME "DINST"
#define AHBJTAG_DREG_NAME "DATA"
#define AHBJTAG_MAX_REG_LEN 40

typedef struct
{
    urj_data_register_t *ahbjtag_areg;
    urj_data_register_t *ahbjtag_dreg;
} bus_params_t;

#define AHBJTAG_AREG  ((bus_params_t *) bus->params)->ahbjtag_areg
#define AHBJTAG_DREG  ((bus_params_t *) bus->params)->ahbjtag_dreg

static uint32_t next_waddr = 0;
static uint32_t read_addr = 0;

/**
 * bus->driver->(*new_bus)
 *
 */
static urj_bus_t *
ahbjtag_bus_new (urj_chain_t *chain, const urj_bus_driver_t *driver,
               const urj_param_t *params[])
{
    urj_bus_t *bus = NULL;
    urj_part_t *part;

    bus = urj_bus_generic_new (chain, driver, sizeof (bus_params_t));
    if (bus == NULL)
        return NULL;
    part = bus->part;

    urj_part_instruction_length_set (part, chain->total_instr_len);
    urj_part_data_register_define (part, "ADDR", 35);
    urj_part_instruction_define (part, "AINST", "000010", "ADDR");
    urj_part_data_register_define (part, "DATA", 33);
    urj_part_instruction_define (part, "DINST", "000011", "DATA");

    AHBJTAG_AREG = urj_part_find_data_register (part, AHBJTAG_AREG_NAME);
    AHBJTAG_DREG = urj_part_find_data_register (part, AHBJTAG_DREG_NAME);

    return bus;
}


/**
 * bus->driver->(*free_bus)
 *
 */
static void
ahbjtag_bus_free (urj_bus_t *bus)
{
    urj_data_register_t *dr = AHBJTAG_AREG;

    /* fill all fields with '0'
       -> prepare idle instruction for next startup/detect */
    urj_part_set_instruction (bus->part, AHBJTAG_ADDR_NAME);
    urj_tap_chain_shift_instructions (bus->chain);

    urj_tap_register_fill (dr->in, 0);
    urj_tap_chain_shift_data_registers (bus->chain, 0);

    urj_bus_generic_free (bus);
}

/**
 * bus->driver->(*printinfo)
 *
 */
static void
ahbjtag_bus_printinfo (urj_log_level_t ll, urj_bus_t *bus)
{
    int i;

    for (i = 0; i < bus->chain->parts->len; i++)
        if (bus->part == bus->chain->parts->parts[i])
            break;
    urj_log (ll, _("GRLIB AHB driver via USER registers (JTAG part No. %d)\n"),
            i);
}

/**
 * bus->driver->(*prepare)
 *
 */
static void
ahbjtag_bus_prepare (urj_bus_t *bus)
{
    if (!bus->initialized)
        URJ_BUS_INIT (bus);

    /* ensure AHBJTAG_ADDR is active */
    urj_part_set_instruction (bus->part, AHBJTAG_ADDR_NAME);
    urj_tap_chain_shift_instructions (bus->chain);
}

static int
block_bus_area (urj_bus_t *bus, uint32_t adr, urj_bus_area_t *area)
{
    area->description = NULL;
    area->start = 0;
    area->length = 0xfffffffc;
    area->width = 32;
    return URJ_STATUS_OK;
}

/**
 * bus->driver->(*area)
 *
 */
static int
ahbjtag_bus_area (urj_bus_t *bus, uint32_t adr, urj_bus_area_t *area)
{

    return block_bus_area (bus, adr, area);
}

static void
setup_address_data (urj_bus_t *bus, uint32_t ad, urj_data_register_t *dr)
{
    int idx;

    /* set address/data */
    for (idx = 0; idx < 32; idx++)
    {
        dr->in->data[idx] = ad & 1;
        ad >>= 1;
    }
}

/**
 * bus->driver->(*read_start)
 *
 */
static int
ahbjtag_bus_read_start (urj_bus_t *bus, uint32_t adr)
{
    urj_chain_t *chain = bus->chain;
    urj_data_register_t *dr = AHBJTAG_AREG;

    urj_part_set_instruction (bus->part, AHBJTAG_ADDR_NAME);
    urj_tap_chain_shift_instructions (bus->chain);
    setup_address_data (bus, adr, dr);
    /* select read instruction */
    dr->in->data[32] = 0;
    dr->in->data[33] = 1;
    dr->in->data[34] = 0;

    urj_tap_chain_shift_data_registers (chain, 0);
    next_waddr = 0;
    read_addr = adr;

    return URJ_STATUS_OK;
}

/**
 * bus->driver->(*read_next)
 *
 */
static uint32_t
ahbjtag_bus_read_next (urj_bus_t *bus, uint32_t adr)
{
    urj_chain_t *chain = bus->chain;
    urj_data_register_t *dr = AHBJTAG_DREG;
    uint32_t d;
    int idx;

    urj_part_set_instruction (bus->part, AHBJTAG_DATA_NAME);
    urj_tap_chain_shift_instructions (bus->chain);
    setup_address_data (bus, 0, dr);
    if ((adr & 0x3fc) != 0x3fc)
        dr->in->data[32] = 1;  // auto-increment address
    else
        dr->in->data[32] = 0;  // no auto-increment over 1 Kbyte boundary
    urj_tap_chain_shift_data_registers (chain, 1);

    /* extract data from TDO stream */
    d = 0;
    for (idx = 0; idx < 32; idx++)
        if (dr->out->data[idx])
            d |= 1 << idx;

    /* reload address if at the end of a 1 Kbyte block */
    if ((adr & 0x3fc) == 0x3fc)
        ahbjtag_bus_read_start (bus, adr + 4);

    urj_log (URJ_LOG_LEVEL_DETAIL, _("ahbjtag read : 0x%08x : 0x%08x\n"), adr, d);
    read_addr = adr + 4;

    return d;
}

/**
 * bus->driver->(*read_end)
 *
 */
static uint32_t
ahbjtag_bus_read_end (urj_bus_t *bus)
{
    urj_chain_t *chain = bus->chain;
    urj_data_register_t *dr = AHBJTAG_DREG;
    uint32_t d;
    int idx;

    urj_part_set_instruction (bus->part, AHBJTAG_DATA_NAME);
    urj_tap_chain_shift_instructions (bus->chain);
    dr->in->data[32] = 0;  // cancel auto-increment address

    urj_tap_chain_shift_data_registers (chain, 1);

    /* extract data from TDO stream */
    d = 0;
    for (idx = 0; idx < 32; idx++)
        if (dr->out->data[idx])
            d |= 1 << idx;

    urj_log (URJ_LOG_LEVEL_DETAIL, _("ahbjtag read : 0x%08x : 0x%08x\n"), read_addr, d);
    return d;
}

/**
 * bus->driver->(*write)
 *
 */
static void
ahbjtag_bus_write (urj_bus_t *bus, uint32_t adr, uint32_t data)
{
    urj_chain_t *chain = bus->chain;
    urj_data_register_t *dr = AHBJTAG_AREG;

    urj_log (URJ_LOG_LEVEL_DETAIL, _("ahbjtag write: 0x%08x : 0x%08x\n"), adr, data);

    if ((next_waddr != adr) || ((adr & 0x3fc) == 0))
    {
	urj_part_set_instruction (bus->part, AHBJTAG_ADDR_NAME);
	urj_tap_chain_shift_instructions (bus->chain);
	setup_address_data (bus, adr, dr);

	/* select write instruction */
	dr->in->data[34] = 1;
	dr->in->data[33] = 1;
	dr->in->data[32] = 0;

	urj_tap_chain_shift_data_registers (chain, 0);
    }

    dr = AHBJTAG_DREG;

    urj_part_set_instruction (bus->part, AHBJTAG_DATA_NAME);
    urj_tap_chain_shift_instructions (bus->chain);
    setup_address_data (bus, data, dr);
    dr->in->data[32] = 1;  // auto-increment

    urj_tap_chain_shift_data_registers (chain, 1);
    next_waddr = adr + 4;
}

const urj_bus_driver_t urj_bus_ahbjtag_bus = {
    "ahbjtag",
    N_("GRLIB AHBJTAG driver via USER registers 2 & 3\n"),
    ahbjtag_bus_new,
    ahbjtag_bus_free,
    ahbjtag_bus_printinfo,
    ahbjtag_bus_prepare,
    ahbjtag_bus_area,
    ahbjtag_bus_read_start,
    ahbjtag_bus_read_next,
    ahbjtag_bus_read_end,
    urj_bus_generic_read,
    urj_bus_generic_write_start,
    ahbjtag_bus_write,
    urj_bus_generic_no_init,
    urj_bus_generic_no_enable,
    urj_bus_generic_no_disable,
    URJ_BUS_TYPE_PARALLEL,
};
