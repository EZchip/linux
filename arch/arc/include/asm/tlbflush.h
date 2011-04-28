/* Sameer: Keeping it empty for a while.
           Will populate it as and when needed. */

extern void local_flush_tlb_page(struct vm_area_struct *vma,
    unsigned long page);


extern void local_flush_tlb_range(struct vm_area_struct *vma,
    unsigned long start, unsigned long end);

extern void local_flush_tlb_kernel_range(unsigned long start,
    unsigned long end);

extern void local_flush_tlb_all(void);
extern void local_flush_tlb_mm(struct mm_struct *);


#define flush_tlb_range(vma,vmaddr,end)     local_flush_tlb_range(vma, vmaddr, end)
#define flush_tlb_page(vma,page)            local_flush_tlb_page(vma, page)
#define flush_tlb_kernel_range(vmaddr,end)  local_flush_tlb_kernel_range(vmaddr, end)
#define flush_tlb_all()                     local_flush_tlb_all()
#define flush_tlb_mm(mm)                    local_flush_tlb_mm(mm)
