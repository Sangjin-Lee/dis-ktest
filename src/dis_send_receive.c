#include "pr_fmt.h"

#include <linux/types.h>
#include <linux/string.h>
#include <linux/socket.h>

#include "dis_send_receive.h"
#include "dis_ktest.h"

void print_cq(struct cq_ctx *cq)
{
    int i;
    struct ib_wc *cqe;
    pr_info("Printing Result Of Transmission:");

    /* Print Result Of Transmission */
    for(i = 0; i < cq->cqe_c; i++) {
        cqe = &cq->cqe[i];
        switch (cqe->opcode)
        {
        case IB_WC_SEND:
            pr_info("CQE num: %d, Opcode: IB_WC_SEND, status: %s, wr_id: %d",
                    i, ib_wc_status_msg(cqe->status), (int)cqe->wr_id);
            break;
        
        case IB_WC_RECV:
            pr_info("CQE num: %d, Opcode: IB_WC_RECV, status: %s, wr_id: %d",
                    i, ib_wc_status_msg(cqe->status), (int)cqe->wr_id);
            break;
        default:
            pr_info("CQE num: %d, Opcode: Unknown", i);
            break;
        }
    }
}

void print_sge(struct sge_ctx *sge) {
    pr_devel("Send message : %s", sge->send_sge);
    pr_devel("Recv message : %s", sge->recv_sge);
}

void check_sge(struct sge_ctx *sge, int sge_num) {
    int i, fail_counter;
    fail_counter = 0;
    for (i = 0; i < SGE_LENGTH; i++) {
        if (sge->send_sge[i] != sge->recv_sge[i]) {
            pr_devel("Sge %d error at i: %d, send = %c, recv = %c\n",
                        sge_num, i, sge->send_sge[i], sge->recv_sge[i]);
            fail_counter++;
        }

        if(fail_counter > 10) {
            break;
        }
    }
}

