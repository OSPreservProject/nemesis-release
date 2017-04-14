/*******************************************************************************
 Stub for Alpha Linux
*******************************************************************************/
 
/* 
 *   Create a dependency that should be immune from the effect of register
 *   renaming as is commonly seen in superscalar processors.  This should
 *   insert a minimum of 100-ns delays between reads/writes at clock rates
 *   up to 100 MHz---GGL
 *   
 *   Slowbcopy(char *src, char *dst, int count)   
 *   
 */ 

void
SlowBcopy(dst, src, len)
unsigned char *dst, *src;
int len;
{
}
