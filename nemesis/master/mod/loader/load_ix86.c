/* Loader for Intel ELF files */

/* Dickon Reed */

#include <nemesis.h>
#include <exceptions.h>
#include <contexts.h>
#include <stdio.h>
#include <salloc.h>
#include <Load.ih>
#include <Heap.ih>
#include <elf.h>
#include <Context.ih>
#include <autoconf/module_offset_data.h>
#ifdef CONFIG_MODULE_OFFSET_DATA
#include <IDCMacros.h>
#include <ModData.ih>
#endif /* CONFIG_MODULE_OFFSET_DATA */

/* general tracing, volumous */
#ifdef DEBUG
#define TRC(_x) _x
#else
#define TRC(_x)
#endif

/* non-aligned access tracing, very large */
#define ATRC(_x)

INLINE uint32_t read32off(uint8_t *x)
{
    uint32_t v, i;
    ATRC(fprintf(stderr,"Doing 32 bit read\n"));
    for (i=0; i<4; i++) { 
	ATRC(fprintf(stderr,"byte %d is %x\n", i, *(x+i)));
    }
    v= (*x) + ((*(x+1))<<8) + ((*(x+2))<<16) + ((*(x+3))<<24);
    ATRC(fprintf(stderr,"Done 32 bit read: %x\n", v));
    return v;
}

INLINE void write32off(uint8_t *loc, uint32_t x)
{
    int i;
    ATRC(fprintf(stderr,"Doing 32 bit write\n"));
    for (i=0; i<4; i++) { 
	ATRC(fprintf(stderr,"byte %d is %x\n", i, *(loc+i)));
    }
    *loc = x & 0xff;
    *(loc+1) = (x>>8) & 0xff;
    *(loc+2) = (x>>16) & 0xff;
    *(loc+3) = (x>>24) & 0xff;
    ATRC(fprintf(stderr,"Done 32 bit write: now %x (viz %x)\n",(*loc) + ((*(loc+1))<<8) + ((*(loc+2))<<16) + ((*(loc+3))<<24),x ));
    for (i=0; i<4; i++) { 
	ATRC(fprintf(stderr,"byte %d is %x viz %x\n", i, *(loc+i)));
    }

}

/*
 * Module stuff
 */
static Load_FromRd_fn Load_FromRd_m;
static Load_op ld_ms = { Load_FromRd_m };
static const Load_cl ld_cl = { &ld_ms, BADPTR };

CL_EXPORT (Load, Load, ld_cl);


void filereadblock(FILE *stream, int offset, int size, void *addr)
{
    TRC(fprintf(stderr,"Doing fseek(%x)\n", offset));
    fseek(stream, offset, SEEK_SET);
    if (size==0) return;
    TRC(fprintf(stderr,"Doing fread(%x)\n", size));
    if (!fread(addr, size, 1, stream)) {
	fprintf(stderr, "Couldn't read %x bytes!\n", size);
	RAISE_Load$Failure(Load_Bad_Format);
    }
}

#ifdef DEBUG
void dumpstretch(Stretch_clp str, char *name)
{
    char *ptr;
    int size;
    int i;
    ptr = STR_RANGE(str, &size);
    
    for (i=0; i<size; i++) {
	
	printf("%s %x: %x\n", name, i, *ptr);
	ptr++;
    }
}
#endif /* DEBUG */

/*
 * POSIX emulation stuff *
 */

/* XXX Why isn't this in libc? */
/* XXX Because libc's confused by having separate readers and writers; we
   only want ONE file position per FILE *, not two. This _can_ be sorted
   out, but will need a little thought and quite a bit of modification of
   mod/venimpl/c/stdio. */
/* For now, we assume that we only have a reader to worry about. (The
   joke is, we construct our FILE * using a reader that's passed in
   directly, and which has more than likely been ripped out of a FILE*
   from libc by the code in cexec.c... haha.) */
int fseek (FILE *stream, long int offset, int whence)
{
    NOCLOBBER int r;

    r= 0;
    if (whence != SEEK_SET) {
	fprintf(stderr,"Wah! fseek doesn't support access mode %d\n", whence);
	return -1;
    } else {
	TRY {
	    Rd$Seek(stream->rd, offset);
	} CATCH_ALL {
	    r=-1;
	} 
	ENDTRY;
    }
    return r;
}

