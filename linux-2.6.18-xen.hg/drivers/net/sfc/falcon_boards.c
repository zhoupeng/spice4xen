/****************************************************************************
 * Driver for Solarflare Solarstorm network controllers and boards
 * Copyright 2007-2009 Solarflare Communications Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation, incorporated herein by reference.
 */

#include "net_driver.h"
#include "phy.h"
#include "efx.h"
#include "nic.h"
#include "regs.h"
#include "io.h"
#include "workarounds.h"

/* Macros for unpacking the board revision */
/* The revision info is in host byte order. */
#define FALCON_BOARD_TYPE(_rev) (_rev >> 8)
#define FALCON_BOARD_MAJOR(_rev) ((_rev >> 4) & 0xf)
#define FALCON_BOARD_MINOR(_rev) (_rev & 0xf)

/* Board types */
#define FALCON_BOARD_SFE4001 0x01
#define FALCON_BOARD_SFE4002 0x02
#define FALCON_BOARD_SFN4111T 0x51
#define FALCON_BOARD_SFN4112F 0x52

/*****************************************************************************
 * Support for LM87 sensor chip used on several boards
 */
#define LM87_REG_ALARMS1		0x41
#define LM87_REG_ALARMS2		0x42
#define LM87_IN_LIMITS(nr, _min, _max)			\
	0x2B + (nr) * 2, _max, 0x2C + (nr) * 2, _min
#define LM87_AIN_LIMITS(nr, _min, _max)			\
	0x3B + (nr), _max, 0x1A + (nr), _min
#define LM87_TEMP_INT_LIMITS(_min, _max)		\
	0x39, _max, 0x3A, _min
#define LM87_TEMP_EXT1_LIMITS(_min, _max)		\
	0x37, _max, 0x38, _min

#define LM87_ALARM_TEMP_INT		0x10
#define LM87_ALARM_TEMP_EXT1		0x20

static int efx_init_lm87(struct efx_nic *efx, struct i2c_board_info *info,
			 const u8 *reg_values)
{
	struct falcon_board *board = falcon_board(efx);
	struct i2c_client *client = i2c_new_device(&board->i2c_adap, info);
	int rc;

	if (!client)
		return -EIO;

	while (*reg_values) {
		u8 reg = *reg_values++;
		u8 value = *reg_values++;
		rc = i2c_smbus_write_byte_data(client, reg, value);
		if (rc)
			goto err;
	}

	board->hwmon_client = client;
	return 0;

err:
	i2c_unregister_device(client);
	return rc;
}

static void efx_fini_lm87(struct efx_nic *efx)
{
	i2c_unregister_device(falcon_board(efx)->hwmon_client);
}

static int efx_check_lm87(struct efx_nic *efx, unsigned mask)
{
	struct i2c_client *client = falcon_board(efx)->hwmon_client;
	s32 alarms1, alarms2;

	/* If link is up then do not monitor temperature */
	if (EFX_WORKAROUND_7884(efx) && efx->link_state.up)
		return 0;

	alarms1 = i2c_smbus_read_byte_data(client, LM87_REG_ALARMS1);
	alarms2 = i2c_smbus_read_byte_data(client, LM87_REG_ALARMS2);
	if (alarms1 < 0)
		return alarms1;
	if (alarms2 < 0)
		return alarms2;
	alarms1 &= mask;
	alarms2 &= mask >> 8;
	if (alarms1 || alarms2) {
		EFX_ERR(efx,
			"LM87 detected a hardware failure (status %02x:%02x)"
			"%s%s\n",
			alarms1, alarms2,
			(alarms1 & LM87_ALARM_TEMP_INT) ? " INTERNAL" : "",
			(alarms1 & LM87_ALARM_TEMP_EXT1) ? " EXTERNAL" : "");
		return -ERANGE;
	}

	return 0;
}

