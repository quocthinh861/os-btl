// #ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Virtual memory module mm/mm-vm.c
 */

#include "string.h"
#include "mm.h"
#include <stdlib.h>
#include <stdio.h>

/*enlist_vm_freerg_list - add new rg to freerg_list
 *@mm: memory region
 *@rg_elmt: new region
 *
 */
// int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct rg_elmt)
// {
//   struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list;

//   if (rg_elmt.rg_start >= rg_elmt.rg_end)
//     return -1;

//   if (rg_node != NULL)
//     rg_elmt.rg_next = rg_node;

//   /* Enlist the new region */
//   mm->mmap->vm_freerg_list = &rg_elmt;

//   return 0;
// }

/*get_vma_by_num - get vm area by numID
 *@mm: memory region
 *@vmaid: ID vm area to alloc memory region
 *
 */
struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid)
{
  struct vm_area_struct *pvma = mm->mmap;

  if (mm->mmap == NULL)
    return NULL;

  int vmait = 0;

  while (vmait < vmaid)
  {
    if (pvma == NULL)
      return NULL;

    pvma = pvma->vm_next;
  }

  return pvma;
}

/*get_symrg_byid - get mem region by region ID
 *@mm: memory region
 *@rgid: region ID act as symbol index of variable
 *
 */
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
{
  if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
    return NULL;

  return &mm->symrgtbl[rgid];
}

/*__alloc - allocate a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *@alloc_addr: address of allocated memory region
 *
 */
int __alloc(struct pcb_t *caller, int vmaid, int rgid, int size, int *alloc_addr)
{
  /*Allocate at the toproof */
  struct vm_rg_struct rgnode;

  if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
  {
    caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
    caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;

    *alloc_addr = rgnode.rg_start;
    return 0;
  }

  /* TODO get_free_vmrg_area FAILED handle the region management (Fig.6)*/
  else
  {
    printf("Process %d, failed to allocate memory region with size %d\n", caller->pid, size);

    /* Attempt to increate limit to get space */
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);
    int inc_sz = PAGING_PAGE_ALIGNSZ(size);
    int old_sbrk;

    old_sbrk = cur_vma->sbrk;

    /* TODO INCREASE THE LIMIT
     * inc_vma_limit(caller, vmaid, inc_sz)
     */
    inc_vma_limit(caller, vmaid, inc_sz);

    if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)
    {
      /* Successful increase limit */
      caller->mm->symrgtbl[rgid].rg_start = old_sbrk;
      caller->mm->symrgtbl[rgid].rg_end = old_sbrk + size;

      *alloc_addr = old_sbrk;
      return 0;
    }
  }
  return -1;
}

/*__free - remove a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __free(struct pcb_t *caller, int vmaid, int rgid)
{
  struct vm_rg_struct *rgnode = (struct vm_rg_struct *)malloc(sizeof(struct vm_rg_struct));

  if (rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
    return -1;

  /* TODO: Manage the collect freed region to freerg_list */
  rgnode->rg_start = caller->mm->symrgtbl[rgid].rg_start;
  rgnode->rg_end = caller->mm->symrgtbl[rgid].rg_end;

  /*enlist the obsoleted memory region */
  if (enlist_vm_rg_node(&caller->mm->mmap->vm_freerg_list, rgnode) == 0)
  {
  }

  return 0;
}

/*pgalloc - PAGING-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int pgalloc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
{
  int addr;
  printf("Process %d: Allocating %d bytes\n", proc->pid, size);

  printf("------------------------ show page table process %d before allocating ------------------------\n", proc->pid);
  print_pgtbl(proc, 0, -1);
  printf("----------------------------------------------------------------------------------------------\n");

  /* By default using vmaid = 0 */
  int result = __alloc(proc, 0, reg_index, size, &addr);

  printf("------------------------ show page table process %d after allocating ------------------------\n", proc->pid);
  print_pgtbl(proc, 0, -1);
  printf("---------------------------------------------------------------------------------------------\n");

  return result;
}

/*pgfree - PAGING-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */

int pgfree_data(struct pcb_t *proc, uint32_t reg_index)
{
  return __free(proc, 0, reg_index);
}

/*pg_getpage - get the page in ram
 *@mm: memory region
 *@pagenum: PGN
 *@framenum: return FPN
 *@caller: caller
 *
 */