PUBLIC Stretch_clp
Load$FromStream (FILE *stream, Context_clp imps, Context_clp exps,
		 ProtectionDomain_ID new_pdom, Stretch_clp *data)
{
    StretchAllocator_clp sa       = NAME_FIND ("sys>StretchAllocator",
					       StretchAllocator_clp);
    Elf32_Ehdr header;
    Elf32_Shdr *sections, *sectionptr;
    int sectionnum;
    char *strtable, *symstrtab;

#define SECTION_TEXT 0
#define SECTION_RODATA 1
#define SECTION_DATA 2
#define SECTION_BSS 3
#define SECTION_SYMTAB 4
#define SECTION_SYMSTRTAB 5
#define MAXSECTIONS 6
#define SECTIONLIMIT 64 /* the number of sections that can occur in the file */
    int offsets[MAXSECTIONS], sizes[MAXSECTIONS];
    int *addr[MAXSECTIONS], secnum[MAXSECTIONS];
    int kind;
    const char *segmentnames[] = {".text", ".rodata", ".data", ".bss",
				  ".symtab", ".strtab", "???"};
    Stretch_clp NOCLOBBER text_seg, data_seg;
    char *text_seg_p, *data_seg_p;
    int size;
    Elf32_Sym *symtab;
    int NOCLOBBER n;
    int symtabsize;
    char *secaddr[SECTIONLIMIT];
    int secmap[SECTIONLIMIT];
    void (*entrypoint)(Closure_clp );
#ifdef DEBUG
    const char *symtypenames[] = {"None", "Object", "Function", "Section",
				  "File", "q5", "q6","q7","q8","q9","q10",
				  "q11","q12","LoProc","MedProc","HiProc"};
    const char *symbindnames[] = {"Local", "Global", "Weak", "q3", "q4",
				  "q5", "q6","q7","q8","q9","q10","q11",
				  "q12","LoProc","MedProc","HiProc"};
#endif /* DEBUG */

    for (kind=0; kind<MAXSECTIONS; kind++) offsets[kind]=sizes[kind]=0;
    for (n=0; n<MAXSECTIONS; n++) secaddr[n] = BADPTR;
    for (n=0; n<MAXSECTIONS; n++) secmap[n] = MAXSECTIONS;
    TRC(fprintf(stderr,"ELF loader starting\n"));
    
    /* get and dump the header */
    filereadblock(stream, 0, sizeof(Elf32_Ehdr), &header);

    TRC(fprintf(stderr,"Got header:\n"));
    
    if (header.e_ident[0] != 0x7f || header.e_ident[1] != 'E' ||
	header.e_ident[2] != 'L' || header.e_ident[3] != 'F')
	RAISE_Load$Failure(Load_Bad_Format);

#ifdef DEBUG
    fprintf(stderr,"\tNumber of sections: %d\n",header.e_shnum);
    fprintf(stderr,"\tentry point is %d\n", header.e_entry);
    fprintf(stderr,"\tprogram header table offset is %x\n", header.e_phoff);
    fprintf(stderr,"\tsection table offset is %d\n", header.e_shoff);
    fprintf(stderr,"\tflags offset is %d\n", header.e_flags);
    fprintf(stderr,"\theader size in bytes %d\n", header.e_ehsize);
    fprintf(stderr,"\teach program header entry is size %d\n",
	    header.e_phentsize);
    fprintf(stderr,"\tprogram heaader table has %d entrys\n", header.e_phnum);
    fprintf(stderr,"\teach section header entry is size %d\n",
	    header.e_shentsize);
    fprintf(stderr,"\tsection header table has %d entrys\n", header.e_shnum);
    fprintf(stderr,"\tstring table is section %d\n", header.e_shstrndx);
#endif /* DEBUG */
    
    TRC(fprintf(stderr,"Reading sections table\n"));
    /* read the sections table */
    sections = Heap$Malloc(Pvs(heap), header.e_shentsize *header.e_shnum);
    filereadblock(stream, header.e_shoff, header.e_shentsize * header.e_shnum,
		  sections);
    
    /* read the string table if it exists */
    if (header.e_shstrndx) {
	TRC(fprintf(stderr,"Reading string table\n"));
	sectionptr = sections + header.e_shstrndx;
	strtable = Heap$Malloc(Pvs(heap), sectionptr->sh_size);
	filereadblock(stream, sectionptr->sh_offset, sectionptr->sh_size,
		      strtable);
    } else {
	strtable = 0;
    }
    
    /* Read the header information for each section */
    sectionptr = sections;
    for (sectionnum=0; sectionnum<header.e_shnum;
	 sectionnum++, sectionptr++) {
	char *name;

	name = strtable ? strtable + sectionptr->sh_name : "NONAMES";
	
	TRC(fprintf(stderr,"Section %d: \"%s\" type %d\n", sectionnum,
		    name, sectionptr -> sh_type));
	TRC(fprintf(stderr,"\tfrom %x for %x\n", sectionptr -> sh_offset,
		    sectionptr -> sh_size));
	
	for (kind=0; kind<MAXSECTIONS; kind++) {
	    if (!strcmp(name, segmentnames[kind])) {
		TRC(fprintf(stderr,"\tis kind %d\n", kind));
		offsets[kind] = sectionptr -> sh_offset;
		sizes[kind] = sectionptr -> sh_size;
		if(kind <= SECTION_BSS) {
		    /* XXX Pad text, rodata, data  and bss segments */
		    sizes[kind] = (sizes[kind] + 7) & (~7);
		}
		secnum[kind] = sectionnum;
		secmap[sectionnum] = kind;
	    }
	}
	
    }

    /* Allocate stretches for the text, and if necessary the data sections */
    text_seg = STR_NEW_SALLOC(sa, sizes[SECTION_TEXT] + sizes[SECTION_RODATA]);
    text_seg_p = STR_RANGE(text_seg, &size);
    /* This memset is in here because it seems to catch a fair few memory
       system problems XXX SDE */
    memset(text_seg_p, 0, sizes[SECTION_TEXT] + sizes[SECTION_RODATA]);

    if ((sizes[SECTION_DATA] + sizes[SECTION_BSS])>0) {
	data_seg = STR_NEW_SALLOC(sa, sizes[SECTION_DATA]+sizes[SECTION_BSS]);
	data_seg_p = STR_RANGE(data_seg, &size);
    } else {
	data_seg = NULL;
	data_seg_p = BADPTR;
    }
    symstrtab = Heap$Malloc(Pvs(heap), sizes[SECTION_SYMSTRTAB]);
    symtab = Heap$Malloc(Pvs(heap), sizes[SECTION_SYMTAB]);
   
    /* Store the addresses for each of the sections */
    secaddr[secnum[SECTION_TEXT]] = (void *)addr[SECTION_TEXT] =
	(void *)text_seg_p;
    secaddr[secnum[SECTION_RODATA]] = (void *)addr[SECTION_RODATA] = (void *)
	(text_seg_p + sizes[SECTION_TEXT]);
    secaddr[secnum[SECTION_DATA]] = (void *)addr[SECTION_DATA] =
	(void *)data_seg_p;
    secaddr[secnum[SECTION_BSS]] = (void *)addr[SECTION_BSS] = (void *)
	(data_seg_p + sizes[SECTION_DATA]);

    /* Read the sections we're interested in into memory */
    filereadblock(stream, offsets[SECTION_TEXT], sizes[SECTION_TEXT],
		  addr[SECTION_TEXT]);
    filereadblock(stream, offsets[SECTION_RODATA], sizes[SECTION_RODATA],
		  addr[SECTION_RODATA]);
    if (data_seg) {
	filereadblock(stream, offsets[SECTION_DATA], sizes[SECTION_DATA],
		      addr[SECTION_DATA]);
    }
    filereadblock(stream, offsets[SECTION_SYMSTRTAB],
		  sizes[SECTION_SYMSTRTAB], symstrtab);
    filereadblock(stream, offsets[SECTION_SYMTAB], sizes[SECTION_SYMTAB],
		  symtab);

#ifdef DEBUG
    for (n=0; n<sizes[SECTION_SYMSTRTAB]; n++)
	fprintf(stderr,"%d: %c %d\n", n, symstrtab[n], symstrtab[n]);

    for (kind = 0; kind<MAXSECTIONS; kind++) {
	if (offsets[kind]) {
	    fprintf(stderr,"Section %s\n", segmentnames[kind]);
	    fprintf(stderr,"\tfile offset = %x, size = %x, "
			"resident at %x\n", offsets[kind],
			sizes[kind], addr[kind]);
	}
    }
#endif /* DEBUG */
    
    /* Zero the BSS if we have one */
    if (data_seg && sizes[SECTION_BSS]>0)
	memset(addr[SECTION_BSS], 0, sizes[SECTION_BSS]); /* always nice */

    /* go through the symbol table, relocating stuff */
    symtabsize = sizes[SECTION_SYMTAB]/sizeof(Elf32_Sym);
    for (n=0; n<symtabsize; n++) {
	int32_t offset;
	int segment;
	int section;

	section = symtab[n].st_shndx;

	/* some symbols have section numbers containing special
	   information; ignore them */
	if (section >= header.e_shnum) continue;

	/* XXX is this cast valid? */
	offset = (int32_t)secaddr[symtab[n].st_shndx];
	    
	segment = secmap[symtab[n].st_shndx];
	    
	if (segment < 0 || segment > MAXSECTIONS) segment = MAXSECTIONS;
	    
#ifdef DEBUG
	fprintf(stderr,"\"%s\" number %d section %s (%d) bind %s "
		"type %s size %x value %x ",
		(symtab[n].st_name && symtab[n].st_name <
		 sizes[SECTION_SYMSTRTAB]) ?
		(symstrtab + symtab[n].st_name) : "NULL",
		n, segmentnames[segment], symtab[n].st_shndx,
		symbindnames[ELF32_ST_BIND(symtab[n].st_info)],
		symtypenames[ELF32_ST_TYPE(symtab[n].st_info)],
		symtab[n].st_size, symtab[n].st_value);
#endif /* DEBUG */
	symtab[n].st_value += offset;
	TRC(fprintf(stderr," => %x\n", symtab[n].st_value));

	/* Global symbols get plonked in the namespace */
	if (ELF32_ST_BIND(symtab[n].st_info) == STB_GLOBAL &&
	    symtab[n].st_name && 
	    symtab[n].st_name < sizes[SECTION_SYMSTRTAB])
	{
	    Type_Any any;
	    Load_Symbol lsym;
		
	    lsym = Heap$Malloc(Pvs(heap), sizeof(*lsym));
		
	    lsym -> name = strdup(symstrtab + symtab[n].st_name);
	    lsym -> value = symtab[n].st_value; /* XXX */
	    lsym -> type = ELF32_ST_TYPE(symtab[n].st_info); /* XXX */
	    lsym -> class = symtab[n].st_shndx;
	    TRC(fprintf(stderr,"Adding symbol %s, value %x, type %d, "
			"class %d\n", lsym->name, lsym->value,
			lsym->type, lsym->class));
	    ANY_INIT(&any, Load_Symbol, lsym);
	    TRY {
		Context$Add(exps, lsym->name, &any);
	    } CATCH_Context$Exists() { 
	    } ENDTRY;

#ifdef CONFIG_MODULE_OFFSET_DATA
	    if(!strcmp(lsym->name, "NemesisModuleName")) {
		ModData_clp md = IDC_OPEN("sys>ModDataOffer",ModData_clp);
		    
		TRC(fprintf(stderr, "Module name = %s \n", 
			    (string_t) lsym->value));
		ModData$Register(md,(string_t)lsym->value,addr[SECTION_TEXT]);
	    }
#endif /* CONFIG_MODULE_OFFSET_DATA */
	}
    }
    TRC(fprintf(stderr,"**********end of symbol table\n"));

    /* go through each section, processing the relocation sections */
    sectionptr = sections;
    for (sectionnum=0; sectionnum<header.e_shnum;
	 sectionnum++, sectionptr++) {

#ifdef DEBUG
	if (sectionptr->sh_type == SHT_REL) {
	    fprintf(stderr,"Relocation section; symbol table is %d, "
		    "refers to section %d\n", sectionptr->sh_link,
		    sectionptr->sh_info);
	}
#endif /* DEBUG */
	if (sectionptr->sh_type == SHT_REL &&
	    sectionptr->sh_link == secnum[SECTION_SYMTAB] && 
	    (sectionptr->sh_info == secnum[SECTION_TEXT] ||
	     sectionptr->sh_info == secnum[SECTION_RODATA] ||
	     sectionptr->sh_info == secnum[SECTION_DATA]))
	{
	    Elf32_Rel *rels, *relptr;

	    rels = Heap$Malloc(Pvs(heap), sectionptr->sh_size);
	    
	    filereadblock(stream, sectionptr->sh_offset,
			  sectionptr->sh_size, rels);
	    relptr = rels ;

	    while ((((char *) relptr) - ((char *) rels)) <
		   sectionptr->sh_size)
	    {
		Elf32_Sym *symbol;
		int32_t A, S, P;
		int32_t *target;

		symbol = &(symtab[ELF32_R_SYM(relptr->r_info)]);
#ifdef DEBUG
		fprintf(stderr,"RS %d@%x Rel offset %x type %d "
			"symbol %d -> \"%s\" section %d value %x\n",
			sectionnum, relptr - rels, relptr->r_offset,
			ELF32_R_TYPE(relptr->r_info),
			ELF32_R_SYM(relptr->r_info),
			symstrtab + symbol->st_name, symbol->st_shndx,
			symbol->st_value );
#endif /* DEBUG */
		target = (void *)(secaddr[sectionptr->sh_info] +
				  relptr->r_offset);
		A = read32off((void *)target);
		S = symbol -> st_value;
		P = (int32_t)target;
		TRC(fprintf(stderr,"&A=%x A=%x S=%x P=%x\n",
			    secaddr[sectionptr->sh_info] + relptr->r_offset,
			    A, S, P));
		switch (ELF32_R_TYPE(relptr->r_info)) {
		case R_386_32:
		    write32off((void *)target,S + A);
		    break;
		case R_386_PC32:
		    write32off((void *)target, S+A-P);
		    break;
		case R_386_GLOB_DAT:
		    write32off((void *)target, S);
		    break;
		case R_386_JMP_SLOT:
		    write32off((void *)target, S);
		    break;
		case R_386_NONE:
		case R_386_COPY:
		    break;
		default:
		    fprintf(stderr,"Unsupported relocation type %d found; "
			    "skipping\n", ELF32_R_TYPE(relptr->r_info));
		    break;
		}
		TRC(fprintf(stderr," relocated symbol is %x at %p\n",
			    read32off((void *)target), target));
		if (!strcmp(symstrtab + symbol->st_name, "Main")) {
		    entrypoint = (void *)read32off((void *)target);
		}
		relptr++;
	    }
	}
	if (sectionptr->sh_type == SHT_RELA) {
	    fprintf(stderr,"RelA section found. Not supported\n");
	}
    }


    /* 
    ** Ok; now that we're finished, need to setup the correct protections 
    ** on the text and data stretches - we really should be allocating them 
    ** from the child in the case a new domain is created, but for now
    ** let's just ensure the protections are done in the correct order. 
    */

    /* XXX SMH: require global execute here in case of stubs, etc. 
       Hopefully the lack of read permission will daunt most abusers. */
    STR_SETGLOBAL(text_seg, 
		  SET_ELEM(Stretch_Right_Execute)
		  | SET_ELEM(Stretch_Right_Read));
    if (data_seg) {
	STR_SETGLOBAL(data_seg, 
		      SET_ELEM(Stretch_Right_Read));
    }
    
    TRC(printf("Loader: mapping text seg -R-E in new pdom (=%p)\n", new_pdom));
    STR_SETPROT(text_seg, new_pdom,  
		SET_ELEM(Stretch_Right_Read) | 
		SET_ELEM(Stretch_Right_Execute) |
		SET_ELEM(Stretch_Right_Meta));

    if (data_seg) {
	TRC(printf("Loader: mapping data seg -RW- in pdom %p\n", new_pdom));
	STR_SETPROT(data_seg, new_pdom,
		    SET_ELEM(Stretch_Right_Read) | 
		    SET_ELEM(Stretch_Right_Write) |
		    SET_ELEM(Stretch_Right_Meta));
    }

#if 0    /* XXX leave rights here for now - exec may need them */
    if(my_pdom != new_pdom) {
	PTRC(printf(
	    "Loader: removing all prots from text stretch in my pdom =%p.\n", 
	    my_pdom));
	STR_REMPROT(textseg, my_pdom); 
	if (data_seg) {
	    PTRC(printf(
		"Loader: removing all prots from data stretch "
		"in my pdom = %p.\n", 
		my_pdom));
	    STR_REMPROT(dataseg, my_pdom); 
	}
    }
#endif

    /* XXX SMH: should text be globally RE? partic if is a module? */

    {
	string_t tname, dname;

	tname = NAME_ALLOCATE("self>text_%d");
	dname = NAME_ALLOCATE("self>data_%d");
	
	CX_ADD(tname, text_seg, Stretch_clp);
	CX_ADD(dname, data_seg, Stretch_clp);
	free(dname); free(tname);
    }
    *data = data_seg;
    return text_seg;
}


static Stretch_clp
Load_FromRd_m (Load_clp		self,
	       Rd_clp		rd,
	       Context_clp	imps,
	       Context_clp	exps,
	       ProtectionDomain_ID pdid, 
/* RETURNS */  Stretch_clp     *data)
{
  FILE f;
  f.rd = rd;
  return Load$FromStream (&f, imps, exps, pdid, data);
}

