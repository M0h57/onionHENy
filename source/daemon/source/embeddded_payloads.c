 __asm__(


	".global shellui_elf_start\n"
	".type   shellui_elf_start, @object\n"
	".align  16\n"
	"shellui_elf_start:\n"
    	".incbin \"../assets/shellui.elf\"\n"
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
    	".incbin \"../assets/fps_elf.elf\"\n"
	"fps_elf_end:\n"
	    ".global fps_elf_size\n"
	    ".type   fps_elf_size, @object\n"
	    ".align  4\n"
	"fps_elf_size:\n"
    	".int    fps_elf_end - fps_elf_start\n"

);
