/*
 * Copyright (c) 2010, Gerard Lledó Vives, gerard.lledo@gmail.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */

#include "common.h"
#include "ext4_super.h"
#include "logging.h"
#include "disk.h"

#define BOOT_SECTOR_SIZE            0x400
#define GROUP_DESC_MIN_SIZE         0x20

static struct ext4_super_block *super;
static struct ext4_group_desc *gdesc_table;


static uint32_t super_size(void)
{
    return sizeof(struct ext4_super_block);
}

static uint64_t super_block_group_size(void)
{   
    return BLOCKS2BYTES(super->s_blocks_per_group);
}

static uint32_t super_n_block_groups(void)
{
    return super->s_blocks_count_lo / super->s_blocks_per_group;
}

static uint32_t super_group_desc_size(void)
{
    if (!super->s_desc_size) return GROUP_DESC_MIN_SIZE;
    else return sizeof(struct ext4_group_desc);
}

static uint16_t super_magic(void)
{
    return super->s_magic;
}

uint32_t super_block_size(void) {
    return ((uint64_t)1) << (super->s_log_block_size + 10);
}

uint32_t super_inodes_per_group(void)
{
    return super->s_inodes_per_group;
}

uint32_t super_inode_size(void)
{
    return super->s_inode_size;
}


int super_fill(void)
{
    super = malloc(super_size());
    disk_read(BOOT_SECTOR_SIZE, sizeof(struct ext4_super_block), super);

    if (super_magic() != 0xEF53) {
        ERR("Partition doesn't contain EXT4 filesystem");
        return -1;
    }

    INFO("BLOCK SIZE: %i", super_block_size());
    INFO("BLOCK GROUP SIZE: %i", super_block_group_size());
    INFO("N BLOCK GROUPS: %i", super_n_block_groups());
    INFO("INODE SIZE: %i", super_inode_size());
    INFO("INODES PER GROUP: %i", super_inodes_per_group());

    return 0;
}

/* FIXME: Handle bg_inode_table_hi when size > GROUP_DESC_MIN_SIZE */
off_t super_group_inode_table_offset(uint32_t inode_num)
{
    int n_group = inode_num / super_inodes_per_group();
    ASSERT(n_group < super_n_block_groups());
    DEBUG("Inode table offset: 0x%x", gdesc_table[n_group].bg_inode_table_lo);
    return BLOCKS2BYTES(gdesc_table[n_group].bg_inode_table_lo);
}

/* struct ext4_group_desc might be bigger than on disk structure, if we are not
 * using big ones.  That info is in the superblock.  Be careful when allocating
 * or manipulating this pointers. */
int super_group_fill(void)
{
    gdesc_table = malloc(sizeof(struct ext4_group_desc) * super_n_block_groups());

    for (int i = 0; i < super_n_block_groups(); i++) {
        off_t bg_off = ALIGN_TO(BOOT_SECTOR_SIZE + super_size(), super_block_size());
        bg_off += i * super_group_desc_size();

        /* disk advances super_group_desc_size(), pointer sizeof(struct...).
         * These values might be different!!! */
        disk_read(bg_off, super_group_desc_size(), &gdesc_table[i]);
    }

    return 0;
}
