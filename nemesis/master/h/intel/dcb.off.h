/* DCB_xxx are offsets from the dcbro    */
#define DCB_L_PSMASK     0x0008
#define DCB_L_FEN        0x0018
#define DCB_L_NUMEPS     0x0028
#define DCB_A_DCBRW      0x003C
#define DCB_A_CTXTS      0x0040
#define DCB_L_NUMCTXTS   0x0044

/* DCBRW_xxx are offsets from the dcbrw  */
#define DCBRW_L_ACTCTX   0x0808
#define DCBRW_L_RESCTX   0x080c
#define DCBRW_L_MODE     0x0810
#define DCBRW_L_MMEP     0x0828
#define DCBRW_L_MMCODE   0x082C
#define DCBRW_L_MMVA     0x0830
#define DCBRW_L_MMSTR    0x0834

/* This is also an offset from the dcbrw */
#define PVS_DCBRW_OFFSET 0x083C
