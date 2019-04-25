struct fid_mr* registerDirectMemory(void *info, const void *addr, int size) {
  CmiOfiRdmaPtr_t *rdmaInfo = (CmiOfiRdmaPtr_t *)info;
  uint64_t requested_key = 0;
  int ret;

  if(FI_MR_SCALABLE == context.mr_mode) {
    requested_key = __sync_fetch_and_add(&(context.mr_counter), 1);
  }
  ret = fi_mr_reg(context.domain,
                  addr,
                  size,
                  FI_REMOTE_READ | FI_REMOTE_WRITE | FI_READ | FI_WRITE,
                  0ULL,
                  requested_key,
                  0ULL,
                  &(rdmaInfo->mr),
                  NULL);
  if (ret) {
    CmiAbort("registerDirectMemory: fi_mr_reg failed!\n");
  }
  rdmaInfo->key = fi_mr_key(rdmaInfo->mr);
}


void ofi_post_nocopy_operation(
  char *lbuf,
  const char *rbuf,
  int  remoteNodeNo,
  uint64_t rkey,
  int size,
  struct fid_mr *lmr,
  ofiCallbackFn done_fn,
  void *cbRef,
  int  *completion_count,
  int operation) {

  int remaining = size;
  size_t chunk_size;
  OFIRequest *rma_req;

  while (remaining > 0) {
    chunk_size = (remaining <= context.rma_maxsize) ? remaining : context.rma_maxsize;

#if USE_OFIREQUEST_CACHE
    rma_req = alloc_request(context.request_cache);
#else
    rma_req = (OFIRequest*)CmiAlloc(sizeof(OFIRequest));
#endif

    CmiAssert(rma_req);
    rma_req->callback = done_fn;
    rma_req->data.rma_ncpy_info = cbRef;

    (*completion_count)++;

    if(operation == OFI_READ_OP) {
      // Perform an RDMA read or get operation
      OFI_RETRY(fi_read(context.ep,
                        lbuf,
                        chunk_size,
                        (lmr) ? fi_mr_desc(lmr) : NULL,
                        remoteNodeNo,
                        (uint64_t)rbuf,
                        rkey,
                        &rma_req->context));
    } else if(operation == OFI_WRITE_OP) {
      // Perform an RDMA write or put operation
      struct iovec l_iovec{};
      l_iovec.iov_base = (void*)lbuf;
      l_iovec.iov_len = chunk_size;

      struct fi_rma_iov rma_iov{};
      rma_iov.addr = (uint64_t)rbuf;
      rma_iov.len = chunk_size;
      rma_iov.key = rkey;

      void *desc = (lmr ? fi_mr_desc(lmr) : NULL);

      struct fi_msg_rma msg{};
      msg.msg_iov = &l_iovec;
      msg.desc = &desc;
      msg.iov_count = 1;
      msg.addr = (fi_addr_t)remoteNodeNo;
      msg.rma_iov = &rma_iov;
      msg.rma_iov_count = 1;
      msg.context = &rma_req->context;
      msg.data = 0;

      OFI_RETRY(fi_writemsg(context.ep,
                           &msg,
                           FI_DELIVERY_COMPLETE));
    } else {
      CmiAbort("ofi_post_nocopy_operation: Invalid RDMA operation\n");
    }

    remaining -= chunk_size;
    lbuf      += chunk_size;
    rbuf      += chunk_size;
  }
}

/* Support for Nocopy Direct API */
// Method called on the completion of a direct onesided operation

// Set the machine specific information for a nocopy pointer
void LrtsSetRdmaBufferInfo(void *info, const void *ptr, int size, unsigned short int mode){
  registerDirectMemory(info, ptr, size);
}

static inline void ofi_onesided_direct_operation_callback(struct fi_cq_tagged_entry *e, OFIRequest *req)
{
  CmiOfiRdmaComp_t *rdmaComp = (CmiOfiRdmaComp_t *)(req->data.rma_ncpy_info);
  rdmaComp->completion_count--;
  if(0 == rdmaComp->completion_count) {

    // Invoke the ack handler
    CmiInvokeNcpyAck(rdmaComp->ack_info);
    free(rdmaComp);
  }
#if USE_OFIREQUEST_CACHE
  free_request(req);
#else
  CmiFree(req);
#endif
}

