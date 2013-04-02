/*
 * sunxi_nand.c
 *
 * Copyright (C) 2013 Qiang Yu <yuq825@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <common.h>
#include <nand.h>
#include <errno.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <asm/arch/dma.h>
#include <asm/arch/clock.h>

#define NFC_REG_CTL               0x01c03000
#define NFC_REG_ST                0x01c03004
#define NFC_REG_INT               0x01c03008
#define NFC_REG_TIMING_CTL        0x01c0300c
#define NFC_REG_TIMING_CFG        0x01c03010
#define NFC_REG_ADDR_LOW          0x01c03014
#define NFC_REG_ADDR_HIGH         0x01c03018
#define NFC_REG_SECTOR_NUM        0x01c0301c
#define NFC_REG_CNT		          0x01c03020
#define NFC_REG_CMD		          0x01c03024
#define NFC_REG_RCMD_SET          0x01c03028
#define NFC_REG_WCMD_SET          0x01c0302c
#define NFC_REG_IO_DATA           0x01c03030
#define NFC_REG_ECC_CTL           0x01c03034
#define NFC_REG_ECC_ST            0x01c03038
#define NFC_REG_DEBUG             0x01c0303c
#define NFC_REG_ECC_CNT0          0x01c03040
#define NFC_REG_ECC_CNT1          0x01c03044
#define NFC_REG_ECC_CNT2          0x01c03048
#define NFC_REG_ECC_CNT3          0x01c0304c
#define NFC_REG_USER_DATA_BASE    0x01c03050
#define NFC_REG_USER_DATA(i)      ( NFC_REG_USER_DATA_BASE + 4 * i )
#define NFC_REG_SPARE_AREA        0x01c030a0
#define NFC_RAM0_BASE             0x01c03400

/*define bit use in NFC_CTL*/
#define NFC_EN					(1 << 0)
#define NFC_RESET				(1 << 1)
#define NFC_BUS_WIDYH			(1 << 2)
#define NFC_RB_SEL				(1 << 3)
#define NFC_CE_SEL				(7 << 24)
#define NFC_CE_CTL				(1 << 6)
#define NFC_CE_CTL1				(1 << 7)
#define NFC_PAGE_SIZE			(0xf << 8)
#define NFC_SAM					(1 << 12)
#define NFC_RAM_METHOD			(1 << 14)
#define NFC_DEBUG_CTL			(1 << 31)

/*define bit use in NFC_ST*/
#define NFC_RB_B2R				(1 << 0)
#define NFC_CMD_INT_FLAG		(1 << 1)
#define NFC_DMA_INT_FLAG		(1 << 2)
#define NFC_CMD_FIFO_STATUS		(1 << 3)
#define NFC_STA					(1 << 4)
#define NFC_NATCH_INT_FLAG		(1 << 5)
#define NFC_RB_STATE0			(1 << 8)
#define NFC_RB_STATE1			(1 << 9)
#define NFC_RB_STATE2			(1 << 10)
#define NFC_RB_STATE3			(1 << 11)

/*define bit use in NFC_INT*/
#define NFC_B2R_INT_ENABLE		(1 << 0)
#define NFC_CMD_INT_ENABLE		(1 << 1)
#define NFC_DMA_INT_ENABLE		(1 << 2)


/*define bit use in NFC_CMD*/
#define NFC_CMD_LOW_BYTE		(0xff << 0)
#define NFC_CMD_HIGH_BYTE		(0xff << 8)
#define NFC_ADR_NUM				(0x7 << 16)
#define NFC_SEND_ADR			(1 << 19)
#define NFC_ACCESS_DIR			(1 << 20)
#define NFC_DATA_TRANS			(1 << 21)
#define NFC_SEND_CMD1			(1 << 22)
#define NFC_WAIT_FLAG			(1 << 23)
#define NFC_SEND_CMD2			(1 << 24)
#define NFC_SEQ					(1 << 25)
#define NFC_DATA_SWAP_METHOD	(1 << 26)
#define NFC_ROW_AUTO_INC		(1 << 27)
#define NFC_SEND_CMD3           (1 << 28)
#define NFC_SEND_CMD4           (1 << 29)
#define NFC_CMD_TYPE			(3 << 30)

/* define bit use in NFC_RCMD_SET*/
#define NFC_READ_CMD			(0xff<< 0)
#define NFC_RANDOM_READ_CMD0 	(0xff << 8)
#define NFC_RANDOM_READ_CMD1 	(0xff << 16)

/*define bit use in NFC_WCMD_SET*/
#define NFC_PROGRAM_CMD			(0xff << 0)
#define NFC_RANDOM_WRITE_CMD	(0xff << 8)
#define NFC_READ_CMD0			(0xff << 16)
#define NFC_READ_CMD1	        (0xff << 24)