int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller)
{
  uint32_t pte = mm->pgd[pgn];

  if (PAGING_PAGE_SWAPPED_PRESENT(pte))
  { /* Page is not online, make it actively living */
    printf("Process %d: Page %d is swapped out\n", caller->pid, pgn);
    int vicpgn, swpfpn;
    int vicfpn;
    uint32_t vicpte;

    int tgtfpn = GETVAL(pte, PAGING_PTE_SWPOFF_MASK, PAGING_SWPFPN_OFFSET);

    // Find free frame in RAM
    if (MEMPHY_get_freefp(caller->mram, &swpfpn) == 0)
    {
      printf(" Process %d: Swap out page %d to swap frame %d\n", caller->pid, pgn, swpfpn);

      __swap_cp_page(caller->active_mswp, tgtfpn, caller->mram, swpfpn);

      pte_set_fpn(&caller->mm->pgd[pgn], tgtfpn);

      struct framephy_struct *fp = caller->mram->used_fp_list;
      struct framephy_struct *newnode = malloc(sizeof(struct framephy_struct));

      /* Create new node with value fpn */
      newnode->fpn = swpfpn;
      newnode->fp_next = fp;
      caller->mram->used_fp_list = newnode;
    }
    else
    {
      printf("Cannot find free frame in RAM  -  pg_getpage\n");

      /* TODO: Play with your paging theory here */
      /* Find victim page */
      if (find_victim_page_MRU(caller->mm, &vicpgn) != 0)
      {
        printf("ERROR: Cannot find vitim page  -  pg_getpage()\n");
        return -1;
      }

      /* Get free frame in MEMSWP */
      if (MEMPHY_get_freefp(caller->active_mswp, &swpfpn) != 0)
      {
        printf("ERROR: Cannot find free frame in RAM  -  pg_getpage\n");
        return -1;
      }

      printf("Process %d: Swap out page %d to swap frame %d\n", caller->pid, vicpgn, swpfpn);

      vicpte = caller->mm->pgd[vicpgn]; // victim pte from victim page number
      vicfpn = GETVAL(vicpte, PAGING_PTE_FPN_MASK, 0);

      /* Do swap frame from MEMRAM to MEMSWP and vice versa */
      /* Copy victim frame to swap */
      __swap_cp_page(caller->mram, vicfpn, caller->active_mswp, swpfpn);
      /* Copy target frame from swap to mem */
      __swap_cp_page(caller->active_mswp, tgtfpn, caller->mram, vicfpn);

      /* Update page table */
      pte_set_swap(&caller->mm->pgd[vicpgn], 0, swpfpn);

      /* Update its online status of the target page */
      pte_set_fpn(&caller->mm->pgd[pgn], vicfpn);

      printf("Show mru_pgn before updating: ");
      struct pgn_t *temp = caller->mm->mru_pgn;
      while (temp != NULL)
      {
        printf("%d ", temp->pgn);
        temp = temp->pg_next;
      }
      printf("\n");
      // remove pgn from mru_pgn
      remove_pgn_node(&caller->mm->mru_pgn, pgn);
      enlist_pgn_node(&caller->mm->mru_pgn, pgn);

      printf("Show mru_pgn after updating: ");
      struct pgn_t *temp3 = caller->mm->mru_pgn;
      while (temp3 != NULL)
      {
        printf("%d ", temp3->pgn);
        temp3 = temp3->pg_next;
      }
      printf("\n");
    }
  }

  *fpn = GETVAL(mm->pgd[pgn], PAGING_PTE_FPN_MASK, 0);

  printf("Process %d: Page %d is online with frame %d\n", caller->pid, pgn, *fpn);

  printf("Because this Page is currently used, so we need to update mru_pgn\n");
  printf("Show mru_pgn before updating: ");
  struct pgn_t *temp = caller->mm->mru_pgn;
  while (temp != NULL)
  {
    printf("%d ", temp->pgn);
    temp = temp->pg_next;
  }
  printf("\n");

  // remove pgn from mru_pgn
  remove_pgn_node(&caller->mm->mru_pgn, pgn);
  enlist_pgn_node(&caller->mm->mru_pgn, pgn);

  printf("Show mru_pgn after updating: ");
  struct pgn_t *temp3 = caller->mm->mru_pgn;
  while (temp3 != NULL)
  {
    printf("%d ", temp3->pgn);
    temp3 = temp3->pg_next;
  }
  printf("\n");

  return 0;
}

void remove_pgn_node(struct pgn_t **head, int pgn)
{
  struct pgn_t *temp = *head;
  struct pgn_t *prev = NULL;

  while (temp != NULL)
  {
    if (temp->pgn == pgn)
    {
      if (prev == NULL)
      {
        *head = temp->pg_next;
      }
      else
      {
        prev->pg_next = temp->pg_next;
      }
      free(temp);
      break;
    }
    prev = temp;
    temp = temp->pg_next;
  }
}

