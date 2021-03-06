/*
    File: page_table.C

    Author: Raghavan	

    Description: Basic Paging

*/


#include "page_table.H"
#include "paging_low.H"
#include "console.H"

/* STATIC MEMBERS ARE COMMON TO ENTIRE PAGING SUBSYSTEM */
  PageTable * PageTable::current_page_table; /* pointer to currently loaded page table object */
  unsigned int PageTable::paging_enabled = 0;     /* is paging turned on (i.e. are addresses logical)? */
  FramePool     * PageTable::kernel_mem_pool;    /* Frame pool for the kernel memory */
  FramePool     * PageTable::process_mem_pool;   /* Frame pool for the process memory */
  unsigned long   PageTable::shared_size = 0;        /* size of shared address space */
  VMPool        * PageTable::vm_poolref;
  
  const unsigned int PageTable::PAGE_SIZE ; /* in bytes */
  const unsigned int PageTable::ENTRIES_PER_PAGE ; /* in entries, duh! */

/*--------------------------------------------------------------------------*/
/* EXPORTED FUNCTIONS */
/*--------------------------------------------------------------------------*/

  /* Set the global parameters for the page table */
void PageTable::init_paging(FramePool * _kernel_mem_pool, FramePool * _process_mem_pool, const unsigned long _shared_size){
  kernel_mem_pool = _kernel_mem_pool;
  process_mem_pool = _process_mem_pool;
  shared_size = _shared_size;
  vm_poolref = NULL;
}
  
  /* Initializes a page table with a given location for the directory and the
     page table proper.
     NOTE: The PageTable object still needs to be stored somewhere! Probably it is best
           to have it on the stack, as there is no memory manager yet...
     NOTE2: It may also be simpler to create the first page table *before* paging
           has been enabled.
  */

PageTable::PageTable(){
  page_directory = (unsigned long *) ((kernel_mem_pool->get_frame())*PAGE_SIZE); ; 
  Console::putui((unsigned int)page_directory);
  unsigned long *page_table = (unsigned long *) ((process_mem_pool->get_frame())*PAGE_SIZE); 
  Console::putui((unsigned int)page_table);
  unsigned long address = 0;
  unsigned int i;
  // map the first 4MB of memory
  for(i=0; i<ENTRIES_PER_PAGE; i++)
  {
	page_table[i] = address | 3; // attribute set to: supervisor level, read/write, present(011)
	address = address + PAGE_SIZE; // 4096 = 4k;
  }
  // fill the first entry of the page directory
  page_directory[0] = (unsigned long)page_table;  
  page_directory[0] = page_directory[0] | 3;
  for(i=1; i<ENTRIES_PER_PAGE; i++)
  {
	page_directory[i] = 0 | 2; 
  }
  page_directory[ENTRIES_PER_PAGE-1] = ((unsigned long) page_directory)|3;
  paging_enabled = 0;
}

/* Makes the given page table the current table. This must be done once during
   system startup and whenever the address space is switched (e.g. during
   process switching). */
	
void PageTable::load(){
  // write_cr3, read_cr3, write_cr0, and read_cr0 all come from the assembly functions
  current_page_table = this; 
}

/* Enable paging on the CPU. Typically, a CPU start with paging disabled, and
   memory is accessed by addressing physical memory directly. After paging is
   enabled, memory is addressed logically. */

void PageTable::enable_paging(){
  paging_enabled=1;
  write_cr3((unsigned long)current_page_table->page_directory);
  write_cr0(read_cr0() | 0x80000000); // set the paging bit in CR0 to 1
}

/* The page fault handler. */
void PageTable::handle_fault(REGS * _r){
  unsigned long f_addr = read_cr2();
  unsigned long addr;
  unsigned long pg_dir_index = (f_addr & 0xFFC00000) >> 22;
  unsigned long pg_tb_index = (f_addr & 0x003FF000 ) >> 12;
  unsigned long pg_offset = (f_addr & 0x00000FFC);
  unsigned long *page_table;
  if(vm_poolref->is_legitimate(f_addr) == true){
	page_table = (unsigned long *) (current_page_table->page_directory[pg_dir_index]);
  	if(((current_page_table->page_directory[pg_dir_index]) & 1) == 0){
		page_table = (unsigned long *) ((process_mem_pool->get_frame())*PAGE_SIZE);
		current_page_table->page_directory[pg_dir_index] = (unsigned long) (page_table);
		current_page_table->page_directory[pg_dir_index] |= 3;
		page_table = (unsigned long *) ((pg_dir_index<<12)|0xFFC00000);
		for(unsigned int i=0;i<ENTRIES_PER_PAGE;i++){
			page_table[i] = 0|2;
		}
	}
	page_table = (unsigned long *) ((pg_dir_index<<12)|0xFFC00000);
	if((page_table[pg_tb_index] & 1) == 0){
 		addr = ((process_mem_pool->get_frame())*PAGE_SIZE);
		page_table [pg_tb_index] = addr | 3;
  	}
  }
}

/* Release the frame associated with the page _page_no */
void PageTable::free_page(unsigned long _page_no){
  process_mem_pool->release_frame(_page_no);
}  
  
/* The page table needs to know about where it gets its pages from.
     For this, we have VMPools register with the page table. */
void PageTable::register_vmpool(VMPool *_pool){
  vm_poolref = _pool;
}







