/*H**************************************************************************
* NAME:         wav.h
*----------------------------------------------------------------------------
* Copyright (c) 2003 Atmel.
*----------------------------------------------------------------------------
* RELEASE:      snd1c-refd-nf-4_0_3      
* REVISION:     1.3     
*----------------------------------------------------------------------------
* PURPOSE
* This file contains the wav file header definition
*
* WAVE FILE FORMAT
* ================
* RIFF Chunk (12 bytes in length total)
* -------------------------------------
* Byte Nb
* 0 - 3     "RIFF" (ASCII Characters)
* 4 - 7     Total Length Of Package To Follow (Binary, little endian)
* 8 - 11    "WAVE" (ASCII Characters)
*
* FORMAT Chunk (24 bytes in length total)
* ---------------------------------------
* Byte Nb
* 0 - 3     "fmt " (ASCII Characters)
* 4 - 7     Length Of FORMAT Chunk (Binary, always 0x10)
* 8 - 9     Always 0x01
* 10 - 11   Channel Numbers (Always 0x01=Mono, 0x02=Stereo)
* 12 - 15   Sample Rate (Binary, in Hz)
* 16 - 19   Bytes Per Second
* 20 - 21   Bytes Per Sample: 1= 8b Mono, 2= 8b Stereo or 16b Mono, 4= 16b Stereo
* 22 - 23   Bits Per Sample
*
* DATA Chunk
* ----------
* Byte Nb
* 0 - 3     "data" (ASCII Characters)
* 4 - 7     Length Of Data To Follow
* 8 - end   Data (Samples)
*
* EXAMPLE
* -------
* 0000  52 49 46 46 46 2D 00 00-57 41 56 45 66 6D 74 20   RIFFF-..WAVEfmt
* 0010  10 00 00 00 01 00 01 00-22 56 00 00 22 56 00 00   ........"V.."V..
* 0020  01 00 08 00 64 61 74 61-22 2D 00 00 80 80 80 80   ....data"-......
* 0030  80 80 80 80 80 80 80 80-80 80 80 80 80 80 80 80   ................
* 0040  80 80 80 80 80 80 80 80-80 80 80 80 80 80 80 80   ................
* As expected, the file begins with the ASCII characters "RIFF" identifying
* it as a WAV file. The next four bytes tell us the length is 0x2D46 bytes
* (11590 bytes in decimal) which is the length of the entire file minus the
* 8 bytes for the "RIFF" and length (11598 - 11590 = 8 bytes).
* The ASCII characters for "WAVE" and "fmt " follow. Next (line 2 above) we
* find the value 0x00000010 in the first 4 bytes (length of format chunk:
* always constant at 0x10). The next four bytes are 0x0001 (Always) and
* 0x0001 (A mono WAV, one channel used). 
* Since this is a 8-bit WAV, the sample rate and the bytes/second are the
* same at 0x00005622 or 22,050 in decimal. For a 16-bit stereo WAV the
* bytes/sec would be 4 times the sample rate. The next 2 bytes show the
* number of bytes per sample to be 0x0001 (8-bit mono) and the number of
* bits per sample to be 0x0008. 
* Finally, the ASCII characters for "data" appear followed by 0x00002D22
* (11,554 decimal) which is the number of bytes of data to follow (actual
* samples). The data is a value from 0x00 to 0xFF. In the example above 0x80
* would represent "0" or silence on the output since the DAC used to playback
* samples is a bipolar device (i.e. a value of 0x00 would output a negative
* voltage and a value of 0xFF would output a positive voltage at the output
* of the DAC on the sound card)
*****************************************************************************/

#ifndef _WAV_H_
#define _WAV_H_

/*_____ I N C L U D E S ____________________________________________________*/


/*_____ M A C R O S ________________________________________________________*/

#define WAV_HEADER_SIZE   sizeof(wav_struct)
#define WAV_SECTOR_SIZE   (Uint32)(WAV_FILE_DURATION * SECT_PER_SECOND)
#define WAV_BYTE_SIZE     (Uint32)((WAV_FILE_DURATION * SECT_PER_SECOND) * SECTOR_SIZE)

#define WAV_HEADER {{{'R','I','F','F'}, LE32((Uint32)36), {'W','A','V','E'}},\
                    {{'f','m','t',' '}, FMT_LENGTH, PCM_FMT, MONO, LE32((Uint32)(1000/SAMPLING_PERIOD)), LE32((Uint32)(1000/SAMPLING_PERIOD)), ONE_BYTE, EIGHT_BIT},\
                    {{'d','a','t','a'}, LE32((Uint32)0)}}

/* RIFF info */
#define RIFF_FIELD        'RIFF'
#define WAVE_FIELD        'WAVE'

/* FMT info */
#define FMT_FIELD         'FMT '
#define FMT_LENGTH        LE32((Uint32)(16))  /* data start beg of sector */
/* wave format */
#define PCM_FMT           ((Uint16)0x0100)
/* channel number */
#define MONO              ((Uint16)0x0100)
#define STEREO            ((Uint16)0x0200)
/* bytes per sample */
#define ONE_BYTE          ((Uint16)0x0100)
#define TWO_BYTE          ((Uint16)0x0200)
/* bits per sample */
#define EIGHT_BIT         ((Uint16)0x0800)
#define SIXTEEN_BIT       ((Uint16)0x1000)
/* DATA info */
#define DATA_FIELD        'data'


/*_____ D E F I N I T I O N ________________________________________________*/

/* WAV Format Structure */

typedef struct
{ /* RIFF info */
  char    riff[4];
  Uint32  pack_length;
  char    wave[4];
} riff_struct;

typedef struct
{ /* FMT info */
  char    fmt[4];
  Uint32  fmt_length;
  Uint16  wav_format; 
  Uint16  channel_nb;
  Uint32  sample_rate;
  Uint32  bytes_per_second;
  Uint16  bytes_per_sample;
  Uint16  bits_per_sample;
} fmt_struct;

typedef struct
{ /* DATA info */
  char    dat[4];
  Uint32  data_length;
} data_struct;

typedef struct
{
  riff_struct   rif_info;
  fmt_struct    fmt_info;
  data_struct   dat_info;
} wav_struct;


/*_____ D E C L A R A T I O N ______________________________________________*/



#endif  /* _WAV_H_ */