int send_receive_init(struct send_receive_ctx *ctx)
{
    int i, ret, sleep_ms_count;
    struct dev_ctx *dev;
    struct pd_ctx *pd;
    struct cq_ctx *cq;
    struct qp_ctx *qp;
    struct rqe_ctx *rqe;
    struct sqe_ctx *sqe;
    struct sge_ctx *sge;
    struct mr_ctx *mr;
    pr_devel(DIS_STATUS_START);

    pr_devel("Querying Device Port: %d", 1);
    dev = &ctx->dev;
    dev->port_num = 1;
    ret = ib_query_port(dev->ibdev, dev->port_num, &dev->port_attr);
    if (ret) {
        pr_devel(DIS_STATUS_FAIL);
        return -42;
    }

    pr_devel("Creating Protection Domain: %d", ctx->pd_c);
    pd = &ctx->pd[ctx->pd_c];
    pd->ibdev = dev->ibdev;
    pd->flags = 0;
    pd->ibpd = ib_alloc_pd(pd->ibdev, pd->flags);
    if (!pd->ibpd) {
        pr_devel(DIS_STATUS_FAIL);
		return -42;
    }
    ctx->pd_c++;

    pr_devel("Creating Completion Queue: %d", ctx->cq_c);
    cq = &ctx->cq[ctx->cq_c];
    cq->ibdev           = dev->ibdev;
    cq->comp_handler    = NULL,
    cq->event_handler   = NULL,
    cq->context         = NULL,

    cq->init_attr.cqe           = 10,
    cq->init_attr.comp_vector   = 0,
    cq->init_attr.flags         = 0,

    cq->ibcq = ib_create_cq(cq->ibdev,
                                cq->comp_handler,
                                cq->event_handler,
                                cq->context,
                                &cq->init_attr);
    if (!cq->ibcq) {
        pr_devel(DIS_STATUS_FAIL);
        return -42;
    }
    ctx->cq_c++;

    pr_devel("Creating Queue Pair: %d", ctx->qp_c);
    qp = &ctx->qp[ctx->qp_c];
    qp->ibpd    = pd->ibpd;
    qp->send_cq = cq;
    qp->recv_cq = cq;

    qp->init_attr.qp_context    = NULL;
    qp->init_attr.send_cq       = cq->ibcq;
    qp->init_attr.recv_cq       = cq->ibcq;
    qp->init_attr.srq           = NULL;
    qp->init_attr.sq_sig_type   = IB_SIGNAL_ALL_WR;
    qp->init_attr.qp_type       = IB_QPT_RC;
    qp->init_attr.create_flags  = 0;

    qp->init_attr.cap.max_send_wr       = WQE_PER_QP + 10;
    qp->init_attr.cap.max_recv_wr       = WQE_PER_QP + 10;
	qp->init_attr.cap.max_send_sge      = SGE_PER_WQE;
	qp->init_attr.cap.max_recv_sge      = SGE_PER_WQE;
	qp->init_attr.cap.max_inline_data   = 0;

    qp->ibqp = ib_create_qp(qp->ibpd, &qp->init_attr);
    if (!qp->ibqp) {
        pr_devel(DIS_STATUS_FAIL);
        return -42;
    }
    ctx->qp_c++;

    pr_devel("Transitioning Queue Pair %d to INIT state", ctx->qp_c - 1);
    qp->attr.qp_state           = IB_QPS_INIT;
    qp->attr.qp_access_flags    = IB_ACCESS_REMOTE_WRITE;
    qp->attr.qp_access_flags    |= IB_ACCESS_REMOTE_READ;
    qp->attr.qp_access_flags    |= IB_ACCESS_LOCAL_WRITE;
    qp->attr.pkey_index         = 0;
    qp->attr.port_num           = dev->port_num;

    qp->attr_mask = IB_QP_STATE;
    qp->attr_mask |= IB_QP_ACCESS_FLAGS;
    qp->attr_mask |= IB_QP_PKEY_INDEX;
    qp->attr_mask |= IB_QP_PORT;

    ret = ib_modify_qp(qp->ibqp, &qp->attr, qp->attr_mask);
    if (ret) {
        pr_devel(DIS_STATUS_FAIL);
        return -42;
    }

    pr_devel("Transitioning Queue Pair %d to RTR state", ctx->qp_c - 1);
    qp->attr.qp_state               = IB_QPS_RTR;
    qp->attr.path_mtu               = IB_MTU_4096;
    qp->attr.dest_qp_num            = 100;
    qp->attr.rq_psn                 = 10;
    qp->attr.max_dest_rd_atomic     = 1;
    qp->attr.min_rnr_timer          = 1;

    qp->attr.ah_attr.sl             = 0;
    qp->attr.ah_attr.static_rate    = 1;
    qp->attr.ah_attr.type           = RDMA_AH_ATTR_TYPE_UNDEFINED;
    qp->attr.ah_attr.port_num       = dev->port_num;
    qp->attr.ah_attr.ah_flags       = 0; // Required by dis
    // qp->attr.ah_attr.ah_flags      = IB_AH_GRH; // Required by rxe

    qp->attr.ah_attr.grh.hop_limit  = 1;
    qp->attr.ah_attr.grh.sgid_index = 1;
    qp->attr.ah_attr.grh.sgid_attr  = NULL;

    qp->attr_mask = IB_QP_STATE;
    qp->attr_mask |= IB_QP_AV;
    qp->attr_mask |= IB_QP_PATH_MTU;
    qp->attr_mask |= IB_QP_DEST_QPN;
    qp->attr_mask |= IB_QP_RQ_PSN;
    qp->attr_mask |= IB_QP_MAX_DEST_RD_ATOMIC;
    qp->attr_mask |= IB_QP_MIN_RNR_TIMER;

    ret = ib_modify_qp(qp->ibqp, &qp->attr, qp->attr_mask);
    if (ret) {
        pr_devel(DIS_STATUS_FAIL);
        return -42;
    }

    pr_devel("Transitioing Queue Pair %d to RTS state", ctx->qp_c - 1);
    qp->attr.qp_state       = IB_QPS_RTS;
    qp->attr.timeout        = 10;
    qp->attr.retry_cnt      = 10;
    qp->attr.rnr_retry      = 10;
    qp->attr.sq_psn         = 10;
    qp->attr.max_rd_atomic  = 1;

    qp->attr_mask = IB_QP_STATE;
    qp->attr_mask |= IB_QP_TIMEOUT;
    qp->attr_mask |= IB_QP_RETRY_CNT;
    qp->attr_mask |= IB_QP_RNR_RETRY;
    qp->attr_mask |= IB_QP_SQ_PSN;
    qp->attr_mask |= IB_QP_MAX_QP_RD_ATOMIC;

    ret = ib_modify_qp(qp->ibqp, &qp->attr, qp->attr_mask);
    if (ret) {
        pr_devel(DIS_STATUS_FAIL);
        return -42;
    }
    
    /* Set up connection to requester */
    //TODO: Set up socket based exchange of GID

    pr_devel("Initializing Send/Receive Segment: %d\n", ctx->sge_c);
    sge = &ctx->sge[ctx->sge_c];
    strncpy(sge->send_sge, "Hello There!", SGE_LENGTH);
    strncpy(sge->recv_sge, "", SGE_LENGTH);
    sge->length = SGE_LENGTH;
    ctx->sge_c++;
    
    pr_devel("Initializing Memory Region for Send/Receive segment: %d\n", 
                ctx->sge_c - 1);
    mr = &ctx->mr[ctx->mr_c];
    mr->ibpd = pd->ibpd;
    mr->access = IB_ACCESS_LOCAL_WRITE;
    mr->ibmr = pd->ibpd->device->ops.get_dma_mr(mr->ibpd, mr->access);
    if (!mr->ibmr) {
        pr_devel(DIS_STATUS_FAIL);
        return -42;
    }
    ctx->mr_c++;
    sge->lkey = mr->ibmr->lkey;

    pr_devel("Initializing Send/Receive Segment: %d\n", ctx->sge_c);
    sge = &ctx->sge[ctx->sge_c];
    strncpy(sge->send_sge, "Gerneral Kenobi", SGE_LENGTH);
    strncpy(sge->recv_sge, "", SGE_LENGTH);
    sge->length = SGE_LENGTH;
    ctx->sge_c++;
    
    pr_devel("Initializing Memory Region for Send/Receive segment: %d\n", 
                ctx->sge_c - 1);
    mr = &ctx->mr[ctx->mr_c];
    mr->ibpd = pd->ibpd;
    mr->access = IB_ACCESS_LOCAL_WRITE;
    mr->ibmr = pd->ibpd->device->ops.get_dma_mr(mr->ibpd, mr->access);
    if (!mr->ibmr) {
        pr_devel(DIS_STATUS_FAIL);
        return -42;
    }
    ctx->mr_c++;
    sge->lkey = mr->ibmr->lkey;

    pr_devel("Posting Receive Queue Element: %d", qp->rqe_c);
    rqe = &qp->rqe[qp->rqe_c];
    rqe->ibqp               = qp->ibqp;
    rqe->ibbadwr            = NULL;

    rqe->ibwr.num_sge       = ctx->sge_c;
    rqe->ibwr.wr_id         = qp->rqe_c;
    rqe->ibwr.next          = NULL;
    rqe->ibwr.sg_list       = rqe->ibsge;
    
    for (i = 0; i < ctx->sge_c; i++) {
        rqe->ibsge[i].addr      = (uintptr_t)ctx->sge[i].recv_sge;
        rqe->ibsge[i].length    = ctx->sge[i].length;
        rqe->ibsge[i].lkey      = ctx->sge[i].lkey;
    }

    ret = ib_post_recv(rqe->ibqp, &rqe->ibwr, &rqe->ibbadwr);
    if (ret) {
        pr_devel(DIS_STATUS_FAIL);
        return -42;
    }
    qp->recv_cq->cqe_expected++;
    qp->rqe_c++;

    pr_devel("Posting Send Queue Element: %d", qp->sqe_c);
    sqe = &qp->sqe[qp->sqe_c];
    sqe->ibqp               = qp->ibqp;
    sqe->ibbadwr            = NULL;

	sqe->ibwr.opcode        = IB_WR_SEND;
	sqe->ibwr.send_flags    = IB_SEND_SIGNALED;
    sqe->ibwr.num_sge       = ctx->sge_c;
	sqe->ibwr.wr_id         = qp->sqe_c;
    sqe->ibwr.sg_list       = sqe->ibsge;

    for (i = 0; i < ctx->sge_c; i++) {
        sqe->ibsge[i].addr      = (uintptr_t)ctx->sge[i].send_sge;
        sqe->ibsge[i].length    = ctx->sge[i].length;
        sqe->ibsge[i].lkey      = ctx->sge[i].lkey;
    }

    ret = ib_post_send(sqe->ibqp, &sqe->ibwr, &sqe->ibbadwr);
    if (ret) {
        pr_devel(DIS_STATUS_FAIL);
        return -42;
    }
    qp->send_cq->cqe_expected++;
    qp->sqe_c++;

    pr_devel("Polling Completion Queue: %d", ctx->cq_c);
    sleep_ms_count = 0;
    while(sleep_ms_count < POLL_TIMEOUT_SEC * 1000) {
        ret = ib_poll_cq(cq->ibcq,
                            cq->cqe_expected - cq->cqe_c,
                            &cq->cqe[cq->cqe_c]);
        if (ret < 0) {
            pr_devel(DIS_STATUS_FAIL);
            return -42;
        }

        cq->cqe_c += ret;
        if(cq->cqe_c >= cq->cqe_expected) {
           break;
        }

        msleep(POLL_INTERVAL_MSEC);
        sleep_ms_count += POLL_INTERVAL_MSEC;
    }

    /* Print results */
    for (i = 0; i < ctx->cq_c; i++) {
        print_cq(&ctx->cq[i]);
    }

    for (i = 0; i < ctx->sge_c; i++) {
        print_sge(&ctx->sge[i]);
    }

    for (i = 0; i < ctx->sge_c; i++) {
        check_sge(&ctx->sge[i], i);
    }

    pr_devel(DIS_STATUS_COMPLETE);
    return 0;
}

void send_receive_exit(struct send_receive_ctx *ctx) 
{
    int i;
    pr_devel("Cleaning up context");

    for (i = 0; i < ctx->mr_c; i++) {
        ctx->mr[i].ibpd->device->ops.dereg_mr(ctx->mr[i].ibmr, NULL);
        pr_devel("Destroy MR %d: " DIS_STATUS_COMPLETE, i);
    }

    for (i = 0; i < ctx->qp_c; i++) {
        ib_destroy_qp(ctx->qp[i].ibqp);
        pr_devel("Destroy QP %d: " DIS_STATUS_COMPLETE, i);
    }

    for (i = 0; i < ctx->cq_c; i++) {
        ib_destroy_cq(ctx->cq[i].ibcq);
        pr_devel("Destroy CQ %d: " DIS_STATUS_COMPLETE, i);
    }

    for (i = 0; i < ctx->pd_c; i++) {
        ib_dealloc_pd(ctx->pd[i].ibpd);
        pr_devel("Destroy PD %d: " DIS_STATUS_COMPLETE, i);
    }
    
    pr_devel(DIS_STATUS_COMPLETE);
}