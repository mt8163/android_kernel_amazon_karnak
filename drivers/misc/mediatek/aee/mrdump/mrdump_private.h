#if !defined(__MRDUMP_PRIVATE_H__)


#include <asm/memory.h>
#include <asm-generic/sections.h>

#ifdef CONFIG_ARCH64
#define mrdump_virt_addr_valid(kaddr) ((((void *)(kaddr) >= (void *)PAGE_OFFSET && \
					(void *)(kaddr) < (void *)high_memory) || \
					((void *)(kaddr) >= (void *)KIMAGE_VADDR && \
					(void *)(kaddr) < (void *)_end)) && \
					pfn_valid(__pa(kaddr) >> PAGE_SHIFT))
#else
#define mrdump_virt_addr_valid(kaddr) ((void *)(kaddr) >= (void *)PAGE_OFFSET && \
					(void *)(kaddr) < (void *)high_memory && \
					pfn_valid(__pa(kaddr) >> PAGE_SHIFT))
#endif


struct pt_regs;

void mrdump_save_current_backtrace(struct pt_regs *regs);
void mrdump_print_crash(struct pt_regs *regs);
extern void __inner_flush_dcache_all(void);
extern void __inner_flush_dcache_L1(void);

#endif