void process_onesided_reg_and_put(struct fi_cq_tagged_entry *e, OFIRequest *req) {

  char *data = (char *)req->data.rma_ncpy_ack;

  // Allocate a new receiver buffer to receive other messages
  req->data.recv_buffer = CmiAlloc(context.eager_maxsize);

  NcpyOperationInfo *ncpyOpInfo = (NcpyOperationInfo *)(data);
  resetNcpyOpInfoPointers(ncpyOpInfo);

  // Free the NcpyOperationInfo in a reverse API operation
  ncpyOpInfo->freeMe = CMK_FREE_NCPYOPINFO;

  registerDirectMemory(ncpyOpInfo->srcLayerInfo + CmiGetRdmaCommonInfoSize(),
                       ncpyOpInfo->srcPtr,
                       ncpyOpInfo->srcSize);

  const char *rbuf  = (FI_MR_SCALABLE == context.mr_mode) ? 0 : (const char*)(ncpyOpInfo->destPtr);

  // Allocate a completion object for tracking write completion and ack handling
  CmiOfiRdmaComp_t* rdmaComp = (CmiOfiRdmaComp_t *)malloc(sizeof(CmiOfiRdmaComp_t));
  rdmaComp->ack_info         = ncpyOpInfo;
  rdmaComp->completion_count = 0;

  ofi_post_nocopy_operation(
      (char*)(ncpyOpInfo->srcPtr),
      rbuf,
      CmiNodeOf(ncpyOpInfo->destPe),
      ((CmiOfiRdmaPtr_t *)((char *)(ncpyOpInfo->destLayerInfo) + CmiGetRdmaCommonInfoSize()))->key,
      ncpyOpInfo->srcSize,
      ((CmiOfiRdmaPtr_t *)((char *)(ncpyOpInfo->srcLayerInfo) + CmiGetRdmaCommonInfoSize()))->mr,
      ofi_onesided_direct_operation_callback,
      (void *)rdmaComp,
      &(rdmaComp->completion_count),
      OFI_WRITE_OP);
}

void process_onesided_reg_and_get(struct fi_cq_tagged_entry *e, OFIRequest *req) {

  char *data = (char *)req->data.rma_ncpy_ack;

  // Allocate a new receiver buffer to receive other messages
  req->data.recv_buffer = CmiAlloc(context.eager_maxsize);

  NcpyOperationInfo *ncpyOpInfo = (NcpyOperationInfo *)(data);
  resetNcpyOpInfoPointers(ncpyOpInfo);

  // Free this message
  ncpyOpInfo->freeMe = CMK_FREE_NCPYOPINFO;

  registerDirectMemory(ncpyOpInfo->destLayerInfo + CmiGetRdmaCommonInfoSize(),
                       ncpyOpInfo->destPtr,
                       ncpyOpInfo->srcSize);

  const char *rbuf  = (FI_MR_SCALABLE == context.mr_mode) ? 0 : (const char*)(ncpyOpInfo->srcPtr);

  // Allocate a completion object for tracking write completion and ack handling
  CmiOfiRdmaComp_t* rdmaComp = (CmiOfiRdmaComp_t *)malloc(sizeof(CmiOfiRdmaComp_t));
  rdmaComp->ack_info         = ncpyOpInfo;
  rdmaComp->completion_count = 0;

  ofi_post_nocopy_operation(
      (char*)(ncpyOpInfo->destPtr),
      rbuf,
      CmiNodeOf(ncpyOpInfo->srcPe),
      ((CmiOfiRdmaPtr_t *)((char *)(ncpyOpInfo->srcLayerInfo) + CmiGetRdmaCommonInfoSize()))->key,
      ncpyOpInfo->srcSize,
      ((CmiOfiRdmaPtr_t *)((char *)(ncpyOpInfo->destLayerInfo) + CmiGetRdmaCommonInfoSize()))->mr,
      ofi_onesided_direct_operation_callback,
      (void *)rdmaComp,
      &(rdmaComp->completion_count),
      OFI_READ_OP);
}


