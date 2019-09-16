removed unneeded files

soloud_opensles.cpp:
added #import <string.h> for memset <- do i need to do in this version???
renamed mix_s16 to mixSigned16 <- seems to be done in this version

soloud_vita_homebrew.cpp:
moved

#include <psp2/audioout.h>
#include <psp2/kernel/threadmgr.h>
#include <stdio.h>

below 
#else
so that include doesn't fail