/*define bit use in NFC_ECC_CTL*/
#define NFC_ECC_EN				(1 << 0)
#define NFC_ECC_PIPELINE		(1 << 3)
#define NFC_ECC_EXCEPTION       (1 << 4)
#define NFC_ECC_BLOCK_SIZE		(1 << 5)
#define NFC_RANDOM_EN           (1 << 9 )
#define NFC_RANDOM_DIRECTION    (1 << 10 )
#define NFC_ECC_MODE_SHIFT      12
#define NFC_ECC_MODE			(0xf << NFC_ECC_MODE_SHIFT)
#define NFC_RANDOM_SEED         (0x7fff << 16))

#define NFC_IRQ_MAJOR		    13
/*cmd flag bit*/
#define NFC_PAGE_MODE  			0x1
#define NFC_NORMAL_MODE  		0x0

#define NFC_DATA_FETCH 			0x1
#define NFC_NO_DATA_FETCH 		0x0
#define NFC_MAIN_DATA_FETCH 	0x1
#define NFC_SPARE_DATA_FETCH	0X0
#define NFC_WAIT_RB				0x1
#define NFC_NO_WAIT_RB			0x0
#define NFC_IGNORE				0x0

#define NFC_INT_RB				0
#define NFC_INT_CMD				1
#define NFC_INT_DMA				2
#define NFC_INT_BATCh			5


struct nand_chip_param {
	unsigned char id[8];
	int id_len;
	int page_shift;
	int clock_freq; //the highest access frequence of the nand flash chip, based on MHz
	int ecc_mode;   //the Ecc Mode for the nand flash chip, 0: bch-16, 1:bch-28, 2:bch_32
};

struct nand_chip_param nand_chip_param[] = {
	// id, id_len, page_shift clock_freq, ecc_mode
	// SAMSUNG
	//---------------------------------------------------------------------------------------
    { {0xec, 0xf1, 0xff, 0x15, 0xff, 0xff, 0xff, 0xff }, 4,  11,   15,    0 },   // K9F1G08
    { {0xec, 0xf1, 0x00, 0x95, 0xff, 0xff, 0xff, 0xff }, 4,  11,   15,    0 },   // K9F1G08
    { {0xec, 0xda, 0xff, 0x15, 0xff, 0xff, 0xff, 0xff }, 4,  11,   15,    0 },   // K9K2G08
    { {0xec, 0xda, 0x10, 0x95, 0xff, 0xff, 0xff, 0xff }, 4,  11,   15,    0 },   // K9F2G08
    { {0xec, 0xdc, 0xc1, 0x15, 0xff, 0xff, 0xff, 0xff }, 4,  11,   15,    0 },   // K9K4G08
    { {0xec, 0xdc, 0x10, 0x95, 0xff, 0xff, 0xff, 0xff }, 4,  11,   15,    0 },   // K9F4G08
    { {0xec, 0xd3, 0x51, 0x95, 0xff, 0xff, 0xff, 0xff }, 4,  11,   30,    0 },   // K9K8G08
    //---------------------------------------------------------------------------------------
    { {0xec, 0xd3, 0x50, 0xa6, 0xff, 0xff, 0xff, 0xff }, 4,  11,   30,    0 },   // K9F8G08
    { {0xec, 0xd5, 0x51, 0xa6, 0xff, 0xff, 0xff, 0xff }, 4,  11,   30,    0 },   // K9KAG08
    //---------------------------------------------------------------------------------------
    { {0xec, 0xdc, 0x14, 0x25, 0xff, 0xff, 0xff, 0xff }, 4,  11,   20,    0 },   // K9G4G08
    { {0xec, 0xdc, 0x14, 0xa5, 0xff, 0xff, 0xff, 0xff }, 4,  11,   30,    0 },   // K9G4G08
    { {0xec, 0xd3, 0x55, 0x25, 0xff, 0xff, 0xff, 0xff }, 4,  11,   20,    0 },   // K9L8G08
    { {0xec, 0xd3, 0x55, 0xa5, 0xff, 0xff, 0xff, 0xff }, 4,  11,   30,    0 },   // K9L8G08
    { {0xec, 0xd3, 0x14, 0x25, 0xff, 0xff, 0xff, 0xff }, 4,  11,   20,    0 },   // K9G8G08
    { {0xec, 0xd3, 0x14, 0xa5, 0xff, 0xff, 0xff, 0xff }, 4,  11,   30,    0 },   // K9G8G08
    { {0xec, 0xd5, 0x55, 0x25, 0xff, 0xff, 0xff, 0xff }, 4,  11,   30,    0 },   // K9LAG08
    { {0xec, 0xd5, 0x55, 0xa5, 0xff, 0xff, 0xff, 0xff }, 4,  11,   30,    0 },   // K9LAG08
    //---------------------------------------------------------------------------------------
    { {0xec, 0xd5, 0x14, 0xb6, 0xff, 0xff, 0xff, 0xff }, 4,  12,   30,    0 },   // K9GAG08
    { {0xec, 0xd7, 0x55, 0xb6, 0xff, 0xff, 0xff, 0xff }, 4,  12,   30,    0 },   // K9LBG08
    { {0xec, 0xd7, 0xd5, 0x29, 0xff, 0xff, 0xff, 0xff }, 4,  12,   30,    0 },   // K9LBG08
    { {0xec, 0xd7, 0x94, 0x72, 0xff, 0xff, 0xff, 0xff }, 4,  13,   30,    2 },   // K9GBG08
    { {0xec, 0xd5, 0x98, 0x71, 0xff, 0xff, 0xff, 0xff }, 4,  12,   30,    3 },   // K9AAG08