/*****************************************************************************
 * Support for the SFE4001 and SFN4111T NICs.
 *
 * The SFE4001 does not power-up fully at reset due to its high power
 * consumption.  We control its power via a PCA9539 I/O expander.
 * Both boards have a MAX6647 temperature monitor which we expose to
 * the lm90 driver.
 *
 * This also provides minimal support for reflashing the PHY, which is
 * initiated by resetting it with the FLASH_CFG_1 pin pulled down.
 * On SFE4001 rev A2 and later this is connected to the 3V3X output of
 * the IO-expander; on the SFN4111T it is connected to Falcon's GPIO3.
 * We represent reflash mode as PHY_MODE_SPECIAL and make it mutually
 * exclusive with the network device being open.
 */

/**************************************************************************
 * Support for I2C IO Expander device on SFE4001
 */
#define	PCA9539 0x74

#define	P0_IN 0x00
#define	P0_OUT 0x02
#define	P0_INVERT 0x04
#define	P0_CONFIG 0x06

#define	P0_EN_1V0X_LBN 0
#define	P0_EN_1V0X_WIDTH 1
#define	P0_EN_1V2_LBN 1
#define	P0_EN_1V2_WIDTH 1
#define	P0_EN_2V5_LBN 2
#define	P0_EN_2V5_WIDTH 1
#define	P0_EN_3V3X_LBN 3
#define	P0_EN_3V3X_WIDTH 1
#define	P0_EN_5V_LBN 4
#define	P0_EN_5V_WIDTH 1
#define	P0_SHORTEN_JTAG_LBN 5
#define	P0_SHORTEN_JTAG_WIDTH 1
#define	P0_X_TRST_LBN 6
#define	P0_X_TRST_WIDTH 1
#define	P0_DSP_RESET_LBN 7
#define	P0_DSP_RESET_WIDTH 1

#define	P1_IN 0x01
#define	P1_OUT 0x03
#define	P1_INVERT 0x05
#define	P1_CONFIG 0x07

#define	P1_AFE_PWD_LBN 0
#define	P1_AFE_PWD_WIDTH 1
#define	P1_DSP_PWD25_LBN 1
#define	P1_DSP_PWD25_WIDTH 1
#define	P1_RESERVED_LBN 2
#define	P1_RESERVED_WIDTH 2
#define	P1_SPARE_LBN 4
#define	P1_SPARE_WIDTH 4

/* Temperature Sensor */
#define MAX664X_REG_RSL		0x02
#define MAX664X_REG_WLHO	0x0B

static void sfe4001_poweroff(struct efx_nic *efx)
{
	struct i2c_client *ioexp_client = falcon_board(efx)->ioexp_client;
	struct i2c_client *hwmon_client = falcon_board(efx)->hwmon_client;

	/* Turn off all power rails and disable outputs */
	i2c_smbus_write_byte_data(ioexp_client, P0_OUT, 0xff);
	i2c_smbus_write_byte_data(ioexp_client, P1_CONFIG, 0xff);
	i2c_smbus_write_byte_data(ioexp_client, P0_CONFIG, 0xff);

	/* Clear any over-temperature alert */
	i2c_smbus_read_byte_data(hwmon_client, MAX664X_REG_RSL);
}