/*pg_getval - read value at given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1; /* invalid page access */

  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;

  printf("phyaddr: %d\n", phyaddr);

  MEMPHY_read(caller->mram, phyaddr, data);

  return 0;
}

/*pg_setval - write value to given offset
 *@mm: memory region
 *@addr: virtual address to acess
 *@value: value
 *
 */
int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  if (pg_getpage(mm, pgn, &fpn, caller) != 0)
    return -1; /* invalid page access */

  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;

  printf("phyaddr: %d\n", phyaddr);

  MEMPHY_write(caller->mram, phyaddr, value);

  return 0;
}

/*__read - read value in region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __read(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE *data)
{
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);

  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
    return -1;
  printf("Reading to region %d [%d, %d]\n", rgid, currg->rg_start, currg->rg_end); // DEBUGGING
  pg_getval(caller->mm, currg->rg_start + offset, data, caller);

  return 0;
}

/*pgwrite - PAGING-based read a region memory */
int pgread(
    struct pcb_t *proc, // Process executing the instruction
    uint32_t source,    // Index of source register
    uint32_t offset,    // Source address = [source] + [offset]
    uint32_t destination)
{
  BYTE data;
  printf("Read proc %d\n", proc->pid); // DEBUGGING
  int val = __read(proc, 0, source, offset, &data);

  destination = (uint32_t)data;
#ifdef IODUMP
  printf("read region=%d offset=%d value=%d\n", source, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); // print max TBL
#endif
  MEMPHY_dump(proc->mram);
#endif

  return val;
}

/*__write - write a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size
 *
 */
int __write(struct pcb_t *caller, int vmaid, int rgid, int offset, BYTE value)
{
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);

  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  if (currg == NULL || cur_vma == NULL) /* Invalid memory identify */
    return -1;

  printf("Writing to region %d [%d, %d]\n", rgid, currg->rg_start, currg->rg_end); // DEBUGGING

  pg_setval(caller->mm, currg->rg_start + offset, value, caller);

  return 0;
}

/*pgwrite - PAGING-based write a region memory */
int pgwrite(
    struct pcb_t *proc,   // Process executing the instruction
    BYTE data,            // Data to be wrttien into memory
    uint32_t destination, // Index of destination register
    uint32_t offset)
{
  printf("Write proc %d\n", proc->pid); // DEBUGGING
#ifdef IODUMP
  printf("write region=%d offset=%d value=%d\n", destination, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); // print max TBL
#endif
  MEMPHY_dump(proc->mram);
#endif

  return __write(proc, 0, destination, offset, data);
}

/*free_pcb_memphy - collect all memphy of pcb
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 */
int free_pcb_memph(struct pcb_t *caller)
{
  int pagenum, fpn;
  uint32_t pte;

  for (pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
  {
    pte = caller->mm->pgd[pagenum];

    if (!PAGING_PAGE_PRESENT(pte))
    {
      fpn = PAGING_FPN(pte);
      MEMPHY_put_freefp(caller->mram, fpn);
    }
    else
    {
      fpn = PAGING_SWP(pte);
      MEMPHY_put_freefp(caller->active_mswp, fpn);
    }
  }

  return 0;
}

/*get_vm_area_node - get vm area for a number of pages
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
struct vm_rg_struct *get_vm_area_node_at_brk(struct pcb_t *caller, int vmaid, int size, int alignedsz)
{
  struct vm_rg_struct *newrg;
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  newrg = malloc(sizeof(struct vm_rg_struct));

  newrg->rg_start = cur_vma->sbrk;
  newrg->rg_end = newrg->rg_start + size;

  return newrg;
}

/*validate_overlap_vm_area
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
int validate_overlap_vm_area(struct pcb_t *caller, int vmaid, int vmastart, int vmaend)
{
  struct vm_area_struct *vma = get_vma_by_num(caller->mm, vmaid);

  /* TODO validate the planned memory area is not overlapped */

  // Check if the new memory range overlaps with the current vm_area_struct
  while (vma)
  {
    // if (OVERLAP(vma->vm_start, vmastart, vmaend, vma->vm_end) == 0)
    //   return -1;
    vma = vma->vm_next;
  }

  return 0;
}

/*inc_vma_limit - increase vm area limits to reserve space for new variable
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@inc_sz: increment size
 *
 */