    { {0xec, 0xd5, 0x94, 0x29, 0xff, 0xff, 0xff, 0xff }, 4,  12,   30,    0 },   // K9GAG08U0D
    { {0xec, 0xd5, 0x84, 0x72, 0xff, 0xff, 0xff, 0xff }, 4,  13,   24,    2 },   // K9GAG08U0E
    { {0xec, 0xd3, 0x84, 0x72, 0xff, 0xff, 0xff, 0xff }, 4,  13,   24,    2 },   // K9G8G08U0C
	{ {0xec, 0xd7, 0x94, 0x76, 0xff, 0xff, 0xff, 0xff }, 4,  13,   30,    3 },   // K9GBG08U0A
	{ {0xec, 0xd7, 0x94, 0x7A, 0xff, 0xff, 0xff, 0xff }, 4,  13,   30,    3 },   // K9GBG08U0A

	{ {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff }, 0,  0,    0,     0 },
};

static int read_offset = 0, write_offset = 0;
static int buffer_size = 8192 + 1024;
static char write_buffer[8192 + 1024] __attribute__((aligned(4)));
static char read_buffer[8192 + 1024] __attribute__((aligned(4)));
static int dma_hdle;
static struct nand_ecclayout sunxi_ecclayout;
static int program_column = -1, program_page = -1;

#define NAND_MAX_CLOCK (10 * 1000000)

static void sunxi_nand_set_clock(int hz)
{
	struct sunxi_ccm_reg *const ccm =
		(struct sunxi_ccm_reg *)SUNXI_CCM_BASE;
	int clock = clock_get_pll5();
	int edo_clk = hz * 2;
	int div_n = 0, div_m;
	int nand_clk_divid_ratio = clock / edo_clk;

	if (clock % edo_clk)
		nand_clk_divid_ratio++;
	for (div_m = nand_clk_divid_ratio; div_m > 16 && div_n < 3; div_n++) {
		if (div_m % 2)
			div_m++;
		div_m >>= 1;
	}
	div_m--;
	if (div_m > 15)
		div_m = 15;	/* Overflow */

	/* nand clock source is PLL5 */
	/* TODO: define proper clock sources for NAND reg */
	clrsetbits_le32(&ccm->nand_sclk_cfg, 3 << 24, 2 << 24); /* 2 = PLL5 */
	clrsetbits_le32(&ccm->nand_sclk_cfg, 3 << 16, div_n << 16);
	clrsetbits_le32(&ccm->nand_sclk_cfg, 0xf << 0, div_m << 0);
	setbits_le32(&ccm->nand_sclk_cfg, (1 << 31));
	/* open clock for nand and DMA*/
	setbits_le32(&ccm->ahb_gate0, (1 << AHB_GATE_OFFSET_NAND) | (1 << AHB_GATE_OFFSET_DMA));
	debug("NAND Clock: PLL5=%dHz, divid_ratio=%d, n=%d m=%d, clock=%dHz (target %dHz\n", 
		  clock, nand_clk_divid_ratio, div_n, div_m, (clock>>div_n)/(div_m+1), hz);
}

static void print_nand_clock(void)
{
	struct sunxi_ccm_reg *const ccm =
		(struct sunxi_ccm_reg *)SUNXI_CCM_BASE;
	debug("============= nand clock ==============\n");
	debug("nand_sclk_cfg=%08x ahb_gate0=%08x\n", 
		  readl(&ccm->nand_sclk_cfg), readl(&ccm->ahb_gate0));
}

static void sunxi_nand_set_gpio(void)
{
	struct sunxi_gpio *pio =
	    &((struct sunxi_gpio_reg *)SUNXI_PIO_BASE)->gpio_bank[2];
	writel(0x22222222, &pio->cfg[0]);
	writel(0x22222222, &pio->cfg[1]);
	writel(0x22222222, &pio->cfg[2]);
}

