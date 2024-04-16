#include <linux/printk.h>
#include <linux/mm_econ.h>
#include <linux/mm.h>
#include <linux/kobject.h>
#include <linux/init.h>


/* Modes of operation:
 * - 0: off (default linux behavior)
 * - 1: on	(cost-benefit estimation)
*/
static int mm_econ_mode = 0;

struct profile_range {
	u64 start;
	u64 end;
	u64 benefit;

	struct rb_node node;
};

/* The preloaded profile */
static struct rb_root preloaded_profile = RB_ROOT;

/* Search the profile for the range containing the given address, and return
 * it, otherwise return NULL.
 */
static struct profile_range *
profile_search(u64 address)
{
	struct rb_node *node = preloaded_profile.rb_node;

	while (node) {
		struct profile_range *range =
			container_of(node, struct profile_range, node);

		if (range->start <= addr && addr < range->end)
			return range;

		if (addr < range->start)
			node = node->rb_left;
		else
			node = node->rb_right;
	}

	return NULL;

}
/*
 * Insert the given range into the profile.
 */
static void
profile_range_insert(struct profile_range *new_range)
{
	struct rb_node **new = &(preloaded_profile.rb_node), *parent = NULL;

	while(*new) {
		struct profile_range *this =
			container_of(*new, struct profile_range, node);

		if (((new_range->start <= this->start && this->start < new_range->end)
					|| (this->start <= new_range->start && new_range->start < this->end)))
		{
			pr_err("Attempting to add overlapping profile range.\n");
			return;
		}

		parent = *new;
		if (new_range->start < this->start)
			new = &((*new)->rb_left);
		else if (new_range->start > this->start)
			new = &((*new)->rb_right);
		else
			break;
	}

	rb_link_node(&new_range->node, parent, new);
	rb_insert_color(&new_range->node, &preloaded_profile);
}

static void
profile_free_all(void)
{
	struct rb_node *node = preloaded_profile.rb_node;

	while(node) {
		struct profile_range *range =
			container_of(node, struct profile_range, node);

		rb_erase(node, &preloaded_profile);
		node = preloaded_profile.rb_node;

		vfree(range);
	}
}

/* Sysfs files to manipulate mm_econ options. */
static ssize_t enabled_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", mm_econ_mode);
}

static ssize_t enabled_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	int mode;
	int ret;

	ret = kstrtoint(buf, 0, &mode);

	if (ret != 0) {
		mm_econ_mode = 0;
		return ret;
	}
	else if (mode >= 0 && mode <= 1) {
		mm_econ_mode = mode;
		return count;
	}
	else {
		mm_econ_mode = 0;
		return -EINVAL;
	}
}

static struct kobj_attribute enabled_attr =
__ATTR(enabled, 0644, enabled_show, enabled_store);

static ssize_t preloaded_profile_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	ssize_t count = sprintf(buf, "\n");
	struct rb_node *node = rb_first(&preloaded_profile);

	pr_warn("mm_econ: profile...\n");

	while(node) {
		struct profile_range *range =
			container_of(node, struct profile_range, node);
		pr_warn("mm_econ: [%llu, %llu) (%llu bytes) misses=%llu\n",
				range->start, range->end,
				(range->end - range->start),
				range->misses);

		node = rb_next(node);
	}

	pr_warn("mm_econ: END profile...\n");

	return count;
}

static ssize_t preloaded_profile_store(struct kobject *kobj,
		struct kobj_attribute *attr,
		const char *buf, size_t count)
{
	char *tok = (char *)buf;
	struct profile_range *range = NULL;
	ssize_t error;
	int ret;
	u64 value;
	char *value_buf;

	profile_free_all();

	while (tok) {
		range = vmalloc(sizeof(struct profile_range));
		if (!range) {
			error = -ENOMEM;
			goto err;
		}

		// Get the start of range.
		value_buf = strsep(&tok, " ");
		if (!value_buf) {
			error = -EINVAL;
			goto err;
		}

		ret = kstrtoull(value_buf, 0, &value);
		if (ret != 0) {
			error = -EINVAL;
			goto err;
		}

		range->start = value;

		// Get the end of range.
		value_buf = strsep(&tok, " ");
        if (!value_buf) {
            error = -EINVAL;
            goto err;
        }

        ret = kstrtoull(value_buf, 0, &value);
        if (ret != 0) {
            error = -EINVAL;
            goto err;
        }

        range->end = value;

		// Get the benefit.
		value_buf = strsep(&tok, ";");
        if (!value_buf) {
            error = -EINVAL;
            goto err;
        }

        ret = kstrtoull(value_buf, 0, &value);
        if (ret != 0) {
            error = -EINVAL;
            goto err;
        }

        range->benefit = value;

		profile_range_insert(range);
	}

	return count;

err:
	if (range)
		vfree(range);
	profile_free_all();
	return error;
}

static struct kobj_attribute preloaded_profile_attr =
__ATTR(preloaded_profile, 0644, preloaded_profile_show, preloaded_profile_store);

static struct attribute *mm_econ_attr[] = {
	&enabled_attr.attr,
	&preloaded_profile_attr.attr,
	NULL,
};

static const struct attribute_group mm_econ_attr_group = {
	.attrs = mm_econ_attr,
};

static int __init mm_econ_init(void)
{
	struct kobject *mm_econ_kobj;
	int err;

	mm_econ_kobj = kobject_create_and_add("mm_econ", mm_kobj);
	if(unlikely(!mm_econ_kobj)) {
		pr_err("failed to create mm_econ kobject\n");
		return -ENOMEM;
	}

	err = sysfs_create_group(mm_econ_kobj, &mm_econ_attr_group);
	if (err) {
		pr_err("failed to register mm_econ group\n");
		kobject_put(mm_econ_kobj);
		return err;
	}

	return 0;
}
subsys_initcall(mm_econ_init);
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
            // TODO(markm)
            cost->cost = 0;
            cost->benefit = 0;
            return;

        case MM_ACTION_DEMOTE_HUGE:
            // TODO(markm)
            cost->cost = 0;
            cost->benefit = 0;
            return;

        case MM_ACTION_RUN_DEFRAG:
            // TODO(markm)
            cost->cost = 0;
            cost->benefit = 0;
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
	if (mm_econ_mode == 0) {
		return true;
	}
	else if (mm_econ_mode == 1) {
		/* For now keep default linux behaviour */
		pr_info("Estimator has been called.\n");
		return true;
	}
	else {
		BUG();
		return false;
	}
}
