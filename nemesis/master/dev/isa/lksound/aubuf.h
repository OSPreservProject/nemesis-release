
/* the client visible portion of an aubuf */

/* an aubuf is an area of shared memory writable by both the clients and the
 * server. It is used for audio mixing.
 *
 * An aubuf is described by an aubufrec. An aubufrec contains two parts; 
 * an array of client
 */


#define MAXCLIENTS 64


typedef struct aubufclient_st {
    uint64_t active;   /* an identifier unique between clients 
			  or null, to indicate that no one is using the
			  data
			  */
    uint32_t volume;   /* a scale value to apply to all the samples.
			  It is weighted so that a value of 2^16 should
			  cause the client to play at it's preferred volume.
			  ie the client does:
			  *(ptrintobuf) += ((sample) * (volume))>>16
			  
			  written to by the server when it wants to change
			  the volume a client is playing at
			  */
    char *name;        /* the client may make this NULL or point to a
			  string readable by the server that describes
			  the client
			  */
    addr_t   control; /* reservered till I figure out how to use it;
			 the idea is that the server can make calls using
			 this to control a client (eg stop it)
			 */
} aubufclient;

typedef struct aubufrec_st {
    uint32_t * buffer;  /* base of the shared buffer */
    uint32_t length;   /* length of the shared buffer, in stereo 16 bit 
			  samples.

			  The client may assume this number is a power of
			  2
			  */

    uint32_t reader_location;  /* the last known location of the reader.
				*/

    uint64_t reader_timestamp; /* the timestamp when the reader was at
				  the above location
				  */

    uint64_t reader_rate;      /* the rate of the reader, in 
				  time per stereo 16 bit sample */

    uint32_t reader_granularity; /* the maximum 
				    number of stereo 16 bit bit samples 
				    the reader may be working ahead by
				    */

    uint32_t space;  /* the amount of space available */

    uint64_t modifier;           /* null if no one is writing to the 
				    client array, otherwise a bit pattern
				    unique to the client modifying the
				    client buffer below
				    */

    uint64_t modification_count; /* to be incremented whenever the
				    client array u below is touched */
    
    aubufclient u[64]; /* the client array */
} aubufrec;

/* to use an aubuf, a client should:

   - obtain the address of the aubufrec
   - wait till modifier is null, then write it's pattern in to 
   modifier
   - find an empty client register in u, by finding a null active field
   - write it's details in that aubufcleintrecord. (Writing active last)
   - bumb up modification count
   - clear modifier

   When the client wants to play some sounds, it should:

   - calculate the current reader pointer, by calculating:

   delta = (NOW() - reader_timestamp) / reader_rate

   delta is the number of 16 bit stereo samples (ie locations) the reader 
   is away from reader_location

   safedelta = delta + reader_granularity

   safedelta is therefore the maximum lead off reader_location the reader
   may be accessing

   Therefore the client may work from:

   safestart = reader_location + safedelta


   The client may write from that point as far as zerored_location. 

   */