static void print_nand_gpio(void)
{
	struct sunxi_gpio *pio =
	    &((struct sunxi_gpio_reg *)SUNXI_PIO_BASE)->gpio_bank[2];
	debug("============= nand gpio ===============\n");
	debug("cfg0=%08x cfg1=%08x cfg2=%08x\n", 
		  readl(&pio->cfg[0]), readl(&pio->cfg[1]), readl(&pio->cfg[2]));
}

/////////////////////////////////////////////////////////////////
// Utils
//

static inline void wait_cmdfifo_free(void)
{
	int timeout = 0xffff;
	while ((timeout--) && (readl(NFC_REG_ST) & NFC_CMD_FIFO_STATUS));
	if (timeout <= 0) {
		error("wait_cmdfifo_free timeout\n");
	}
}

static inline void wait_cmd_finish(void)
{
	int timeout = 0xffff;
	while((timeout--) && !(readl(NFC_REG_ST) & NFC_CMD_INT_FLAG));
	if (timeout <= 0) {
		error("wait_cmd_finish timeout\n");
		return;
	}
	writel(NFC_CMD_INT_FLAG, NFC_REG_ST);
}

static void select_rb(int rb)
{
	uint32_t ctl;
	// A10 has 2 RB pin
	ctl = readl(NFC_REG_CTL);
	ctl &= ~NFC_RB_SEL;
	ctl |= ((rb & 0x1) << 3);
	writel(ctl, NFC_REG_CTL);
}

// 1 for ready, 0 for not ready
static inline int check_rb_ready(int rb)
{
	return (readl(NFC_REG_ST) & (NFC_RB_STATE0 << (rb & 0x3))) ? 1 : 0;
}

static void enable_random(void)
{
	uint32_t ctl;
	ctl = readl(NFC_REG_ECC_CTL);
	ctl |= NFC_RANDOM_EN;
	writel(ctl, NFC_REG_ECC_CTL);
}

static void disable_random(void)
{
	uint32_t ctl;
	ctl = readl(NFC_REG_ECC_CTL);
	ctl &= ~NFC_RANDOM_EN;
	writel(ctl, NFC_REG_ECC_CTL);
}

static void enable_ecc(int pipline)
{
	uint32_t cfg = readl(NFC_REG_ECC_CTL);
	if (pipline)
		cfg |= NFC_ECC_PIPELINE;
	else
		cfg &= (~NFC_ECC_PIPELINE) & 0xffffffff;

	// if random open, disable exception
	if(cfg & (1 << 9))
	    cfg &= ~(0x1 << 4);
	else
	    cfg |= 1 << 4;

	//cfg |= (1 << 1); 16 bit ecc

	cfg |= NFC_ECC_EN;
	writel(cfg, NFC_REG_ECC_CTL);
}

int check_ecc(int eblock_cnt)
{
	int i;
    int ecc_mode;
	int max_ecc_bit_cnt = 16;
	int cfg;

	ecc_mode = (readl(NFC_REG_ECC_CTL) & NFC_ECC_MODE) >> NFC_ECC_MODE_SHIFT;
	if(ecc_mode == 0)
		max_ecc_bit_cnt = 16;
	if(ecc_mode == 1)
		max_ecc_bit_cnt = 24;
	if(ecc_mode == 2)
		max_ecc_bit_cnt = 28;
	if(ecc_mode == 3)
		max_ecc_bit_cnt = 32;
	if(ecc_mode == 4)
		max_ecc_bit_cnt = 40;
	if(ecc_mode == 5)
		max_ecc_bit_cnt = 48;
	if(ecc_mode == 6)
		max_ecc_bit_cnt = 56;
    if(ecc_mode == 7)
		max_ecc_bit_cnt = 60;
    if(ecc_mode == 8)
		max_ecc_bit_cnt = 64;

	//check ecc error
	cfg = readl(NFC_REG_ECC_ST) & 0xffff;
	for (i = 0; i < eblock_cnt; i++) {
		if (cfg & (1<<i)) {
			error("ECC too many error at %d\n", i);
			return -1;
		}
	}

	//check ecc limit
	for (i = 0; i < eblock_cnt; i += 4) {
		int j, n = (eblock_cnt - i) < 4 ? (eblock_cnt - i) : 4;
		cfg = readl(NFC_REG_ECC_CNT0 + i);
		for (j = 0; j < n; j++, cfg >>= 8) {
			if ((cfg & 0xff) >= max_ecc_bit_cnt - 4) {
				error("ECC limit %d/%d\n", (cfg & 0xff), max_ecc_bit_cnt);
				//return -1;
			}
		}
	}

	return 0;
}

static void disable_ecc(void)
{
	uint32_t cfg = readl(NFC_REG_ECC_CTL);
	cfg &= (~NFC_ECC_EN) & 0xffffffff;
	writel(cfg, NFC_REG_ECC_CTL);
}

