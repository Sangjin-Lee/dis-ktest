#include "pr_fmt.h"

#include <linux/types.h>
#include <linux/string.h>

#include "dis_verbs.h"

int verbs_poll_cq(struct cqe_ctx *cqe)
{
    int ret, cqe_count = 0;
    pr_devel(DIS_STATUS_START);

    //TODO: Introduce max loop count?
    //TODO: Sleep if ret == 0?
    do {
        ret = ib_poll_cq(cqe->ibcq, cqe->num_entries, cqe->ibwc);
        if (ret < 0) {
            pr_devel(DIS_STATUS_FAIL);
            return -42;
        }
        cqe_count += ret;
    } while(cqe_count < cqe->num_entries);
    pr_devel(DIS_STATUS_COMPLETE);
    return 0;
}

int verbs_post_recv(struct rqe_ctx *rqe)
{
    int ret;
    pr_devel(DIS_STATUS_START);
    ret = ib_post_recv(rqe->ibqp, &rqe->ibwr, &rqe->ibbadwr);
    if (ret) {
        pr_devel(DIS_STATUS_FAIL);
        return -42;
    }
    pr_devel(DIS_STATUS_COMPLETE);
    return 0;
}

int verbs_post_send(struct sqe_ctx *sqe)
{
    int ret;
    pr_devel(DIS_STATUS_START);
    ret = ib_post_send(sqe->ibqp, &sqe->ibwr, &sqe->ibbadwr);
    if (ret) {
        pr_devel(DIS_STATUS_FAIL);
        return -42;
    }
    pr_devel(DIS_STATUS_COMPLETE);
    return 0;
}

// int verbs_alloc_mr(struct mr_ctx *mr)
// {
//     pr_devel(DIS_STATUS_START);
//     mr->ibmr = ib_alloc_mr(&mr->ibpd, IB_ACCESS_REMOTE_READ |
//                             IB_ACCESS_REMOTE_WRITE |
//                             IB_ACCESS_LOCAL_WRITE);
//     if (!mr->ibmr) {
//         pr_devel(DIS_STATUS_FAIL);
//         return -42;
//     }
//     pr_devel(DIS_STATUS_COMPLETE);
//     return 0;
// }

int verbs_create_qp(struct qp_ctx *qp)
{
    pr_devel(DIS_STATUS_START);
    qp->ibqp = ib_create_qp(qp->ibpd, &qp->attr);
    if (!qp->ibqp) {
        pr_devel(DIS_STATUS_FAIL);
        return -42;
    }
    pr_devel(DIS_STATUS_COMPLETE);
    return 0;
}

int verbs_create_cq(struct cq_ctx *cq)
{
    pr_devel(DIS_STATUS_START);
    cq->ibcq = ib_create_cq(cq->ibdev,
                                cq->comp_handler,
                                cq->event_handler,
                                cq->context,
                                &cq->attr);
    if (!cq->ibcq) {
        pr_devel(DIS_STATUS_FAIL);
        return -42;
    }
    pr_devel(DIS_STATUS_COMPLETE);
    return 0;
}

int verbs_alloc_pd(struct pd_ctx *pd)
{
    pr_devel(DIS_STATUS_START);
    pd->ibpd = ib_alloc_pd(pd->ibdev, pd->flags);
    if (!pd->ibpd) {
        pr_devel(DIS_STATUS_FAIL);
		return -42;
    }
    pr_devel(DIS_STATUS_COMPLETE);
    return 0;
}