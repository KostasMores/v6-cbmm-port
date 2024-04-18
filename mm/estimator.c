
#include <linux/printk.h>
#include <linux/mm_econ.h>

// Estimates the change in the given metrics under the given action. Updates
// the given cost struct in place.
//
// Note that this is a pure function! It should not keep state regarding to
// previous queries.
void
mm_estimate_changes(const struct mm_action *action, struct mm_cost_delta *cost)
{
    switch (action->action) {
        case MM_ACTION_NONE:
			cost->cost = 0;
			cost->benefit = 0;
            return;

		case MM_ACTION_PROMOTE_HUGE:
			cost->cost = 0;
			cost->benefit = 0;
            return;

        case MM_ACTION_DEMOTE_HUGE:
        	cost->cost = 0;
        	cost->benefit = 0;
            return;

        case MM_ACTION_RUN_DEFRAG:
        	cost->cost = 0;
        	cost->benefit = 0;
            return;

        case MM_ACTION_EBPF_PROG_HOOKED:
			pr_info("mm_estimate_changes: ebpf program hooked and changed.\n");
			pr_info("\t\tnew cost = %llu, new benefit = %llu\n", cost->cost, cost->benefit);
            return;

        default:
            printk(KERN_WARNING "Unknown mm_action %d\n", action->action);
            return;
    }
}

// Decide whether to take an action with the given cost. Returns true if the
// action associated with `cost` should be TAKEN, and false otherwise.
bool mm_decide(const struct mm_cost_delta *cost)
{
    return true;
}