void _dma_config_start(__u32 rw, __u32 src_addr, __u32 dst_addr, __u32 len)
{
	__dma_setting_t   dma_param;

	if(rw)  //write
	{
		if(src_addr & 0xC0000000) //DRAM
			dma_param.cfg.src_drq_type = DMAC_CFG_SRC_TYPE_D_SDRAM;
		else  //SRAM
			dma_param.cfg.src_drq_type = DMAC_CFG_SRC_TYPE_D_SRAM;
		dma_param.cfg.src_addr_type = DMAC_CFG_SRC_ADDR_TYPE_LINEAR_MODE;  //linemode
		dma_param.cfg.src_burst_length = DMAC_CFG_SRC_4_BURST;  //burst mode
		dma_param.cfg.src_data_width = DMAC_CFG_SRC_DATA_WIDTH_32BIT;  //32bit

		dma_param.cfg.dst_drq_type = DMAC_CFG_DEST_TYPE_NFC;
		dma_param.cfg.dst_addr_type = DMAC_CFG_DEST_ADDR_TYPE_IO_MODE; //IO mode
		dma_param.cfg.dst_burst_length = DMAC_CFG_DEST_4_BURST; // burst mode
		dma_param.cfg.dst_data_width = DMAC_CFG_DEST_DATA_WIDTH_32BIT; //32 bit

		dma_param.cfg.wait_state = DMAC_CFG_WAIT_1_DMA_CLOCK; // invalid value
		dma_param.cfg.continuous_mode = DMAC_CFG_CONTINUOUS_DISABLE; //no continous

		dma_param.cmt_blk_cnt =  0x7f077f07; //commit register

	}
	else //read
	{
		dma_param.cfg.src_drq_type = DMAC_CFG_SRC_TYPE_NFC;
		dma_param.cfg.src_addr_type = DMAC_CFG_SRC_ADDR_TYPE_IO_MODE;  //IO mode
		dma_param.cfg.src_burst_length = DMAC_CFG_SRC_4_BURST;  //burst mode
		dma_param.cfg.src_data_width = DMAC_CFG_SRC_DATA_WIDTH_32BIT;  //32bit

		if(dst_addr & 0xC0000000) //DRAM
			dma_param.cfg.dst_drq_type = DMAC_CFG_DEST_TYPE_D_SDRAM;
		else  //SRAM
			dma_param.cfg.dst_drq_type = DMAC_CFG_DEST_TYPE_D_SRAM;
		dma_param.cfg.dst_addr_type = DMAC_CFG_DEST_ADDR_TYPE_LINEAR_MODE; //line mode
		dma_param.cfg.dst_burst_length = DMAC_CFG_DEST_4_BURST; // burst mode
		dma_param.cfg.dst_data_width = DMAC_CFG_DEST_DATA_WIDTH_32BIT; //32 bit

		dma_param.cfg.wait_state = DMAC_CFG_WAIT_1_DMA_CLOCK; // invalid value
		dma_param.cfg.continuous_mode = DMAC_CFG_CONTINUOUS_DISABLE; //no continous

		dma_param.cmt_blk_cnt =  0x7f077f07; //commit register
	}

	DMA_Setting(dma_hdle, (void *)&dma_param);

	if((src_addr & 0x01c03000) == 0x01c03000) {
        flush_cache(dst_addr, len);
    }
    else {
        flush_cache(src_addr, len);
    }
	DMA_Start(dma_hdle, src_addr, dst_addr, len);
}


__s32 _wait_dma_end(void)
{
	__s32 timeout = 0xffff;

	while ((timeout--) && DMA_QueryStatus(dma_hdle));
	if (timeout <= 0)
		return -1;

	return 0;
}

/////////////////////////////////////////////////////////////////
// NFC
//

static void nfc_select_chip(struct mtd_info *mtd, int chip)
{
	uint32_t ctl;
	// A10 has 8 CE pin to support 8 flash chips
    ctl = readl(NFC_REG_CTL);
    ctl &= ~NFC_CE_SEL;
	ctl |= ((chip & 7) << 24);
    writel(ctl, NFC_REG_CTL);
}