// Perform an RDMA Get call into the local destination address from the remote source address
void LrtsIssueRget(NcpyOperationInfo *ncpyOpInfo) {

  OFIRequest *req;

  if(ncpyOpInfo->isSrcRegistered == 0) {
    // Remote buffer is unregistered, send a message to register it and perform PUT
#if USE_OFIREQUEST_CACHE
    req = alloc_request(context.request_cache);
#else
    req = (OFIRequest*)CmiAlloc(sizeof(OFIRequest));
#endif
    CmiAssert(req);

    ZERO_REQUEST(req);

    // set OpMode for reverse operation
    setReverseModeForNcpyOpInfo(ncpyOpInfo);

    if(ncpyOpInfo->freeMe == CMK_FREE_NCPYOPINFO)
      req->freeMe   = 1;// free ncpyOpInfo
    else
      req->freeMe   = 0;// do not free ncpyOpInfo

    req->destNode = CmiNodeOf(ncpyOpInfo->srcPe);
    req->destPE   = ncpyOpInfo->srcPe;
    req->size     = ncpyOpInfo->ncpyOpInfoSize;
    req->callback = send_short_callback;
    req->data.short_msg = ncpyOpInfo;

    ofi_send(ncpyOpInfo,
             ncpyOpInfo->ncpyOpInfoSize,
             CmiNodeOf(ncpyOpInfo->srcPe),
             OFI_RDMA_DIRECT_REG_AND_PUT,
             req);
  } else {

    CmiOfiRdmaPtr_t *dest_info = (CmiOfiRdmaPtr_t *)((char *)ncpyOpInfo->destLayerInfo + CmiGetRdmaCommonInfoSize());
    CmiOfiRdmaPtr_t *src_info = (CmiOfiRdmaPtr_t *)((char *)ncpyOpInfo->srcLayerInfo + CmiGetRdmaCommonInfoSize());

    const char *rbuf        = (FI_MR_SCALABLE == context.mr_mode) ? 0 : (const char*)(ncpyOpInfo->srcPtr);

    // Allocate a completion object for tracking read completion and ack handling
    CmiOfiRdmaComp_t* rdmaComp = (CmiOfiRdmaComp_t *)malloc(sizeof(CmiOfiRdmaComp_t));
    rdmaComp->ack_info         = ncpyOpInfo;
    rdmaComp->completion_count = 0;

    ofi_post_nocopy_operation(
        (char*)(ncpyOpInfo->destPtr),
        rbuf,
        CmiNodeOf(ncpyOpInfo->srcPe),
        src_info->key,
        ncpyOpInfo->srcSize,
        dest_info->mr,
        ofi_onesided_direct_operation_callback,
        (void *)rdmaComp,
        &(rdmaComp->completion_count),
        OFI_READ_OP);
  }
}

// Perform an RDMA Put call into the remote destination address from the local source address
void LrtsIssueRput(NcpyOperationInfo *ncpyOpInfo) {

  OFIRequest *req;

  if(ncpyOpInfo->isDestRegistered == 0) {
    // Remote buffer is unregistered, send a message to register it and perform PUT
#if USE_OFIREQUEST_CACHE
    req = alloc_request(context.request_cache);
#else
    req = (OFIRequest*)CmiAlloc(sizeof(OFIRequest));
#endif
    CmiAssert(req);

    ZERO_REQUEST(req);

    req->destNode = CmiNodeOf(ncpyOpInfo->destPe);
    req->destPE   = ncpyOpInfo->destPe;
    req->size     = ncpyOpInfo->ncpyOpInfoSize;
    req->callback = send_short_callback;
    req->data.short_msg = ncpyOpInfo;

    ofi_send(ncpyOpInfo,
             ncpyOpInfo->ncpyOpInfoSize,
             CmiNodeOf(ncpyOpInfo->destPe),
             OFI_RDMA_DIRECT_REG_AND_GET,
             req);
  } else {

    CmiOfiRdmaPtr_t *dest_info = (CmiOfiRdmaPtr_t *)((char *)(ncpyOpInfo->destLayerInfo) + CmiGetRdmaCommonInfoSize());
    CmiOfiRdmaPtr_t *src_info = (CmiOfiRdmaPtr_t *)((char *)(ncpyOpInfo->srcLayerInfo) + CmiGetRdmaCommonInfoSize());

    const char *rbuf        = (FI_MR_SCALABLE == context.mr_mode) ? 0 : (const char*)(ncpyOpInfo->destPtr);

    // Allocate a completion object for tracking write completion and ack handling
    CmiOfiRdmaComp_t* rdmaComp = (CmiOfiRdmaComp_t *)malloc(sizeof(CmiOfiRdmaComp_t));
    rdmaComp->ack_info         = ncpyOpInfo;
    rdmaComp->completion_count = 0;

    ofi_post_nocopy_operation(
        (char*)(ncpyOpInfo->srcPtr),
        rbuf,
        CmiNodeOf(ncpyOpInfo->destPe),
        dest_info->key,
        ncpyOpInfo->srcSize,
        src_info->mr,
        ofi_onesided_direct_operation_callback,
        (void *)rdmaComp,
        &(rdmaComp->completion_count),
        OFI_WRITE_OP);
  }
}

