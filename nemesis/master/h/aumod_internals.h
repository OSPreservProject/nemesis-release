/* Copyright 1998 Dickon Reed */
/* Revealed; the secret life of aumods */
/* read by AuMod implementation and mixer implementation */

#define MAX_CLIENTS 8
#include <Au.ih>
#include <AuPlaybackWriter.ih>

/* define DOLOGGING to turn on logging, but remember to 
   recompile aumixer AND aumod
   */


typedef struct _AuPlaybackWriter {
    const Au_Rec *au;
    AuPlaybackWriter_cl closure;

    int32_t *ptr;
    uint32_t ptrloc;
    uint32_t absize;
    uint32_t toend;
    int64_t latency;
#ifdef DOLOGGING
    uint64_t *log;
    uint64_t *logptr;
    int logcount;
#endif
    string_t streamname;
    int32_t *queue_length;
    int32_t *volume_left;
    int32_t *volume_right;
} AuPlaybackWriter_st;

typedef struct _Async_AuPlaybackWriter {
    AuPlaybackWriter_cl closure;
    AuPlaybackWriter_clp sync;
    int32_t len;
    int32_t *buffer;
    int32_t writer_index;
    int32_t reader_index;
    Event_Count ec_writer;	/* advanced whenver data is added to the buffer */
    Event_Count ec_reader;	/* advanced whenver data is deleted from the buffer */
    uint32_t volume;
    Time_ns start;
} AsyncAuplaybackWriter_st;

#define VOLMAX 32767
#define VOLMAX_SHIFT 15
#define VOLMAX_LSHIFT16 (16-VOLMAX_SHIFT)