static int sfe4001_poweron(struct efx_nic *efx)
{
	struct i2c_client *ioexp_client = falcon_board(efx)->ioexp_client;
	struct i2c_client *hwmon_client = falcon_board(efx)->hwmon_client;
	unsigned int i, j;
	int rc;
	u8 out;

	/* Clear any previous over-temperature alert */
	rc = i2c_smbus_read_byte_data(hwmon_client, MAX664X_REG_RSL);
	if (rc < 0)
		return rc;

	/* Enable port 0 and port 1 outputs on IO expander */
	rc = i2c_smbus_write_byte_data(ioexp_client, P0_CONFIG, 0x00);
	if (rc)
		return rc;
	rc = i2c_smbus_write_byte_data(ioexp_client, P1_CONFIG,
				       0xff & ~(1 << P1_SPARE_LBN));
	if (rc)
		goto fail_on;

	/* If PHY power is on, turn it all off and wait 1 second to
	 * ensure a full reset.
	 */
	rc = i2c_smbus_read_byte_data(ioexp_client, P0_OUT);
	if (rc < 0)
		goto fail_on;
	out = 0xff & ~((0 << P0_EN_1V2_LBN) | (0 << P0_EN_2V5_LBN) |
		       (0 << P0_EN_3V3X_LBN) | (0 << P0_EN_5V_LBN) |
		       (0 << P0_EN_1V0X_LBN));
	if (rc != out) {
		EFX_INFO(efx, "power-cycling PHY\n");
		rc = i2c_smbus_write_byte_data(ioexp_client, P0_OUT, out);
		if (rc)
			goto fail_on;
		schedule_timeout_uninterruptible(HZ);
	}

	for (i = 0; i < 20; ++i) {
		/* Turn on 1.2V, 2.5V, 3.3V and 5V power rails */
		out = 0xff & ~((1 << P0_EN_1V2_LBN) | (1 << P0_EN_2V5_LBN) |
			       (1 << P0_EN_3V3X_LBN) | (1 << P0_EN_5V_LBN) |
			       (1 << P0_X_TRST_LBN));
		if (efx->phy_mode & PHY_MODE_SPECIAL)
			out |= 1 << P0_EN_3V3X_LBN;

		rc = i2c_smbus_write_byte_data(ioexp_client, P0_OUT, out);
		if (rc)
			goto fail_on;
		msleep(10);

		/* Turn on 1V power rail */
		out &= ~(1 << P0_EN_1V0X_LBN);
		rc = i2c_smbus_write_byte_data(ioexp_client, P0_OUT, out);
		if (rc)
			goto fail_on;

		EFX_INFO(efx, "waiting for DSP boot (attempt %d)...\n", i);

		/* In flash config mode, DSP does not turn on AFE, so
		 * just wait 1 second.
		 */
		if (efx->phy_mode & PHY_MODE_SPECIAL) {
			schedule_timeout_uninterruptible(HZ);
			return 0;
		}

		for (j = 0; j < 10; ++j) {
			msleep(100);

			/* Check DSP has asserted AFE power line */
			rc = i2c_smbus_read_byte_data(ioexp_client, P1_IN);
			if (rc < 0)
				goto fail_on;
			if (rc & (1 << P1_AFE_PWD_LBN))
				return 0;
		}
	}

	EFX_INFO(efx, "timed out waiting for DSP boot\n");
	rc = -ETIMEDOUT;
fail_on:
	sfe4001_poweroff(efx);
	return rc;
}

static int sfn4111t_reset(struct efx_nic *efx)
{
	struct falcon_board *board = falcon_board(efx);
	efx_oword_t reg;

	/* GPIO 3 and the GPIO register are shared with I2C, so block that */
	i2c_lock_adapter(&board->i2c_adap);

	/* Pull RST_N (GPIO 2) low then let it up again, setting the
	 * FLASH_CFG_1 strap (GPIO 3) appropriately.  Only change the
	 * output enables; the output levels should always be 0 (low)
	 * and we rely on external pull-ups. */
	efx_reado(efx, &reg, FR_AB_GPIO_CTL);
	EFX_SET_OWORD_FIELD(reg, FRF_AB_GPIO2_OEN, true);
	efx_writeo(efx, &reg, FR_AB_GPIO_CTL);
	msleep(1000);
	EFX_SET_OWORD_FIELD(reg, FRF_AB_GPIO2_OEN, false);
	EFX_SET_OWORD_FIELD(reg, FRF_AB_GPIO3_OEN,
			    !!(efx->phy_mode & PHY_MODE_SPECIAL));
	efx_writeo(efx, &reg, FR_AB_GPIO_CTL);
	msleep(1);

	i2c_unlock_adapter(&board->i2c_adap);

	ssleep(1);
	return 0;
}

static ssize_t show_phy_flash_cfg(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct efx_nic *efx = pci_get_drvdata(to_pci_dev(dev));
	return sprintf(buf, "%d\n", !!(efx->phy_mode & PHY_MODE_SPECIAL));
}