static void nfc_cmdfunc(struct mtd_info *mtd, unsigned command, int column,
						int page_addr)
{
	int i;
	uint32_t cfg = command;
	int read_size, write_size, do_enable_ecc = 0;
	int addr_cycle, wait_rb_flag, byte_count, sector_count;
	addr_cycle = wait_rb_flag = byte_count = sector_count = 0;

	//debug("command %x ...\n", command);
	wait_cmdfifo_free();

	// switch to AHB
	writel(readl(NFC_REG_CTL) & ~NFC_RAM_METHOD, NFC_REG_CTL);

	switch (command) {
	case NAND_CMD_RESET:
	case NAND_CMD_ERASE2:
		break;
	case NAND_CMD_READID:
		addr_cycle = 1;
		// read 8 byte ID
		byte_count = 8;
		break;
	case NAND_CMD_PARAM:
		addr_cycle = 1;
		byte_count = 1024;
		wait_rb_flag = 1;
		break;
	case NAND_CMD_RNDOUT:
		addr_cycle = 2;
		writel(0xE0, NFC_REG_RCMD_SET);
		byte_count = mtd->oobsize;
		cfg |= NFC_SEQ | NFC_SEND_CMD2;
		wait_rb_flag = 1;
		break;
	case NAND_CMD_READOOB:
	case NAND_CMD_READ0:
		if (command == NAND_CMD_READOOB) {
			cfg = NAND_CMD_READ0;
			// sector num to read
			sector_count = 1024 / 1024;
			read_size = 1024;
			// OOB offset
			column += mtd->writesize;
		}
		else {
			sector_count = mtd->writesize / 1024;
			read_size = mtd->writesize;
			do_enable_ecc = 1;
			debug("cmdfunc read %d %d\n", column, page_addr);
		}
			
		//access NFC internal RAM by DMA bus
		writel(readl(NFC_REG_CTL) | NFC_RAM_METHOD, NFC_REG_CTL);
		// if the size is smaller than NFC_REG_SECTOR_NUM, read command won't finish
		// does that means the data read out (by DMA through random data output) hasn't finish?
		_dma_config_start(0, NFC_REG_IO_DATA, (__u32)read_buffer, read_size);
		addr_cycle = 5;
		// RAM0 is 1K size
		byte_count =1024;
		wait_rb_flag = 1;
		// 0x30 for 2nd cycle of read page
		// 0x05+0xe0 is the random data output command
		writel(0x00e00530, NFC_REG_RCMD_SET);
		// NFC_SEND_CMD1 for the command 1nd cycle enable
		// NFC_SEND_CMD2 for the command 2nd cycle enable
		// NFC_SEND_CMD3 & NFC_SEND_CMD4 for NFC_READ_CMD0 & NFC_READ_CMD1
		cfg |= NFC_SEND_CMD2 | NFC_DATA_SWAP_METHOD;
		// 3 - ?
		// 2 - page command
		// 1 - spare command?
		// 0 - normal command
		cfg |= 2 << 30;
		break;
	case NAND_CMD_ERASE1:
		addr_cycle = 3;
		//debug("cmdfunc earse block %d\n", page_addr);
		break;
	case NAND_CMD_SEQIN:	
		program_column = column;
		program_page = page_addr;
		write_offset = 0;
		return;
	case NAND_CMD_PAGEPROG:
		cfg = NAND_CMD_SEQIN;
		addr_cycle = 5;
		column = program_column;
		page_addr = program_page;
		// for write OOB
		if (column == mtd->writesize) {
			sector_count = 1024 /1024;
			write_size = 1024;
		}
		else if (column == 0) {
			sector_count = mtd->writesize / 1024;
			do_enable_ecc = 1;
			write_size = mtd->writesize;
			for (i = 0; i < sector_count; i++)
				writel(*((unsigned int *)(write_buffer + mtd->writesize) + i), NFC_REG_USER_DATA(i));
		}
		else {
			error("program unsupported column %d %d\n", column, page_addr);
			return;
		}

		//access NFC internal RAM by DMA bus
		writel(readl(NFC_REG_CTL) | NFC_RAM_METHOD, NFC_REG_CTL);
		_dma_config_start(1, (__u32)write_buffer, NFC_REG_IO_DATA, write_size);
		// RAM0 is 1K size
		byte_count =1024;
		writel(0x00008510, NFC_REG_WCMD_SET);
		cfg |= NFC_SEND_CMD2 | NFC_DATA_SWAP_METHOD | NFC_ACCESS_DIR;
		cfg |= 2 << 30;
		if (column != 0) {
			debug("cmdfunc program %d %d with %x %x %x\n", column, page_addr, 
					 write_buffer[0], write_buffer[1], write_buffer[2]);
		}
		break;
	case NAND_CMD_STATUS:
		byte_count = 1;
		break;
	default:
		error("unknown command\n");
		return;
	}

	// address cycle
	if (addr_cycle) {
		uint32_t low = 0;
		uint32_t high = 0;
		switch (addr_cycle) {
		case 2:
			low = column & 0xffff;
			break;
		case 3:
			low = page_addr & 0xffffff;
			break;
		case 5:
			high = (page_addr >> 16) & 0xff;
		case 4:
			low = (column & 0xffff) | (page_addr << 16);
			break;
		}
		writel(low, NFC_REG_ADDR_LOW);
		writel(high, NFC_REG_ADDR_HIGH);
		cfg |= NFC_SEND_ADR;
		cfg |= ((addr_cycle - 1) << 16);
	}

	// command will wait until the RB ready to mark finish?
	if (wait_rb_flag)
		cfg |= NFC_WAIT_FLAG;

	// will fetch data
	if (byte_count) {
		cfg |= NFC_DATA_TRANS;
		writel(byte_count, NFC_REG_CNT);
	}

	// set sectors
	if (sector_count)
		writel(sector_count, NFC_REG_SECTOR_NUM);

	// enable ecc
	if (do_enable_ecc)
		enable_ecc(1);

	// send command
	cfg |= NFC_SEND_CMD1;
	writel(cfg, NFC_REG_CMD);

	switch (command) {
	case NAND_CMD_READ0:
	case NAND_CMD_READOOB:
	case NAND_CMD_PAGEPROG:
		_wait_dma_end();
		break;
	}

	// wait command send complete
	wait_cmdfifo_free();
	wait_cmd_finish();

	// reset will wait for RB ready
	switch (command) {
	case NAND_CMD_RESET:
		// wait rb0 ready
		select_rb(0);
		while (!check_rb_ready(0));
		// wait rb1 ready
		select_rb(1);
		while (!check_rb_ready(1));
		// select rb 0 back
		select_rb(0);
		break;
	case NAND_CMD_READ0:
		for (i = 0; i < sector_count; i++)
			*((unsigned int *)(read_buffer + mtd->writesize) + i) = readl(NFC_REG_USER_DATA(i));
		break;
	}

	if (do_enable_ecc)
		disable_ecc();

	//debug("done\n");

	// read write offset
	read_offset = 0;
}

