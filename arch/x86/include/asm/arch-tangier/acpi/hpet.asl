/*
 * Copyright (c) 2017 Intel Corporation
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

Device (HPET)
{
	Name (_HID, EisaId ("PNP0103") /* HPET System Timer */)
	Name (_UID, Zero)
	Method (_STA, 0, NotSerialized)
	{
		Return (STA_VISIBLE)
	}

	Method (_CRS, 0, Serialized)
	{
		Name (RBUF, ResourceTemplate ()
		{
			Memory32Fixed(ReadWrite, HPET_BASE_ADDRESS, HPET_BASE_SIZE)
			Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, )
			{
				0x00000008,
			}
		})
		Return (RBUF) /* \_SB_.HPET._CRS.RBUF */
	}
}
