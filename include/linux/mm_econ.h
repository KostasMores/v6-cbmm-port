#ifndef _MM_ECON_H_
#define _MM_ECON_H_

#include <linux/types.h>

#define MM_ACTION_NONE                 0
#define MM_ACTION_PROMOTE_HUGE  (1 <<  0)
#define MM_ACTION_DEMOTE_HUGE   (1 <<  1)
#define MM_ACTION_RUN_DEFRAG    (1 <<  2)
#define MM_ACTION_EBPF_PROG_HOOKED  (1 <<  4)

// An action that may be taken by the memory management subsystem.
struct mm_action {
    int action;

	u64 address;
    // Extra parameters of the action.
    union {
        // No extra parameters are needed.
        u64 unused;

        // How large is the huge page we are creating? This is the order (e.g. 2MB would be 9)
        u64 huge_page_order;

        // How long the defragmenter runs.
        u64 how_long;
    };
};

// The cost of a particular action relative to the status quo.
struct mm_cost_delta {
	/* Total estimated cost in cycles */
	u64 cost;

	/* Total estimated benefit in cycles*/
	u64 benefit;
};

bool mm_decide(const struct mm_cost_delta *cost);

void
mm_estimate_changes(const struct mm_action *action, struct mm_cost_delta *cost);

#endif
