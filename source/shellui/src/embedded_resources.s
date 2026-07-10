.intel_syntax noprefix
.text

.global toolbox_start, toolbox_end

# Paths relative to this file (shellui/src/) so .incbin works under CMake/Ninja
toolbox_start:
.incbin "../assets/etaHEN_toolbox.sxml"
toolbox_end:
