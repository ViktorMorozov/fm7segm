#include "rda580x.h"
#include "tuner.h"

#include <avr/pgmspace.h>
#include "../i2c.h"

#ifdef _RDS
#include "rds.h"
#endif

static uint8_t wrBuf[14];
static uint8_t rdBuf[12];

static const uint8_t initData[] PROGMEM = {
	/* 0*/ RDA5807_DHIZ | RDA5807_DMUTE,
	/* 1*/ RDA5807_CLK_MODE_32768 | RDA5807_NEW_METHOD | RDA5807_ENABLE,
	/* 2*/ 0,
	/* 3*/ RDA5807_BAND_EASTEUROPE | RDA5807_SPACE_50,
	/* 4*/ 0,
	/* 5*/ 0,
	/* 6*/ 0b1000 & RDA5807_SEEKTH,
	/* 7*/ RDA5807_LNA_PORT_SEL | RDA5807_VOLUME,
	/* 8*/ 0,
	/* 9*/ 0,
	/*10*/ (0x80 & RDA5807_TH_SOFRBLEND),
	/*11*/ 0,
	/*12*/ 0,
	/*13*/ 0,
};

static void rda580xWriteI2C(uint8_t bytes)
{
	uint8_t i;

	if (tuner.ic == TUNER_RDA5802)
		bytes = RDA5802_WR_BYTES;

	I2CStart(RDA5807M_I2C_ADDR);
	for (i = 0; i < bytes; i++)
		I2CWriteByte(wrBuf[i]);
	I2CStop();

	return;
}

void rda580xInit(void)
{
	// TODO: RDS enable/disable by software

	uint8_t i;

	for (i = 0; i < sizeof(wrBuf); i++)
		wrBuf[i] = pgm_read_byte(&initData[i]);

#ifdef _RDS
	wrBuf[1] |= RDA5807_RDS_EN;
#endif
	if (tuner.ic == TUNER_RDA5807_DF)
		wrBuf[11] |= RDA5807_FREQ_MODE;

	return;
}

void rda580xSetFreq(void)
{
	uint16_t chan;
	uint16_t fMin = RDA5807_MIN_FREQ;
	uint8_t band = RDA5807_BAND_EASTEUROPE;

	if (tuner.mono)
		wrBuf[0] |= RDA5807_MONO;
	else
		wrBuf[0] &= ~RDA5807_MONO;

	if (tuner.ic == TUNER_RDA5802)
		fMin = RDA5802_MIN_FREQ;

	if (tuner.freq >= RDA5807_BAND_CHANGE_FREQ) {
		fMin = RDA5807_BAND_CHANGE_FREQ;
		band = RDA5807_BAND_US_EUROPE;
	}

	// Freq in grid
	chan = (tuner.freq - fMin) / RDA5807_CHAN_SPACING;
	wrBuf[2] = chan >> 2;								/* 8 MSB */
	wrBuf[3] = ((chan & 0x03) << 6) | RDA5807_TUNE | band | RDA5807_SPACE_50;

	// Direct freq
	wrBuf[12] = ((tuner.freq - fMin) * 10) >> 8;
	wrBuf[13] = ((tuner.freq - fMin) * 10) & 0xFF;

	rda580xWriteI2C(RDA5807_WR_BYTES);

	return;
}

uint8_t *rda580xReadStatus(void)
{
	uint8_t i;

	I2CStart(RDA5807M_I2C_ADDR | I2C_READ);
	for (i = 0; i < sizeof(rdBuf) - 1; i++)
		rdBuf[i] = I2CReadByte(I2C_ACK);
	rdBuf[sizeof(rdBuf) - 1] = I2CReadByte(I2C_NOACK);
	I2CStop();

	// Get RDS data
#ifdef _RDS
	if (tuner.ic == TUNER_RDA5807 || tuner.ic == TUNER_RDA5807_DF) {
		/* If seek/tune is complete and current channel is a station */
		if ((rdBuf[0] & RDA5807_STC) && (rdBuf[2] & RDA5807_FM_TRUE)) {
			/* If RDS ready and sync flag are set */
			if ((rdBuf[0] & RDA5807_RDSR) && (rdBuf[0] & RDA5807_RDSS)) {
				/* If there are no errors in blocks A and B */
				if (!(rdBuf[3] & (RDA5807_BLERA | RDA5807_BLERB))) {
					/* Send rdBuf[4..11] as 16-bit blocks A-D */
					rdsSetBlocks(&rdBuf[4]);
				}
			}
		}
	}
#endif

	return rdBuf;
}

void rda580xSetAudio(void)
{
	if (tuner.volume > RDA5807_VOL_MAX)
		tuner.volume = RDA5807_VOL_MAX;

	if (tuner.mute || !tuner.volume)
		wrBuf[0] &= ~RDA5807_DMUTE;
	else
		wrBuf[0] |= RDA5807_DMUTE;

	if (tuner.bass)
		wrBuf[0] |= RDA5807_BASS;
	else
		wrBuf[0] &= ~RDA5807_BASS;

	wrBuf[3] &= ~RDA5807_TUNE;

	if (tuner.volume)
		wrBuf[7] = RDA5807_LNA_PORT_SEL | (tuner.volume - 1);

	rda580xWriteI2C(RDA5802_WR_BYTES);
}

void rda580xPowerOn(void)
{
	wrBuf[1] |= RDA5807_ENABLE;

	return;
}

void rda580xPowerOff(void)
{
	wrBuf[1] &= ~RDA5807_ENABLE;

	rda580xWriteI2C(RDA5802_WR_BYTES);

	return;
}
