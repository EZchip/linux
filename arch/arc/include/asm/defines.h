#ifndef __ARC_ASM_DEFINES_H__
#define __ARC_ASM_DEFINES_H__

#if defined(CONFIG_ARC_MMU_V1)
#define CONFIG_ARC_MMU_VER 1

#elif defined(CONFIG_ARC_MMU_V2)
#define CONFIG_ARC_MMU_VER 2

#elif defined(CONFIG_ARC_MMU_V3)
#define CONFIG_ARC_MMU_VER 3

#endif

#endif  /* __ARC_ASM_DEFINES_H__ */
