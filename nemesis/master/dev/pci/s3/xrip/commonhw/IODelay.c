/*******************************************************************************
  Stub for Alpha Linux
*******************************************************************************/
 
/* 
 *   All we really need is a delay of about 40ns for I/O recovery for just
 *   about any occasion, but we'll be more conservative here:  On a
 *   100-MHz CPU, produce at least a delay of 1,000ns.
 */ 
void
GlennsIODelay(count)
int count;
{
}
