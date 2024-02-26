// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2019-2022 Linaro Limited
 */

#define LOG_CATEGORY UCLASS_CLK

#include <common.h>
#include <clk-uclass.h>
#include <dm.h>
#include <scmi_agent.h>
#include <scmi_protocols.h>
#include <asm/types.h>
#include <linux/clk-provider.h>

/**
 * struct scmi_clk_plat - Platform data for SCMI clocks
 * @channel: Reference to the SCMI channel to use
 */
struct scmi_clk_plat {
	struct scmi_channel *channel;
};

static int scmi_clk_get_num_clock(struct udevice *dev, size_t *num_clocks)
{
	struct scmi_clk_plat *plat = dev_get_plat(dev);
	struct scmi_clk_protocol_attr_out out;
	struct scmi_msg msg = {
		.protocol_id = SCMI_PROTOCOL_ID_CLOCK,
		.message_id = SCMI_PROTOCOL_ATTRIBUTES,
		.out_msg = (u8 *)&out,
		.out_msg_sz = sizeof(out),
	};
	int ret;

	ret = devm_scmi_process_msg(dev, plat->channel, &msg);
	if (ret)
		return ret;

	*num_clocks = out.attributes & SCMI_CLK_PROTO_ATTR_COUNT_MASK;

	return 0;
}

static int scmi_clk_get_attibute(struct udevice *dev, int clkid, char **name)
{
	struct scmi_clk_plat *plat = dev_get_plat(dev);
	struct scmi_clk_attribute_in in = {
		.clock_id = clkid,
	};
	struct scmi_clk_attribute_out out;
	struct scmi_msg msg = {
		.protocol_id = SCMI_PROTOCOL_ID_CLOCK,
		.message_id = SCMI_CLOCK_ATTRIBUTES,
		.in_msg = (u8 *)&in,
		.in_msg_sz = sizeof(in),
		.out_msg = (u8 *)&out,
		.out_msg_sz = sizeof(out),
	};
	int ret;

	ret = devm_scmi_process_msg(dev, plat->channel, &msg);
	if (ret)
		return ret;

	*name = strdup(out.clock_name);

	return 0;
}

static int scmi_clk_gate(struct clk *clk, int enable)
{
	struct scmi_clk_plat *plat = dev_get_plat(clk->dev);
	struct scmi_clk_state_in in = {
		.clock_id = clk->id,
		.attributes = enable,
	};
	struct scmi_clk_state_out out;
	struct scmi_msg msg = SCMI_MSG_IN(SCMI_PROTOCOL_ID_CLOCK,
					  SCMI_CLOCK_CONFIG_SET,
					  in, out);
	int ret;

	ret = devm_scmi_process_msg(clk->dev, plat->channel, &msg);
	if (ret)
		return ret;

	ret = scmi_to_linux_errno(out.status);
	if (ret == -EACCES) {
		debug("Ignore %s enable failure\n", clk_hw_get_name(clk));
		ret = 0;
	}

	return ret;
}

static int scmi_clk_enable(struct clk *clk)
{
	return scmi_clk_gate(clk, 1);
}

static int scmi_clk_disable(struct clk *clk)
{
	return scmi_clk_gate(clk, 0);
}

static ulong scmi_clk_get_rate(struct clk *clk)
{
	struct scmi_clk_plat *plat = dev_get_plat(clk->dev);
	struct scmi_clk_rate_get_in in = {
		.clock_id = clk->id,
	};
	struct scmi_clk_rate_get_out out;
	struct scmi_msg msg = SCMI_MSG_IN(SCMI_PROTOCOL_ID_CLOCK,
					  SCMI_CLOCK_RATE_GET,
					  in, out);
	int ret;

	ret = devm_scmi_process_msg(clk->dev, plat->channel, &msg);
	if (ret < 0)
		return ret;

	ret = scmi_to_linux_errno(out.status);
	if (ret < 0)
		return ret;

	return (ulong)(((u64)out.rate_msb << 32) | out.rate_lsb);
}