static uint8_t nfc_read_byte(struct mtd_info *mtd)
{
	return readb(NFC_RAM0_BASE + read_offset++);
}

static int nfc_dev_ready(struct mtd_info *mtd)
{
	return check_rb_ready(0);
}

static void nfc_write_buf(struct mtd_info *mtd, const uint8_t *buf, int len)
{
	if (write_offset + len > buffer_size) {
		error("write too much offset=%d len=%d buffer size=%d\n",
				 write_offset, len, buffer_size);
		return;
	}
	memcpy(write_buffer + write_offset, buf, len);
	write_offset += len;
}

static void nfc_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
	if (read_offset + len > buffer_size) {
		error("read too much offset=%d len=%d buffer size=%d\n", 
				 read_offset, len, buffer_size);
		return;
	}
	memcpy(buf, read_buffer + read_offset, len);
	read_offset += len;
}

static int get_chip_status(struct mtd_info *mtd)
{
	struct nand_chip *nand = mtd->priv;
	nand->cmdfunc(mtd, NAND_CMD_STATUS, -1, -1);
	return nand->read_byte(mtd);
}

// For erase and program command to wait for chip ready
static int nfc_wait(struct mtd_info *mtd, struct nand_chip *chip)
{
	while (!check_rb_ready(0));
	return get_chip_status(mtd);
}

static void nfc_ecc_hwctl(struct mtd_info *mtd, int mode)
{

}

static int nfc_ecc_calculate(struct mtd_info *mtd, const uint8_t *dat, uint8_t *ecc_code)
{
	return 0;
}

static int nfc_ecc_correct(struct mtd_info *mtd, uint8_t *dat, uint8_t *read_ecc, uint8_t *calc_ecc)
{
	if (check_ecc(mtd->writesize / 1024)) {
		error("ECC check fail\n");
		return -1;
	}
	return 0;
}

static void print_nand_nfc(void)
{
	debug("=============== nand bfc ===============\n");
	debug("CTL=%08x ST=%08x INT=%08x TIMING_CTL=%08x TIMING_CFG=%08x\n",
		  readl(NFC_REG_CTL), readl(NFC_REG_ST), readl(NFC_REG_INT), 
		  readl(NFC_REG_TIMING_CTL), readl(NFC_REG_TIMING_CFG));
	debug("ADDR_LOW=%08x ADDR_HIGH=%08x SECTOR_NUM=%08x CNT=%08x\n",
		  readl(NFC_REG_ADDR_LOW), readl(NFC_REG_ADDR_HIGH), 
		  readl(NFC_REG_SECTOR_NUM), readl(NFC_REG_CNT));
	debug("CMD=%08x RCMD=%08x WCMD=%08x ECC_CTL=%08x ECC_ST=%08x\n",
		  readl(NFC_REG_CMD), readl(NFC_REG_RCMD_SET), readl(NFC_REG_WCMD_SET),
		  readl(NFC_REG_ECC_CTL), readl(NFC_REG_ECC_ST));
	debug("SPARE_AREA=%08x\n", readl(NFC_REG_SPARE_AREA));
}