static ssize_t set_phy_flash_cfg(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	struct efx_nic *efx = pci_get_drvdata(to_pci_dev(dev));
	enum efx_phy_mode old_mode, new_mode;
	int err;

	rtnl_lock();
	old_mode = efx->phy_mode;
	if (count == 0 || *buf == '0')
		new_mode = old_mode & ~PHY_MODE_SPECIAL;
	else
		new_mode = PHY_MODE_SPECIAL;
	if (old_mode == new_mode) {
		err = 0;
	} else if (efx->state != STATE_RUNNING || netif_running(efx->net_dev)) {
		err = -EBUSY;
	} else {
		/* Reset the PHY, reconfigure the MAC and enable/disable
		 * MAC stats accordingly. */
		efx->phy_mode = new_mode;
		if (new_mode & PHY_MODE_SPECIAL)
			falcon_stop_nic_stats(efx);
		if (falcon_board(efx)->type->id == FALCON_BOARD_SFE4001)
			err = sfe4001_poweron(efx);
		else
			err = sfn4111t_reset(efx);
		if (!err)
			err = efx_reconfigure_port(efx);
		if (!(new_mode & PHY_MODE_SPECIAL))
			falcon_start_nic_stats(efx);
	}
	rtnl_unlock();

	return err ? err : count;
}

static DEVICE_ATTR(phy_flash_cfg, 0644, show_phy_flash_cfg, set_phy_flash_cfg);

static void sfe4001_fini(struct efx_nic *efx)
{
	struct falcon_board *board = falcon_board(efx);

	EFX_INFO(efx, "%s\n", __func__);

	device_remove_file(&efx->pci_dev->dev, &dev_attr_phy_flash_cfg);
	sfe4001_poweroff(efx);
	i2c_unregister_device(board->ioexp_client);
	i2c_unregister_device(board->hwmon_client);
}

static int sfe4001_check_hw(struct efx_nic *efx)
{
	s32 status;

	/* If XAUI link is up then do not monitor */
	if (EFX_WORKAROUND_7884(efx) && !efx->xmac_poll_required)
		return 0;
	if (efx->phy_mode != PHY_MODE_NORMAL)
		return 0;

	/* Check the powered status of the PHY. Lack of power implies that
	 * the MAX6647 has shut down power to it, probably due to a temp.
	 * alarm. Reading the power status rather than the MAX6647 status
	 * directly because the later is read-to-clear and would thus
	 * start to power up the PHY again when polled, causing us to blip
	 * the power undesirably.
	 * We know we can read from the IO expander because we did
	 * it during power-on. Assume failure now is bad news. */
	status = i2c_smbus_read_byte_data(falcon_board(efx)->ioexp_client, P1_IN);
	if (status >= 0 &&
	    (status & ((1 << P1_AFE_PWD_LBN) | (1 << P1_DSP_PWD25_LBN))) != 0)
		return 0;

	/* Use board power control, not PHY power control */
	sfe4001_poweroff(efx);
	efx->phy_mode = PHY_MODE_OFF;

	return (status < 0) ? -EIO : -ERANGE;
}

static struct i2c_board_info sfe4001_hwmon_info = {
	I2C_BOARD_INFO("max6647", 0x4e),
};

/* This board uses an I2C expander to provider power to the PHY, which needs to
 * be turned on before the PHY can be used.
 * Context: Process context, rtnl lock held
 */
static int sfe4001_init(struct efx_nic *efx)
{
	struct falcon_board *board = falcon_board(efx);
	int rc;

	board->hwmon_client =
		i2c_new_device(&board->i2c_adap, &sfe4001_hwmon_info);
	if (!board->hwmon_client)
		return -EIO;

	/* Raise board/PHY high limit from 85 to 90 degrees Celsius */
	rc = i2c_smbus_write_byte_data(board->hwmon_client,
				       MAX664X_REG_WLHO, 90);
	if (rc)
		goto fail_hwmon;

	board->ioexp_client = i2c_new_dummy(&board->i2c_adap, PCA9539);
	if (!board->ioexp_client) {
		rc = -EIO;
		goto fail_hwmon;
	}

	if (efx->phy_mode & PHY_MODE_SPECIAL) {
		/* PHY won't generate a 156.25 MHz clock and MAC stats fetch
		 * will fail. */
		falcon_stop_nic_stats(efx);
	}
	rc = sfe4001_poweron(efx);
	if (rc)
		goto fail_ioexp;

	rc = device_create_file(&efx->pci_dev->dev, &dev_attr_phy_flash_cfg);
	if (rc)
		goto fail_on;

	EFX_INFO(efx, "PHY is powered on\n");
	return 0;

fail_on:
	sfe4001_poweroff(efx);
fail_ioexp:
	i2c_unregister_device(board->ioexp_client);
fail_hwmon:
	i2c_unregister_device(board->hwmon_client);
	return rc;
}