static ulong __scmi_clk_set_rate(struct clk *clk, ulong rate)
{
	struct scmi_clk_plat *plat = dev_get_plat(clk->dev);
	struct scmi_clk_rate_set_in in = {
		.clock_id = clk->id,
		.flags = SCMI_CLK_RATE_ROUND_CLOSEST,
		.rate_lsb = (u32)rate,
		.rate_msb = (u32)((u64)rate >> 32),
	};
	struct scmi_clk_rate_set_out out;
	struct scmi_msg msg = SCMI_MSG_IN(SCMI_PROTOCOL_ID_CLOCK,
					  SCMI_CLOCK_RATE_SET,
					  in, out);
	int ret;

	ret = devm_scmi_process_msg(clk->dev, plat->channel, &msg);
	if (ret < 0)
		return ret;

	ret = scmi_to_linux_errno(out.status);
	if (ret < 0)
		return ret;

	return scmi_clk_get_rate(clk);
}

static ulong scmi_clk_set_rate(struct clk *clk, ulong rate)
{
	ulong orig_rate;

	orig_rate = scmi_clk_get_rate(clk);
	if (orig_rate == rate)
		return orig_rate;

	return __scmi_clk_set_rate(clk, rate);
}

static int scmi_clk_probe(struct udevice *dev)
{
	struct scmi_clk_plat *plat = dev_get_plat(dev);
	struct scmi_clk_plat *child_plat;
	struct clk *clk;
	size_t num_clocks, i;
	int ret;

	if (!CONFIG_IS_ENABLED(CLK_CCF)) {
		ret = devm_scmi_of_get_channel(dev, &plat->channel);
		return ret;
	}

	/* register CCF children: CLK UCLASS, no probed again */
	if (device_get_uclass_id(dev->parent) == UCLASS_CLK)
		return 0;

	ret = devm_scmi_of_get_channel(dev, &plat->channel);
	if (ret)
		return ret;

	ret = scmi_clk_get_num_clock(dev, &num_clocks);
	if (ret)
		return ret;

	for (i = 0; i < num_clocks; i++) {
		char *clock_name;

		if (!scmi_clk_get_attibute(dev, i, &clock_name)) {
			clk = kzalloc(sizeof(*clk), GFP_KERNEL);
			if (!clk || !clock_name)
				ret = -ENOMEM;
			else
				ret = clk_register(clk, dev->driver->name,
						   clock_name, dev->name);

			if (ret) {
				free(clk);
				free(clock_name);
				return ret;
			}

			/* Assign the SCMI channel of parent to each child plat */
			child_plat = dev_get_plat(clk->dev);
			child_plat->channel = plat->channel;

			clk_dm(i, clk);
		}
	}

	return 0;
}

static int scmi_clk_set_parent(struct clk *clk, struct clk *parent)
{
	struct scmi_clk_plat *plat = dev_get_plat(clk->dev);
	struct scmi_clk_parent_set_in in = {
		.clock_id = clk->id,
		.parent_clk = parent->id,
	};
	struct scmi_clk_parent_set_out out;
	struct scmi_msg msg = SCMI_MSG_IN(SCMI_PROTOCOL_ID_CLOCK,
					  SCMI_CLOCK_PARENT_SET,
					  in, out);
	int ret;

	ret = devm_scmi_process_msg(clk->dev, plat->channel, &msg);
	if (ret < 0)
		return ret;

	return scmi_to_linux_errno(out.status);
}

static const struct clk_ops scmi_clk_ops = {
	.enable = scmi_clk_enable,
	.disable = scmi_clk_disable,
	.get_rate = scmi_clk_get_rate,
	.set_rate = scmi_clk_set_rate,
	.set_parent = scmi_clk_set_parent,
};

U_BOOT_DRIVER(scmi_clock) = {
	.name = "scmi_clk",
	.id = UCLASS_CLK,
	.ops = &scmi_clk_ops,
	.probe = scmi_clk_probe,
	.plat_auto = sizeof(struct scmi_clk_plat *),
	.flags = DM_FLAG_PRE_RELOC,
};
