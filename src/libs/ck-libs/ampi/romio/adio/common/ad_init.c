/* -*- Mode: C; c-basic-offset:4 ; -*- */
/* 
 *
 *   Copyright (C) 1997 University of Chicago. 
 *   See COPYRIGHT notice in top-level directory.
 */

#include "adio.h"
#include "converse.h" //For Ctv*

CtvDeclare(ADIOI_Flatlist_node*, ADIOI_Flatlist);
CtvDeclare(ADIOI_Datarep *, ADIOI_Datarep_head);
    /* list of datareps registered by the user */

/* for f2c and c2f conversion */
CtvDeclare(ADIO_File *, ADIOI_Ftable);
CtvDeclare(int, ADIOI_Ftable_ptr);
CtvDeclare(int, ADIOI_Ftable_max);
CtvDeclare(ADIO_Request *, ADIOI_Reqtable);
CtvDeclare(int, ADIOI_Reqtable_ptr);
CtvDeclare(int, ADIOI_Reqtable_max);
#ifndef HAVE_MPI_INFO
MPI_Info *MPIR_Infotable = NULL;
int MPIR_Infotable_ptr = 0, MPIR_Infotable_max = 0;
#endif

CtvDeclare(MPI_Info, ADIOI_syshints);

CtvDeclare(MPI_Op, ADIO_same_amode);

#if defined(ROMIO_XFS) || defined(ROMIO_LUSTRE)
int ADIOI_Direct_read = 0, ADIOI_Direct_write = 0;
#endif

CtvDeclare(int, ADIO_Init_keyval);

CtvDeclare(MPI_Errhandler, ADIOI_DFLT_ERR_HANDLER);


static void my_consensus(void *invec, void *inoutvec, int *len, MPI_Datatype *datatype)
{
    int i, *in, *inout;
    in = (int*)invec;
    inout = (int*)inoutvec;

    for (i=0; i< *len; i++) {
        if (in[i] != inout[i])
	    inout[i] = ADIO_AMODE_NOMATCH;
    }
    return;
}

