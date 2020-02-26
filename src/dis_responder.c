#include "pr_fmt.h"

#include <linux/types.h>
#include <linux/string.h>

#include "dis_verbs.h"

void responder_cq_comp_handler(struct ib_cq *ibcq, void *cq_context)
{
    return;
}

int responder_receive_request(struct responder_ctx *ctx)
{
    return 0;
}

int responder_test(struct ib_device *ibdev)
{
    int ret;
    struct responder_ctx ctx;
    pr_devel(DIS_STATUS_START);

    ctx.ibdev = ibdev;
    ret = responder_receive_request(&ctx);

    pr_devel(DIS_STATUS_COMPLETE);
    return ret;
}