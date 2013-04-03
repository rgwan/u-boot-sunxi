#include <common.h>
#include <nand.h>

static struct mtd_info mtd = {0};
static struct nand_chip nand = {0};

static int nand_spl_is_badblock(unsigned int offs)
{
	unsigned char buff[2];
	offs &= ~(mtd.erasesize - 1);
	nand.cmdfunc(&mtd, NAND_CMD_READOOB, 0, offs / mtd.writesize);
	nand.read_buf(&mtd, buff, 2);
	if (buff[0] == 0xff)
		return 0;
	else
		return 1;
}

static int nand_spl_read(uint32_t offs, uint32_t size, void *dst)
{
	uint32_t len;

	// offs must be page aligned
	while (size > 0) {
		nand.cmdfunc(&mtd, NAND_CMD_READ0, 0, offs / mtd.writesize);
		len = size > mtd.writesize ? mtd.writesize : size;
		nand.read_buf(&mtd, dst, len);
		offs += len;
		dst += len;
		size -= len;
	}
	return 0;
}

int nand_spl_load_image(uint32_t offs, unsigned int size, void *dst)
{
	uint32_t to, len, bound;

	debug("nand spl load image from %x to %x size %x\n", offs, dst, size);

	while (size > 0) {
		if (nand_spl_is_badblock(offs)) {
			debug("nand spl block %x is bad\n", offs);
			offs += mtd.erasesize;
			continue;
		}
			
		to = roundup(offs, mtd.erasesize);
		bound = (to == offs) ? mtd.erasesize : (to - offs);
		len = bound > size ? size : bound;
		nand_spl_read(offs, len, dst);
		offs += len;
		dst += len;
		size -= len;
	}

	return 0;
}

/* nand_init() - initialize data to make nand usable by SPL */
void nand_init(void)
{
	mtd.priv = &nand;

	if (board_nand_init(&nand))
		return;

	mtd.erasesize = CONFIG_SYS_NAND_BLOCK_SIZE;
	mtd.writesize = CONFIG_SYS_NAND_PAGE_SIZE;
	mtd.oobsize = CONFIG_SYS_NAND_OOBSIZE;

	nand.select_chip(&mtd, 0);
}

/* Unselect after operation */
void nand_deselect(void)
{

}