void ADIO_Init(int *argc, char ***argv, int *error_code)
{
#if defined(ROMIO_XFS) || defined(ROMIO_LUSTRE)
    char *c;
#endif

    ADIOI_UNREFERENCED_ARG(argc);
    ADIOI_UNREFERENCED_ARG(argv);

    CtvInitialize(ADIOI_Flatlist_node*, ADIOI_Flatlist);
    CtvInitialize(ADIOI_Datarep *, ADIOI_Datarep_head);
    CtvInitialize(ADIO_File *, ADIOI_Ftable);
    CtvInitialize(int, ADIOI_Ftable_ptr);
    CtvInitialize(int, ADIOI_Ftable_max);
    CtvInitialize(ADIO_Request *, ADIOI_Reqtable);
    CtvInitialize(int, ADIOI_Reqtable_ptr);
    CtvInitialize(int, ADIOI_Reqtable_max);
    CtvInitialize(MPI_Info, ADIOI_syshints);
    CtvInitialize(MPI_Op, ADIO_same_amode);
    CtvInitialize(int, ADIO_Init_keyval);
    CtvInitialize(MPI_Errhandler, ADIOI_DFLT_ERR_HANDLER);

    CtvAccess(ADIOI_Flatlist) = NULL;
    CtvAccess(ADIOI_Datarep_head) = NULL;
    CtvAccess(ADIOI_Ftable) = NULL;
    CtvAccess(ADIOI_Ftable_ptr) = 0;
    CtvAccess(ADIOI_Ftable_max) = 0;
    CtvAccess(ADIOI_Reqtable) = NULL;
    CtvAccess(ADIOI_Reqtable_ptr) = 0;
    CtvAccess(ADIOI_Reqtable_max) = 0;
    CtvAccess(ADIOI_syshints) = MPI_INFO_NULL;
    CtvAccess(ADIO_same_amode) = MPI_OP_NULL;
    CtvAccess(ADIO_Init_keyval) = MPI_KEYVAL_INVALID;
    CtvAccess(ADIOI_DFLT_ERR_HANDLER) = MPI_ERRORS_RETURN;

/* initialize the linked list containing flattened datatypes */
    CtvAccess(ADIOI_Flatlist) = (ADIOI_Flatlist_node *) ADIOI_Malloc(sizeof(ADIOI_Flatlist_node));
    CtvAccess(ADIOI_Flatlist)->type = MPI_DATATYPE_NULL;
    CtvAccess(ADIOI_Flatlist)->next = NULL;
    CtvAccess(ADIOI_Flatlist)->blocklens = NULL;
    CtvAccess(ADIOI_Flatlist)->indices = NULL;

#if defined(ROMIO_XFS) || defined(ROMIO_LUSTRE)
    c = getenv("MPIO_DIRECT_READ");
    if (c && (!strcmp(c, "true") || !strcmp(c, "TRUE"))) 
	ADIOI_Direct_read = 1;
    else ADIOI_Direct_read = 0;
    c = getenv("MPIO_DIRECT_WRITE");
    if (c && (!strcmp(c, "true") || !strcmp(c, "TRUE"))) 
	ADIOI_Direct_write = 1;
    else ADIOI_Direct_write = 0;
#endif

    /* Assume system-wide hints won't change between runs: move hint processing
     * from ADIO_Open to here */
    /* FIXME should be checking error code from MPI_Info_create here */
    MPI_Info_create(&CtvAccess(ADIOI_syshints));
    ADIOI_process_system_hints(CtvAccess(ADIOI_syshints));

#ifdef ADIOI_MPE_LOGGING
    {
        MPE_Log_get_state_eventIDs( &ADIOI_MPE_open_a, &ADIOI_MPE_open_b );
        MPE_Log_get_state_eventIDs( &ADIOI_MPE_read_a, &ADIOI_MPE_read_b );
        MPE_Log_get_state_eventIDs( &ADIOI_MPE_write_a, &ADIOI_MPE_write_b );
        MPE_Log_get_state_eventIDs( &ADIOI_MPE_lseek_a, &ADIOI_MPE_lseek_b );
        MPE_Log_get_state_eventIDs( &ADIOI_MPE_close_a, &ADIOI_MPE_close_b );
        MPE_Log_get_state_eventIDs( &ADIOI_MPE_writelock_a,
                                    &ADIOI_MPE_writelock_b );
        MPE_Log_get_state_eventIDs( &ADIOI_MPE_readlock_a,
                                    &ADIOI_MPE_readlock_b );
        MPE_Log_get_state_eventIDs( &ADIOI_MPE_unlock_a, &ADIOI_MPE_unlock_b );
        MPE_Log_get_state_eventIDs( &ADIOI_MPE_postwrite_a,
                                    &ADIOI_MPE_postwrite_b );
	MPE_Log_get_state_eventIDs( &ADIOI_MPE_openinternal_a, 
			&ADIOI_MPE_openinternal_b);
	MPE_Log_get_state_eventIDs( &ADIOI_MPE_stat_a, &ADIOI_MPE_stat_b);
	MPE_Log_get_state_eventIDs( &ADIOI_MPE_iread_a, &ADIOI_MPE_iread_b);
	MPE_Log_get_state_eventIDs( &ADIOI_MPE_iwrite_a, &ADIOI_MPE_iwrite_b);

        int  comm_world_rank;
        MPI_Comm_rank( MPI_COMM_WORLD, &comm_world_rank );

        if ( comm_world_rank == 0 ) {
            MPE_Describe_state( ADIOI_MPE_open_a, ADIOI_MPE_open_b,
                                "open", "orange" );
            MPE_Describe_state( ADIOI_MPE_read_a, ADIOI_MPE_read_b,
                                "read", "green" );
            MPE_Describe_state( ADIOI_MPE_write_a, ADIOI_MPE_write_b,
                                "write", "blue" );
            MPE_Describe_state( ADIOI_MPE_lseek_a, ADIOI_MPE_lseek_b,
                                "lseek", "red" );
            MPE_Describe_state( ADIOI_MPE_close_a, ADIOI_MPE_close_b,
                                "close", "grey" );
            MPE_Describe_state( ADIOI_MPE_writelock_a, ADIOI_MPE_writelock_b,
                                "writelock", "plum" );
            MPE_Describe_state( ADIOI_MPE_readlock_a, ADIOI_MPE_readlock_b,
                                "readlock", "magenta" );
            MPE_Describe_state( ADIOI_MPE_unlock_a, ADIOI_MPE_unlock_b,
                                "unlock", "purple" );
            MPE_Describe_state( ADIOI_MPE_postwrite_a, ADIOI_MPE_postwrite_b,
                                "postwrite", "ivory" );
	    MPE_Describe_state( ADIOI_MPE_openinternal_a, ADIOI_MPE_openinternal_b, "open system", "blue");
	    MPE_Describe_state( ADIOI_MPE_stat_a, ADIOI_MPE_stat_b, "stat", "purple");
	    MPE_Describe_state( ADIOI_MPE_iread_a, ADIOI_MPE_iread_b, "iread", "purple");
	    MPE_Describe_state( ADIOI_MPE_iwrite_a, ADIOI_MPE_iwrite_b, "iwrite", "purple");
        }
    }
#endif

    *error_code = MPI_SUCCESS;
    MPI_Op_create(my_consensus, 1, &CtvAccess(ADIO_same_amode));
}