static void print_nand_regs(void)
{
	print_nand_clock();
	print_nand_gpio();
	print_nand_nfc();
}

int board_nand_init(struct nand_chip *nand)
{
	u32 ctl;
	int i, j;
	uint8_t id[8];
	struct nand_chip_param *chip_param = NULL;

	debug("board_nand_init start\n");

	// set init clock
	sunxi_nand_set_clock(NAND_MAX_CLOCK);

	// set gpio
	sunxi_nand_set_gpio();

	//print_nand_regs();

	// reset NFC
	ctl = readl(NFC_REG_CTL);
	ctl |= NFC_RESET;
	writel(ctl, NFC_REG_CTL);
	while(readl(NFC_REG_CTL) & NFC_RESET);

	// enable NFC
	ctl = NFC_EN;
	writel(ctl, NFC_REG_CTL);

	// serial_access_mode = 1
	ctl = (1 << 8);
	writel(ctl, NFC_REG_TIMING_CTL);

	// reset nand chip
	nfc_cmdfunc(NULL, NAND_CMD_RESET, -1, -1);
	// read nand chip id
	nfc_cmdfunc(NULL, NAND_CMD_READID, 0, -1);
	for (i = 0; i < 8; i++)
		id[i] = nfc_read_byte(NULL);

	// find chip
	for (i = 0; nand_chip_param[i].id_len; i++) {
		int find = 1;
		for (j = 0; j < nand_chip_param[i].id_len; j++) {
			if (id[j] != nand_chip_param[i].id[j]) {
				find = 0;
				break;
			}
		}
		if (find) {
			chip_param = &nand_chip_param[i];
			debug("find nand chip in sunxi database\n");
			break;
		}
	}

	// not find
	if (chip_param == NULL) {
		error("can't find nand chip in sunxi database\n");
		return -ENODEV;
	}

	// set final NFC clock freq
	if (chip_param->clock_freq > 30)
		chip_param->clock_freq = 30;
	sunxi_nand_set_clock(chip_param->clock_freq * 1000000);
	debug("set final clock freq to %dMHz\n", chip_param->clock_freq);

	// disable interrupt
	writel(0, NFC_REG_INT);
	// clear interrupt
	writel(readl(NFC_REG_ST), NFC_REG_ST);

	// set ECC mode
	ctl = readl(NFC_REG_ECC_CTL);
	ctl &= ~NFC_ECC_MODE;
	ctl &= ~NFC_ECC_MODE;
	ctl |= chip_param->ecc_mode << NFC_ECC_MODE_SHIFT;
	writel(ctl, NFC_REG_ECC_CTL);

	// enable NFC
	ctl = NFC_EN;

	// Page size
	if (chip_param->page_shift > 14 || chip_param->page_shift < 10) {
		error("Flash chip page shift out of range %d\n", chip_param->page_shift);
		return -EINVAL;
	}
	// 0 for 1K
	ctl |= ((chip_param->page_shift - 10) & 0xf) << 8;
	writel(ctl, NFC_REG_CTL);

	writel(0xff, NFC_REG_TIMING_CFG);
	writel(1 << chip_param->page_shift, NFC_REG_SPARE_AREA);

	// disable random
	disable_random();

	// setup ECC layout
	sunxi_ecclayout.eccbytes = 0;
	sunxi_ecclayout.oobavail = (1 << chip_param->page_shift) / 1024 * 4 - 2;
	sunxi_ecclayout.oobfree->offset = 1;
	sunxi_ecclayout.oobfree->length = (1 << chip_param->page_shift) / 1024 * 4 - 2;
	nand->ecc.layout = &sunxi_ecclayout;
	nand->ecc.size = (1 << chip_param->page_shift);
	nand->ecc.bytes = 0;

	// set buffer size
	buffer_size = (1 << chip_param->page_shift) + 1024;

	// setup DMA
	dma_hdle = DMA_Request(DMAC_DMATYPE_DEDICATED);
	if (dma_hdle == 0) {
		error("request DMA fail\n");
		return -ENODEV;
	}
	print_nand_dma(dma_hdle);

	nand->ecc.mode = NAND_ECC_HW;
	nand->ecc.hwctl = nfc_ecc_hwctl;
	nand->ecc.calculate = nfc_ecc_calculate;
	nand->ecc.correct = nfc_ecc_correct;
	nand->select_chip = nfc_select_chip;
	nand->dev_ready = nfc_dev_ready;
	nand->cmdfunc = nfc_cmdfunc;
	nand->read_byte = nfc_read_byte;
	nand->read_buf = nfc_read_buf;
	nand->write_buf = nfc_write_buf;
	nand->waitfunc = nfc_wait;
	//nand->bbt_options = NAND_BBT_USE_FLASH;

	debug("board_nand_init finish\n");

	//print_nand_regs();

	return 0;
}
