/*
 * TriCore Baseboard System emulation.
 *
 * Copyright (c) 2013-2014 Bastian Koppelmann C-Lab/University Paderborn
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */


#include "qemu/osdep.h"
#include "qemu/units.h"
#include "qapi/error.h"
#include "cpu.h"
#include "net/net.h"
#include "hw/boards.h"
#include "hw/loader.h"
#include "exec/address-spaces.h"
#include "elf.h"
#include "hw/tricore/tricore.h"
#include "qemu/error-report.h"


/* Board init.  */

static struct tricore_boot_info tricoretb_binfo;

#define SBOOT_SIZE_BYTES 0x1C000
// RSA sigcheck entry point
#define ENTRY_POINT 0x80015F60

static void tricore_load_sboot(CPUTriCoreState *env, MemoryRegion *pmem0)
{
    FILE *sboot_file = fopen(tricoretb_binfo.kernel_filename, "rb");
    uint8_t *sboot_buf = malloc(SBOOT_SIZE_BYTES);
    fread(sboot_buf, SBOOT_SIZE_BYTES, 1, sboot_file);
    uint8_t *pmem0_ptr = memory_region_get_ram_ptr(pmem0);
    memcpy(pmem0_ptr, sboot_buf, SBOOT_SIZE_BYTES);
    env->PC = ENTRY_POINT;
}

static void tricore_s18_init(MachineState *machine, int board_id)
{
    TriCoreCPU *cpu;
    CPUTriCoreState *env;

    MemoryRegion *sysmem = get_system_memory();
    MemoryRegion *pmu_pflash0 = g_new(MemoryRegion, 1);
    MemoryRegion *pmu_pflash1 = g_new(MemoryRegion, 1);
    MemoryRegion *pmu_pflash0_uncache = g_new(MemoryRegion, 1);
    MemoryRegion *pmu_pflash1_uncache = g_new(MemoryRegion, 1);
    MemoryRegion *pmu_dflash0 = g_new(MemoryRegion, 1);
    MemoryRegion *pmu_dflash1 = g_new(MemoryRegion, 1);
    MemoryRegion *brom = g_new(MemoryRegion, 1);
    MemoryRegion *pmi_pspr = g_new(MemoryRegion, 1);
    MemoryRegion *dmi_dspr = g_new(MemoryRegion, 1);
    MemoryRegion *pcp_cmem = g_new(MemoryRegion, 1);
    MemoryRegion *pcp_pram = g_new(MemoryRegion, 1);
    MemoryRegion *lmu_ram = g_new(MemoryRegion, 1);

    cpu = TRICORE_CPU(cpu_create(machine->cpu_type));
    env = &cpu->env;
    memory_region_init_ram(pmu_pflash0, NULL, "PMU_PFLASH0",
                           2 * MiB, &error_fatal);
    memory_region_init_ram(pmu_pflash1, NULL, "PMU_PFLASH1",
                           2 * MiB, &error_fatal);
    memory_region_init_alias(pmu_pflash0_uncache, NULL, "PMU_PFLASH0_UNCACHED", pmu_pflash0, 0,
                           2 * MiB);
    memory_region_init_alias(pmu_pflash1_uncache, NULL, "PMU_PFLASH1_UNCACHED", pmu_pflash1, 0,
                           2 * MiB);
    memory_region_init_ram(pmu_dflash0, NULL, "PMU_DFLASH0", 64 * KiB,
                           &error_fatal);
    memory_region_init_ram(pmu_dflash1, NULL, "PMU_DFLASH1", 64 * KiB,
                           &error_fatal);
    memory_region_init_ram(brom, NULL, "BROM",
                           16 * KiB, &error_fatal);
    memory_region_init_ram(lmu_ram, NULL, "LMURAM",
                           128 * KiB, &error_fatal);
    memory_region_init_ram(pmi_pspr, NULL, "PMI_PSPR",
                           32 * KiB, &error_fatal);
    memory_region_init_ram(dmi_dspr, NULL, "DMI_DSPR",
                           128 * KiB, &error_fatal);
    memory_region_init_ram(pcp_cmem, NULL, "PCP_CMEM",
                           32 * KiB, &error_fatal);
    memory_region_init_ram(pcp_pram, NULL, "PCP_PRAM",
                           16 * KiB, &error_fatal);

    memory_region_add_subregion(sysmem, 0x80000000, pmu_pflash0);
    memory_region_add_subregion(sysmem, 0x80800000, pmu_pflash1);
    memory_region_add_subregion(sysmem, 0xA0000000, pmu_pflash0_uncache);
    memory_region_add_subregion(sysmem, 0xA0800000, pmu_pflash1_uncache);
    memory_region_add_subregion(sysmem, 0xaf000000, pmu_dflash0);
    memory_region_add_subregion(sysmem, 0xaf080000, pmu_dflash1);
    memory_region_add_subregion(sysmem, 0xafffc000, brom);
    memory_region_add_subregion(sysmem, 0xb0000000, lmu_ram);
    memory_region_add_subregion(sysmem, 0xc0000000, pmi_pspr);
    memory_region_add_subregion(sysmem, 0xd0000000, dmi_dspr);
    memory_region_add_subregion(sysmem, 0xf0050000, pcp_cmem);
    memory_region_add_subregion(sysmem, 0xf0060000, pcp_pram);

    tricoretb_binfo.ram_size = machine->ram_size;
    tricoretb_binfo.kernel_filename = machine->kernel_filename;

    if (machine->kernel_filename) {
        tricore_load_sboot(env, pmu_pflash0);
    }
}

static void s18_init(MachineState *machine)
{
    tricore_s18_init(machine, 0x183);
}

static void tc_s18_machine_init(MachineClass *mc)
{
    mc->desc = "TC1791 as used in Simos18";
    mc->init = s18_init;
    mc->default_cpu_type = TRICORE_CPU_TYPE_NAME("tc1791");
}

DEFINE_MACHINE("tricore_s18", tc_s18_machine_init)