// Method invoked to deregister memory handle
void LrtsDeregisterMem(const void *ptr, void *info, int pe, unsigned short int mode){
  CmiOfiRdmaPtr_t *rdmaSrc = (CmiOfiRdmaPtr_t *)info;
  int ret;

  if(mode != CMK_BUFFER_NOREG && rdmaSrc->mr) {
    // Deregister the buffer
    ret = fi_close((struct fid *)rdmaSrc->mr);
    if(ret)
      CmiAbort("LrtsDeregisterMem: fi_close(mr) failed!\n");
  }
}


void LrtsInvokeRemoteDeregAckHandler(int pe, NcpyOperationInfo *ncpyOpInfo) {

  OFIRequest *req;

  // Send a message to de-register buffer and invoke source ack
#if USE_OFIREQUEST_CACHE
  req = alloc_request(context.request_cache);
#else
  req = (OFIRequest*)CmiAlloc(sizeof(OFIRequest));
#endif
  CmiAssert(req);

  ZERO_REQUEST(req);

  if(ncpyOpInfo->freeMe == CMK_FREE_NCPYOPINFO)
    req->freeMe   = 1;// free ncpyOpInfo
  else
    req->freeMe   = 0;// do not free ncpyOpInfo

  req->destNode = CmiNodeOf(ncpyOpInfo->srcPe);
  req->destPE   = ncpyOpInfo->srcPe;
  req->size     = ncpyOpInfo->ncpyOpInfoSize;
  req->callback = send_short_callback;
  req->data.short_msg = ncpyOpInfo;

  ofi_send(ncpyOpInfo,
           ncpyOpInfo->ncpyOpInfoSize,
           CmiNodeOf(ncpyOpInfo->srcPe),
           OFI_RDMA_DIRECT_DEREG_AND_ACK,
           req);
}

void process_onesided_dereg_and_ack(struct fi_cq_tagged_entry *e, OFIRequest *req) {

  char *data = (char *)req->data.rma_ncpy_ack;

  // Allocate a new receiver buffer to receive other messages
  req->data.recv_buffer = CmiAlloc(context.eager_maxsize);

  NcpyOperationInfo *ncpyOpInfo = (NcpyOperationInfo *)(data);
  resetNcpyOpInfoPointers(ncpyOpInfo);

  ncpyOpInfo->freeMe = CMK_FREE_NCPYOPINFO;

  LrtsDeregisterMem(ncpyOpInfo->srcPtr,
                    ncpyOpInfo->srcLayerInfo + CmiGetRdmaCommonInfoSize(),
                    ncpyOpInfo->srcPe,
                    ncpyOpInfo->srcRegMode);

  ncpyOpInfo->isSrcRegistered = 0; // Set isSrcRegistered to 0 after de-registration

  // Invoke source ack
  ncpyOpInfo->opMode = CMK_EM_API_SRC_ACK_INVOKE;
  CmiInvokeNcpyAck(ncpyOpInfo);
}


