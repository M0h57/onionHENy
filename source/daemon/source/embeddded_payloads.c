#ifndef ONIONHEN_SHELLUI_ELF
#define ONIONHEN_SHELLUI_ELF "../assets/shellui.elf"
#endif
#ifndef ONIONHEN_FPS_ELF
#define ONIONHEN_FPS_ELF "../assets/fps_elf.elf"
#endif
#ifndef ONIONHEN_UTIL_ELF
#define ONIONHEN_UTIL_ELF "../../bin/util.elf"
#endif

 __asm__(


	".global shellui_elf_start\n"
	".type   shellui_elf_start, @object\n"
	".align  16\n"
	"shellui_elf_start:\n"
	    ".incbin \"" ONIONHEN_SHELLUI_ELF "\"\n"
	"shellui_elf_end:\n"
	    ".global shellui_elf_size\n"
	    ".type   shellui_elf_size, @object\n"
	    ".align  4\n"
	"shellui_prx_size:\n"
    	".int    shellui_elf_end - shellui_elf_start\n"

	".global fps_elf_start\n"
	".type   fps_elf_start, @object\n"
	".align  16\n"
	"fps_elf_start:\n"
	    ".incbin \"" ONIONHEN_FPS_ELF "\"\n"
	"fps_elf_end:\n"
	    ".global fps_elf_size\n"
	    ".type   fps_elf_size, @object\n"
	    ".align  4\n"
	"fps_elf_size:\n"
    	".int    fps_elf_end - fps_elf_start\n"

	/* Keep util in memory so the daemon watchdog can restart it without disk. */
	".global util_elf_start\n"
	".type   util_elf_start, @object\n"
	".align  16\n"
	"util_elf_start:\n"
		".incbin \"" ONIONHEN_UTIL_ELF "\"\n"
	"util_elf_end:\n"
		".global util_elf_size\n"
		".type   util_elf_size, @object\n"
		".align  4\n"
	"util_elf_size:\n"
		".int    util_elf_end - util_elf_start\n"

);