static int sfn4111t_check_hw(struct efx_nic *efx)
{
	s32 status;

	/* If XAUI link is up then do not monitor */
	if (EFX_WORKAROUND_7884(efx) && !efx->xmac_poll_required)
		return 0;

	/* Test LHIGH, RHIGH, FAULT, EOT and IOT alarms */
	status = i2c_smbus_read_byte_data(falcon_board(efx)->hwmon_client,
					  MAX664X_REG_RSL);
	if (status < 0)
		return -EIO;
	if (status & 0x57)
		return -ERANGE;
	return 0;
}

static void sfn4111t_fini(struct efx_nic *efx)
{
	EFX_INFO(efx, "%s\n", __func__);

	device_remove_file(&efx->pci_dev->dev, &dev_attr_phy_flash_cfg);
	i2c_unregister_device(falcon_board(efx)->hwmon_client);
}

static struct i2c_board_info sfn4111t_a0_hwmon_info = {
	I2C_BOARD_INFO("max6647", 0x4e),
};

static struct i2c_board_info sfn4111t_r5_hwmon_info = {
	I2C_BOARD_INFO("max6646", 0x4d),
};

static void sfn4111t_init_phy(struct efx_nic *efx)
{
	if (!(efx->phy_mode & PHY_MODE_SPECIAL)) {
		if (sft9001_wait_boot(efx) != -EINVAL)
			return;

		efx->phy_mode = PHY_MODE_SPECIAL;
		falcon_stop_nic_stats(efx);
	}

	sfn4111t_reset(efx);
	sft9001_wait_boot(efx);
}

static int sfn4111t_init(struct efx_nic *efx)
{
	struct falcon_board *board = falcon_board(efx);
	int rc;

	board->hwmon_client =
		i2c_new_device(&board->i2c_adap,
			       (board->minor < 5) ?
			       &sfn4111t_a0_hwmon_info :
			       &sfn4111t_r5_hwmon_info);
	if (!board->hwmon_client)
		return -EIO;

	rc = device_create_file(&efx->pci_dev->dev, &dev_attr_phy_flash_cfg);
	if (rc)
		goto fail_hwmon;

	if (efx->phy_mode & PHY_MODE_SPECIAL)
		/* PHY may not generate a 156.25 MHz clock and MAC
		 * stats fetch will fail. */
		falcon_stop_nic_stats(efx);

	return 0;

fail_hwmon:
	i2c_unregister_device(board->hwmon_client);
	return rc;
}

/*****************************************************************************
 * Support for the SFE4002
 *
 */
static u8 sfe4002_lm87_channel = 0x03; /* use AIN not FAN inputs */

static const u8 sfe4002_lm87_regs[] = {
	LM87_IN_LIMITS(0, 0x83, 0x91),		/* 2.5V:  1.8V +/- 5% */
	LM87_IN_LIMITS(1, 0x51, 0x5a),		/* Vccp1: 1.2V +/- 5% */
	LM87_IN_LIMITS(2, 0xb6, 0xca),		/* 3.3V:  3.3V +/- 5% */
	LM87_IN_LIMITS(3, 0xb0, 0xc9),		/* 5V:    4.6-5.2V */
	LM87_IN_LIMITS(4, 0xb0, 0xe0),		/* 12V:   11-14V */
	LM87_IN_LIMITS(5, 0x44, 0x4b),		/* Vccp2: 1.0V +/- 5% */
	LM87_AIN_LIMITS(0, 0xa0, 0xb2),		/* AIN1:  1.66V +/- 5% */
	LM87_AIN_LIMITS(1, 0x91, 0xa1),		/* AIN2:  1.5V +/- 5% */
	LM87_TEMP_INT_LIMITS(10, 60),		/* board */
	LM87_TEMP_EXT1_LIMITS(10, 70),		/* Falcon */
	0
};