int inc_vma_limit(struct pcb_t *caller, int vmaid, int inc_sz)
{
  struct vm_rg_struct *newrg = malloc(sizeof(struct vm_rg_struct));
  int inc_amt = PAGING_PAGE_ALIGNSZ(inc_sz);
  int incnumpage = inc_amt / PAGING_PAGESZ;
  struct vm_rg_struct *area = get_vm_area_node_at_brk(caller, vmaid, inc_sz, inc_amt);
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  int old_end = cur_vma->vm_end;

  /*Validate overlap of obtained region */
  if (validate_overlap_vm_area(caller, vmaid, area->rg_start, area->rg_end) < 0)
  {
    printf("inc_vma_limit(): validate_overlap_vm_area() failed\n");
    return -1; /*Overlap and failed allocation */
  }

  /* The obtained vm area (only)
   * now will be alloc real ram region */
  cur_vma->vm_end += inc_amt;
  cur_vma->sbrk = cur_vma->vm_end;


  if (vm_map_ram(caller, area->rg_start, area->rg_end,
                 old_end, incnumpage, newrg) < 0)
    return -1; /* Map the memory to MEMRAM */

  if (enlist_vm_rg_node(&caller->mm->mmap->vm_freerg_list, newrg) == 0)
  {
  }

  return 0;
}

int find_victim_page_MRU(struct mm_struct *mm, int *retpgn)
{
  struct pgn_t *pg = mm->mru_pgn;

  /* TODO: Implement the theoretical mechanism to find the victim page */
  if (pg == NULL)
  {
    return -1;
  }

  *retpgn = pg->pgn;
  mm->mru_pgn = pg->pg_next;

  free(pg); // Free the memory of the removed pgn

  return 0;
}

/*find_victim_page - find victim page
 *@caller: caller
 *@pgn: return page number
 *
 */
int find_victim_page(struct mm_struct *mm, int *retpgn)
{
  struct pgn_t *pg = mm->mru_pgn;

  /* TODO: Implement the theorical mechanism to find the victim page */
  if (pg == NULL)
  {
    return -1;
  }

  if (pg->pg_next == NULL)
  {
    *retpgn = pg->pgn;
    mm->mru_pgn = NULL;
    return 0;
  }

  while (pg->pg_next->pg_next != NULL)
  {
    pg = pg->pg_next;
  }

  *retpgn = pg->pg_next->pgn;

  pg->pg_next = NULL;

  free(pg->pg_next);

  free(pg);

  return 0;
}

/*get_free_vmrg_area - get a free vm region
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@size: allocated size
 *
 */
int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg)
{
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;

  if (rgit == NULL)
    return -1;

  // show free list of vm area with new region
  printf("Free list of vm area for process %d: ", caller->pid);
  struct vm_rg_struct *temp = rgit;
  while (temp != NULL)
  {
    printf("[%d, %d] ", temp->rg_start, temp->rg_end);
    temp = temp->rg_next;
  }
  printf("\n");

  /* Probe unintialized newrg */
  newrg->rg_start = newrg->rg_end = -1;

  /* Traverse on list of free vm region to find a fit space */
  while (rgit != NULL)
  {
    if (rgit->rg_start + size <= rgit->rg_end)
    { /* Current region has enough space */
      newrg->rg_start = rgit->rg_start;
      newrg->rg_end = rgit->rg_start + size;

      /* Update left space in chosen region */
      if (rgit->rg_start + size < rgit->rg_end)
      {
        rgit->rg_start = rgit->rg_start + size;
      }
      else
      { /*Use up all space, remove current node */
        /*Clone next rg node */
        struct vm_rg_struct *nextrg = rgit->rg_next;

        /*Cloning */
        if (nextrg != NULL)
        {
          rgit->rg_start = nextrg->rg_start;
          rgit->rg_end = nextrg->rg_end;

          rgit->rg_next = nextrg->rg_next;

          free(nextrg);
        }
        else
        {                                /*End of free list */
          rgit->rg_start = rgit->rg_end; // dummy, size 0 region
          rgit->rg_next = NULL;
        }
      }
      break;
    }
    else
    {
      rgit = rgit->rg_next; // Traverse next rg
    }
  }

  if (newrg->rg_start == -1) // new region not found
  {
    printf("new region not found\n");
    return -1;
  }
  else
  {
    printf("new region found: [%d, %d]\n", newrg->rg_start, newrg->rg_end);
  }

  // show free list after alloc
  printf("Free list of vm area for process %d after alloc: ", caller->pid);
  rgit = cur_vma->vm_freerg_list;
  while (rgit != NULL)
  {
    printf("[%d, %d] ", rgit->rg_start, rgit->rg_end);
    rgit = rgit->rg_next;
  }
  printf("\n");

  return 0;
}

// #endif
