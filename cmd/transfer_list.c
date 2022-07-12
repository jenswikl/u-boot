#include <common.h>
#include <command.h>
#include <transfer_list.h>
#include <asm/global_data.h>

DECLARE_GLOBAL_DATA_PTR;

static int do_tl(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{
	struct transfer_list *tl = gd->transfer_list;

	if (argc < 2)
		return CMD_RET_USAGE;

	if (strncmp(argv[1], "li", 2) == 0) {
		struct transfer_entry *te = NULL;

		printf("Transfer list:\n");
		printf("version    0x%x\n", tl->version);
		printf("length     0x%x\n", tl->length);
		printf("max_length 0x%x\n", tl->max_length);
		while (true) {
			te = transfer_list_next(tl, te);
			if (!te)
				break;
			printf("Entry:\n");
			printf("tag_id     0x%x\n", te->tag_id);
			printf("data_size  0x%x\n", te->data_size);
			printf("data_addr  0x%lx\n",
			       (unsigned long)transfer_list_data(te));
		}

		return CMD_RET_SUCCESS;
	}

	/* Unrecognized command */
	return CMD_RET_USAGE;
}

#ifdef CONFIG_SYS_LONGHELP
static char tl_help_text[] =
	"list - List all entries in the Firmware Transfer list\n";
#endif
U_BOOT_CMD(
	tl,	255,	0,	do_tl,
	"Firmware transfer list utility commands", tl_help_text
);