static struct i2c_board_info sfe4002_hwmon_info = {
#if !defined(EFX_USE_KCOMPAT) || !defined(EFX_NEED_LM87_DRIVER)
	I2C_BOARD_INFO("lm87", 0x2e),
#else
	I2C_BOARD_INFO("sfc_lm87", 0x2e),
#endif
	.platform_data	= &sfe4002_lm87_channel,
};

/****************************************************************************/
/* LED allocations. Note that on rev A0 boards the schematic and the reality
 * differ: red and green are swapped. Below is the fixed (A1) layout (there
 * are only 3 A0 boards in existence, so no real reason to make this
 * conditional).
 */
#define SFE4002_FAULT_LED (2)	/* Red */
#define SFE4002_RX_LED    (0)	/* Green */
#define SFE4002_TX_LED    (1)	/* Amber */

static void sfe4002_init_phy(struct efx_nic *efx)
{
	/* Set the TX and RX LEDs to reflect status and activity, and the
	 * fault LED off */
	falcon_qt202x_set_led(efx, SFE4002_TX_LED,
			      QUAKE_LED_TXLINK | QUAKE_LED_LINK_ACTSTAT);
	falcon_qt202x_set_led(efx, SFE4002_RX_LED,
			      QUAKE_LED_RXLINK | QUAKE_LED_LINK_ACTSTAT);
	falcon_qt202x_set_led(efx, SFE4002_FAULT_LED, QUAKE_LED_OFF);
}

static void sfe4002_set_id_led(struct efx_nic *efx, enum efx_led_mode mode)
{
	falcon_qt202x_set_led(
		efx, SFE4002_FAULT_LED,
		(mode == EFX_LED_ON) ? QUAKE_LED_ON : QUAKE_LED_OFF);
}

static int sfe4002_check_hw(struct efx_nic *efx)
{
	struct falcon_board *board = falcon_board(efx);

	/* A0 board rev. 4002s report a temperature fault the whole time
	 * (bad sensor) so we mask it out. */
	unsigned alarm_mask =
		(board->major == 0 && board->minor == 0) ?
		~LM87_ALARM_TEMP_EXT1 : ~0;

	return efx_check_lm87(efx, alarm_mask);
}

static int sfe4002_init(struct efx_nic *efx)
{
	return efx_init_lm87(efx, &sfe4002_hwmon_info, sfe4002_lm87_regs);
}

/*****************************************************************************
 * Support for the SFN4112F
 *
 */
static u8 sfn4112f_lm87_channel = 0x03; /* use AIN not FAN inputs */

static const u8 sfn4112f_lm87_regs[] = {
	LM87_IN_LIMITS(0, 0x83, 0x91),		/* 2.5V:  1.8V +/- 5% */
	LM87_IN_LIMITS(1, 0x51, 0x5a),		/* Vccp1: 1.2V +/- 5% */
	LM87_IN_LIMITS(2, 0xb6, 0xca),		/* 3.3V:  3.3V +/- 5% */
	LM87_IN_LIMITS(4, 0xb0, 0xe0),		/* 12V:   11-14V */
	LM87_IN_LIMITS(5, 0x44, 0x4b),		/* Vccp2: 1.0V +/- 5% */
	LM87_AIN_LIMITS(1, 0x91, 0xa1),		/* AIN2:  1.5V +/- 5% */
	LM87_TEMP_INT_LIMITS(10, 60),		/* board */
	LM87_TEMP_EXT1_LIMITS(10, 70),		/* Falcon */
	0
};

static struct i2c_board_info sfn4112f_hwmon_info = {
#if !defined(EFX_USE_KCOMPAT) || !defined(EFX_NEED_LM87_DRIVER)
	I2C_BOARD_INFO("lm87", 0x2e),
#else
	I2C_BOARD_INFO("sfc_lm87", 0x2e),
#endif
	.platform_data	= &sfn4112f_lm87_channel,
};

#define SFN4112F_ACT_LED	0
#define SFN4112F_LINK_LED	1

