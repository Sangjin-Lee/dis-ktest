#include <rdma/ib_verbs.h>

#define TOTAL_PDS 1
#define TOTAL_CQS 1
#define TOTAL_QPS 1

struct pd_ctx {
    struct ib_device *ibdev;
    int flags;
    struct ib_pd *ibpd;
};

struct cq_ctx {
    struct ib_device *ibdev;
    void (*comp_handler)(struct ib_cq *ibcq, void *cq_context);
    void (*event_handler)(struct ib_event *ibevent, void *cq_context);
    void *context;
    struct ib_cq_init_attr attr;
    struct ib_cq *ibcq;
};

struct qp_ctx {
    struct ib_qp_init_attr qp_attr;
    struct ib_qp *ibqp;
};