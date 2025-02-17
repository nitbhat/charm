#ifndef _CONV_MACH_H
#define _CONV_MACH_H

#define CMK_CMIDELIVERS_USE_COMMON_CODE                    0
#define CMK_CMIDELIVERS_USE_SPECIAL_CODE                   1

#define CMK_CMIPRINTF_IS_A_BUILTIN                         0

#define CMK_GETPAGESIZE_AVAILABLE                          0

#define CMK_HANDLE_SIGUSR                                  1

#define CMK_IS_HETERO                                      0


#define CMK_MALLOC_USE_GNU_MALLOC                          0
#define CMK_MALLOC_USE_OS_BUILTIN                          1

#define CMK_MEMORY_PAGESIZE                                4096
#define CMK_MEMORY_PROTECTABLE                             1

#define CMK_MSG_HEADER_BASIC  CMK_MSG_HEADER_EXT
#define CMK_MSG_HEADER_EXT    { CmiUInt2 hdl,xhdl,info,stratid,root,redID,padding2,padding3; CmiUInt1 cmaMsgType:2;}

#define CMK_MULTICAST_GROUP_TYPE                struct { unsigned pe, id; }
#define CMK_MULTICAST_DEF_USE_COMMON_CODE                  1
#define CMK_MULTICAST_LIST_USE_COMMON_CODE                 1
#define CMK_MULTICAST_GROUP_USE_COMMON_CODE                1


#define CMK_SSH_IS_A_COMMAND                               1
#define CMK_SSH_NOT_NEEDED                                 0

#define CMK_SHARED_VARS_UNAVAILABLE                        0
#define CMK_SHARED_VARS_UNIPROCESSOR                       1

#define CMK_SIGNAL_NOT_NEEDED                              1
#define CMK_SIGNAL_USE_SIGACTION                           0
#define CMK_SIGNAL_USE_SIGACTION_WITH_RESTART              0

#define CMK_SPANTREE_MAXSPAN                               4
#define CMK_SPANTREE_USE_COMMON_CODE                       1

#define CMK_THREADS_REQUIRE_NO_CPV                         0
#define CMK_THREADS_COPY_STACK                             0

#define CMK_TIMER_USE_GETRUSAGE                            0
#define CMK_TIMER_USE_SPECIAL                              0
#define CMK_TIMER_USE_TIMES                                0
#define CMK_TIMER_SIM_USE_GETRUSAGE                        1
#define CMK_TIMER_SIM_USE_TIMES                            0


#define CMK_VECTOR_SEND_USES_COMMON_CODE                   1

#define CMK_WHEN_PROCESSOR_IDLE_BUSYWAIT                   1
#define CMK_WHEN_PROCESSOR_IDLE_USLEEP                     0



#define CMK_LBDB_ON					   0

#endif