static void sfn4112f_init_phy(struct efx_nic *efx)
{
	falcon_qt202x_set_led(efx, SFN4112F_ACT_LED,
			      QUAKE_LED_RXLINK | QUAKE_LED_LINK_ACT);
	falcon_qt202x_set_led(efx, SFN4112F_LINK_LED,
			      QUAKE_LED_RXLINK | QUAKE_LED_LINK_STAT);
}

static void sfn4112f_set_id_led(struct efx_nic *efx, enum efx_led_mode mode)
{
	int reg;

	switch (mode) {
	case EFX_LED_OFF:
		reg = QUAKE_LED_OFF;
		break;
	case EFX_LED_ON:
		reg = QUAKE_LED_ON;
		break;
	default:
		reg = QUAKE_LED_RXLINK | QUAKE_LED_LINK_STAT;
		break;
	}

	falcon_qt202x_set_led(efx, SFN4112F_LINK_LED, reg);
}

static int sfn4112f_check_hw(struct efx_nic *efx)
{
	/* Mask out unused sensors */
	return efx_check_lm87(efx, ~0x48);
}

static int sfn4112f_init(struct efx_nic *efx)
{
	return efx_init_lm87(efx, &sfn4112f_hwmon_info, sfn4112f_lm87_regs);
}

static const struct falcon_board_type board_types[] = {
	{
		.id		= FALCON_BOARD_SFE4001,
		.mwatts		= 18000,
		.ref_model	= "SFE4001",
		.gen_type	= "10GBASE-T adapter",
		.init		= sfe4001_init,
		.init_phy	= efx_port_dummy_op_void,
		.fini		= sfe4001_fini,
		.set_id_led	= tenxpress_set_id_led,
		.monitor	= sfe4001_check_hw,
	},
	{
		.id		= FALCON_BOARD_SFE4002,
		.mwatts		= 7500,
		.ref_model	= "SFE4002",
		.gen_type	= "XFP adapter",
		.init		= sfe4002_init,
		.init_phy	= sfe4002_init_phy,
		.fini		= efx_fini_lm87,
		.set_id_led	= sfe4002_set_id_led,
		.monitor	= sfe4002_check_hw,
	},
	{
		.id		= FALCON_BOARD_SFN4111T,
		.mwatts		= 10000,
		.ref_model	= "SFN4111T",
		.gen_type	= "100/1000/10GBASE-T adapter",
		.init		= sfn4111t_init,
		.init_phy	= sfn4111t_init_phy,
		.fini		= sfn4111t_fini,
		.set_id_led	= tenxpress_set_id_led,
		.monitor	= sfn4111t_check_hw,
	},
	{
		.id		= FALCON_BOARD_SFN4112F,
		.mwatts		= 7500,
		.ref_model	= "SFN4112F",
		.gen_type	= "SFP+ adapter",
		.init		= sfn4112f_init,
		.init_phy	= sfn4112f_init_phy,
		.fini		= efx_fini_lm87,
		.set_id_led	= sfn4112f_set_id_led,
		.monitor	= sfn4112f_check_hw,
	},
};

static const struct falcon_board_type falcon_dummy_board = {
	.init		= efx_port_dummy_op_int,
	.init_phy	= efx_port_dummy_op_void,
	.fini		= efx_port_dummy_op_void,
	.set_id_led	= efx_port_dummy_op_set_id_led,
	.monitor	= efx_port_dummy_op_int,
};

void falcon_probe_board(struct efx_nic *efx, u16 revision_info)
{
	struct falcon_board *board = falcon_board(efx);
	u8 type_id = FALCON_BOARD_TYPE(revision_info);
	int i;

	board->major = FALCON_BOARD_MAJOR(revision_info);
	board->minor = FALCON_BOARD_MINOR(revision_info);

	for (i = 0; i < ARRAY_SIZE(board_types); i++)
		if (board_types[i].id == type_id)
			board->type = &board_types[i];

	if (board->type) {
		EFX_INFO(efx, "board is %s rev %c%d\n",
			 (efx->pci_dev->subsystem_vendor == EFX_VENDID_SFC)
			 ? board->type->ref_model : board->type->gen_type,
			 'A' + board->major, board->minor);
	} else {
		EFX_ERR(efx, "unknown board type %d\n", type_id);
		board->type = &falcon_dummy_board;
	}
}
