// **************************************************************************
// **************************************************************************
//
// dmf2pce.cpp : Examine/Convert DefleMask files for use on a PC Engine.
//
// **************************************************************************
// **************************************************************************
//
// Copyright (c) 2016 John Brandwood
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
//
// (https://opensource.org/licenses/MIT)
//
// **************************************************************************
// **************************************************************************
//
// http://www.deflemask.com/forum/general/dmf-specs/
//
// Current:
//
//  http://www.deflemask.com/DMF_SPECS.txt
//  http://www.deflemask.com/DMP_SPECS.txt
//
// Previous:
//
//  http://www.delek.com.ar/soft/deflemask/DMF_SPECS_0x11.txt
//  http://www.delek.com.ar/soft/deflemask/DMF_SPECS_0x12.txt
//  http://www.delek.com.ar/soft/deflemask/DMF_SPECS_0x13.txt
//  http://www.delek.com.ar/soft/deflemask/DMF_SPECS_0x15.txt
//
// **************************************************************************
// **************************************************************************
//
// Misty Blue - Opening - Turbografx 16/PC engine arrange [Deflemask]
// by Michirin9801
// https://www.youtube.com/watch?v=SgziA7yvTmc
//
// Noroi no Fuuin - Bloody Tears (PC-Engine/Turbografx-16, DefleMask)
// by JIR-O
// https://www.youtube.com/watch?v=GIvxiusv1v8
//
// **************************************************************************
// **************************************************************************

#ifdef _WIN32
 #include "targetver.h"
 #define   WIN32_LEAN_AND_MEAN
 #include  <windows.h>
#endif

//
// Standard C99 includes, with Visual Studio workarounds.
//

#include <stddef.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>

#include <io.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#if defined (_MSC_VER) && (_MSC_VER < 1600) // i.e. before VS2010
  #define __STDC_LIMIT_MACROS
  #include "msinttypes/stdint.h"
#else
  #include <stdint.h>
#endif

#if defined (_MSC_VER) && (_MSC_VER < 1800) // i.e. before VS2013
  #define static_assert( test, message )
  #include "msinttypes/inttypes.h"
  #include "msinttypes/stdbool.h"
#else
  #include <inttypes.h>
  #include <stdbool.h>
#endif

#if defined (_MSC_VER)
  #define SSIZE_MAX UINT_MAX
  #define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
  #define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

//
//
//

#include "zlib/zlib.h"

#define ERRORCODE int

//
//
//

g_aDmfSampleRateHz [] =
  { -1, 8000, 11025, 16000, 22050, 32000, -1, -1 };





// **************************************************************************
//
// ReadBinaryFile ()
//
// Uses POSIX file functions rather than C file functions.
//
// Google FIO19-C for the reason why.
//
// N.B. Will return an error for files larger than 2GB on a 32-bit system.
//

bool ReadBinaryFile ( const char * pName, uint8_t ** pBuffer, size_t * pLength )
{
  int         hFile = 0;
  uint8_t *   pData = NULL;
  off_t       uSize;
  struct stat cStat;

  hFile = open( pName, O_BINARY | O_RDONLY );

  if (hFile == -1)
    goto errorExit;

  if ((fstat( hFile, &cStat ) != 0) || (!S_ISREG( cStat.st_mode )))
    goto errorExit;

  if (cStat.st_size > SSIZE_MAX)
    goto errorExit;

  uSize = cStat.st_size;

  pData = (uint8_t *) malloc( uSize );

  if (pData == NULL)
    goto errorExit;

  if (read( hFile, pData, uSize ) != uSize)
    goto errorExit;

  close( hFile );

  *pBuffer = pData;
  *pLength = uSize;

  return true;

  // Handle errors.

  errorExit:

    if (pData != NULL) free( pData );
    if (hFile >= 0) close( hFile );

    *pBuffer = NULL;
    *pLength = 0;

    return false;
}



// **************************************************************************
//
// ReadDMF ()
//

bool ReadDMF ( const char * pName, uint8_t ** pBuffer, size_t * pLength )
{
  uint8_t * pFileData = NULL;
  size_t    uFileSize = 0;

  // Read in the compressed DMF file data.

  if (!ReadBinaryFile( pName, &pFileData, &uFileSize ))
  {
    printf( "Read failed!\n" );
    goto errorExit;
  }

  // Allow for a 4MB uncompressed DMF maximum.

  *pBuffer = malloc( (*pLength = 4 * 1024 * 1024) );

  // Decompress the DMF data into the new buffer.

  if (uncompress( *pBuffer, pLength, pFileData, uFileSize ) != Z_OK)
  {
    printf( "inflate returned error!\n" );
    goto errorExit;
  }

  // Shrink the DMF buffer to the actual size of the data.

  *pBuffer = realloc( *pBuffer, *pLength );

  //

  return true;

  //

  errorExit:

    return false;
}



// **************************************************************************
//
// DumpDMF ()
//

#define MAX_CHANNELS   8
#define MAX_MTX_ROWS 256

typedef struct _DMF_INFO
{
  int       m_iVersion;
  int       m_iSystem;
  int       m_iNumChannels;
  int       m_iBaseTime;
  int       m_iTickTime1;
  int       m_iTickTime2;
  int       m_fFrameMode;
  int       m_fCustomHz;
  int       m_iCustomHz;
  int       m_iRowsInPattern;
  int       m_iRowsInMatrix;
  int       m_iArpTicks;        // Removed since v11.1
  int       m_iNumInstruments;
  int       m_iNumWavetables;
  int       m_iNumSamples;
  uint8_t * m_aMatrixData     [MAX_CHANNELS];
  uint8_t * m_pInstrVolume    [255];
  uint8_t * m_pInstrArpeggio  [255];
  uint8_t * m_pInstrDutyNoise [255];
  uint8_t * m_pInstrWaveTable [255];
  uint8_t * m_pWaveTable      [255];
  int       m_aEffectColumns  [MAX_CHANNELS];
  uint8_t * m_pPatternData    [MAX_CHANNELS][MAX_MTX_ROWS];
  uint8_t * m_pSampleData     [255];

} DMF_INFO;

DMF_INFO  g_DmfInfo;

int g_iVerbose = 100;

bool DumpDMF( DMF_INFO * pDmfInfo, uint8_t * pDmfData, size_t uDmfSize )
{
  uint8_t * pBuffer = pDmfData;

  int iChannel;
  int iRow;
  int iLine;
  int i;

  // Check that it's a DefleMask file.

  if (memcmp( pDmfData, ".DelekDefleMask.", 16 ) != 0)
  {
    printf( "It's not a DefleMask file!\n" );
    goto errorExit;
  }

  pBuffer += 16;

  // Check version and system.

  pDmfInfo->m_iVersion = pBuffer[0];

  switch (pDmfInfo->m_iVersion)
  {
    case 19:
    case 21:
    case 22:
    case 24:
      printf( "DefleMask format %d file.\n\n", pDmfInfo->m_iVersion );
      break;
    default:
      printf( "It's not a DefleMask v0.12.0 file, it's version %d!\n", pBuffer[0] );
      goto errorExit;
  }

  pDmfInfo->m_iSystem = pBuffer[1];

  switch (pDmfInfo->m_iSystem)
  {
    case 0x02: // SYSTEM_GENESIS
      pDmfInfo->m_iNumChannels = 10;
      break;
    case 0x12: // SYSTEM_GENESIS
      pDmfInfo->m_iNumChannels = 13;
      break;
    case 0x03: // SYSTEM_SMS
      pDmfInfo->m_iNumChannels =  4;
      break;
    case 0x04: // SYSTEM_GAMEBOY
      pDmfInfo->m_iNumChannels =  4;
      break;
    case 0x05: // SYSTEM_PCENGINE
      pDmfInfo->m_iNumChannels =  6;
      break;
    case 0x06: // SYSTEM_NES
      pDmfInfo->m_iNumChannels =  5;
      break;
    case 0x07: // SYSTEM_C64
      pDmfInfo->m_iNumChannels =  3;
      break;
    case 0x17: // SYSTEM_C64
      pDmfInfo->m_iNumChannels =  3;
      break;
    case 0x08: // SYSTEM_YM2151
      pDmfInfo->m_iNumChannels = 13;
      break;
    default:
      pDmfInfo->m_iNumChannels =  0;
  }

  if (pDmfInfo->m_iSystem != 0x05)
  {
    printf( "It's not a PC Engine DefleMask, we're not interested!\n" );
    goto errorExit;
  }

  pBuffer += 2;

  // Display song and author name.

  printf( "Name   : %.*s\n", (int) pBuffer[0], pBuffer+1 );
  pBuffer += 1 + pBuffer[0];

  printf( "Author : %.*s\n", (int) pBuffer[0], pBuffer+1 );
  pBuffer += 1 + pBuffer[0];

  // Skip cursor highlight position.

  pBuffer += 2;

  // Read the global information block.

  pDmfInfo->m_iBaseTime       = pBuffer[0x00];
  pDmfInfo->m_iTickTime1      = pBuffer[0x01];
  pDmfInfo->m_iTickTime2      = pBuffer[0x02];
  pDmfInfo->m_fFrameMode      = pBuffer[0x03];
  pDmfInfo->m_fCustomHz       = pBuffer[0x04];
  pDmfInfo->m_iCustomHz       = 0;

  if (pDmfInfo->m_fCustomHz)
  {
    for (i = 0; i < 3; ++i)
    {
      if (pBuffer[0x05+i] == 0) break;
      pDmfInfo->m_iCustomHz =
        (pDmfInfo->m_iCustomHz * 10) + (pBuffer[0x05+i] - '0');
    }
  }

  if (pDmfInfo->m_iVersion < 24)
  {
    pDmfInfo->m_iRowsInPattern  = pBuffer[0x08];
    pDmfInfo->m_iRowsInMatrix   = pBuffer[0x09];
    pBuffer += 0x0A;
  }
  else
  {
    pDmfInfo->m_iRowsInPattern  = pBuffer[0x08];
    pDmfInfo->m_iRowsInPattern += (((int) pBuffer[0x09]) <<  8);
    pDmfInfo->m_iRowsInPattern += (((int) pBuffer[0x0A]) << 16);
    pDmfInfo->m_iRowsInPattern += (((int) pBuffer[0x0B]) << 24);
    pDmfInfo->m_iRowsInMatrix   = pBuffer[0x0C];
    pBuffer += 0x0D;
  }

  if (pDmfInfo->m_iVersion < 21)
  {
    pDmfInfo->m_iArpTicks   = pBuffer[0x0];
    pBuffer += 1;
  }

  if (g_iVerbose > 0)
  {
    printf( "BaseTime        : %d\n", pDmfInfo->m_iBaseTime );
    printf( "TickTime1       : %d\n", pDmfInfo->m_iTickTime1 );
    printf( "TickTime2       : %d\n", pDmfInfo->m_iTickTime2 );
    printf( "FrameMode       : %s\n", pDmfInfo->m_fFrameMode ? "NTSC" : "PAL" );
    printf( "CustomHz?       : %s\n", pDmfInfo->m_fCustomHz ? "Yes" : "No" );

    if (pDmfInfo->m_fCustomHz)
      printf( "CustomHz        : %d\n", pDmfInfo->m_iCustomHz );

    printf( "Rows in Pattern : %d\n", pDmfInfo->m_iRowsInPattern );
    printf( "Rows in Matrix  : %d\n", pDmfInfo->m_iRowsInMatrix );
  }

  // Read the pattern matrix.

  if (g_iVerbose > 1)
    printf( "\n[Pattern Matrix]\n" );

  for (iChannel = 0; iChannel < pDmfInfo->m_iNumChannels; ++iChannel)
  {
    pDmfInfo->m_aMatrixData[iChannel] = pBuffer;

    if (g_iVerbose > 1)
    {
      printf( "Ch%d : ", iChannel+1 );
      for (iRow = 0; iRow < pDmfInfo->m_iRowsInMatrix; ++iRow)
        printf( "%2d ", (int) pBuffer[iRow] );
      printf( "\n" );
    }

    pBuffer += pDmfInfo->m_iRowsInMatrix;
  }

  // Read the Instrument Data.

  pDmfInfo->m_iNumInstruments = pBuffer[0];
  pBuffer += 1;

  if (g_iVerbose > 1)
    printf( "\n[Instruments]\n" );

  for (iRow = 0; iRow < pDmfInfo->m_iNumInstruments; ++iRow)
  {
    if (g_iVerbose > 1)
    {
      printf( "Instrument %2X : %.*s\n", iRow, (int) pBuffer[0], pBuffer+1 );
    }
    pBuffer += 1 + pBuffer[0];

    if (pBuffer[0] != 0)
    {
      printf( "Cannot handle FM instruments, aborting!\n" );
      goto errorExit;
    }
    pBuffer += 1;

    // Volume macro.

    if (pDmfInfo->m_iSystem != 0x04) // SYSTEM_GAMEBOY
    {
      pDmfInfo->m_pInstrVolume[iRow] = pBuffer;

      if (pBuffer[0] > 0)
      {
        pBuffer += (4 * pBuffer[0]) + 1;
      }
      pBuffer += 1;
    }

    // Arpeggio macro.

    pDmfInfo->m_pInstrArpeggio[iRow] = pBuffer;

    if (pBuffer[0] > 0)
    {
      pBuffer += (4 * pBuffer[0]) + 1;
    }
    pBuffer += 2;

    // Duty/Noise macro.

    pDmfInfo->m_pInstrDutyNoise[iRow] = pBuffer;

    if (pBuffer[0] > 0)
    {
      pBuffer += (4 * pBuffer[0]) + 1;
    }
    pBuffer += 1;

    // Wavetable macro.

    pDmfInfo->m_pInstrWaveTable[iRow] = pBuffer;

    if (pBuffer[0] > 0)
    {
      pBuffer += (4 * pBuffer[0]) + 1;
    }
    pBuffer += 1;

    // System-specific data.

    switch (pDmfInfo->m_iSystem)
    {
      case 0x04: // SYSTEM_GAMEBOY
        pBuffer += 4;
        break;
      case 0x07: // SYSTEM_C64
      case 0x17: // SYSTEM_C64
        pBuffer += 19;
        break;
    }
  }

  // Read the Wavetable Data.

  if (g_iVerbose > 1)
    printf( "\n[Wavetables]\n" );

  pDmfInfo->m_iNumWavetables = pBuffer[0];
  pBuffer += 1;

  if (g_iVerbose > 1)
  {
    printf( "Wavetables : %d\n", pDmfInfo->m_iNumWavetables );
  }

  for (iRow = 0; iRow < pDmfInfo->m_iNumWavetables; ++iRow)
  {
    int iSize;

    pDmfInfo->m_pWaveTable[iRow] = pBuffer;

    iSize  = pBuffer[0];
    iSize += (((int) pBuffer[1]) <<  8);
    iSize += (((int) pBuffer[2]) << 16);
    iSize += (((int) pBuffer[3]) << 24);
    pBuffer += 4 + (4 * iSize);
  }

  // Read the Pattern Data.

  if (g_iVerbose > 1)
    printf( "\n[Patterns]\n" );

  for (iChannel = 0; iChannel < pDmfInfo->m_iNumChannels; ++iChannel)
  {
    pDmfInfo->m_aEffectColumns[iChannel] = pBuffer[0];
    pBuffer += 1;

    if (g_iVerbose > 1)
      printf( "Ch%d : %d effect columns\n", iChannel+1, pDmfInfo->m_aEffectColumns[iChannel] );

    for (iRow = 0; iRow < pDmfInfo->m_iRowsInMatrix; ++iRow)
    {
      pDmfInfo->m_pPatternData[iChannel][iRow] = pBuffer;

      for (iLine = 0; iLine < pDmfInfo->m_iRowsInPattern; ++iLine)
      {
        pBuffer += (6 + (pDmfInfo->m_aEffectColumns[iChannel] * 4) + 2);
      }
    }
  }

  // Read the Sample Data.

  if (g_iVerbose > 1)
    printf( "\n[Samples]\n" );

  pDmfInfo->m_iNumSamples = pBuffer[0];
  pBuffer += 1;

  if (pDmfInfo->m_iNumSamples == 0)
  {
    printf( "None, good job! :-)\n" );
  }

  for (iRow = 0; iRow < pDmfInfo->m_iNumSamples; ++iRow)
  {
    int iSize;

    pDmfInfo->m_pSampleData[iRow] = pBuffer;

    iSize  = pBuffer[0];
    iSize += (((int) pBuffer[1]) <<  8);
    iSize += (((int) pBuffer[2]) << 16);
    iSize += (((int) pBuffer[3]) << 24);
    pBuffer += 4;

    if ((g_iVerbose > 0) && (iRow != 0))
      printf( "\n" );

    if (pDmfInfo->m_iVersion < 24)
    {
      if (g_iVerbose > 0)
      {
        printf( "Sample %2d :\n", iRow+1 );
      }
    }
    else
    {
      if (g_iVerbose > 0)
      {
        printf( "Sample %2d : %.*s\n", iRow+1, (int) pBuffer[0], pBuffer+1 );
      }
      pBuffer += 1 + pBuffer[0];
    }

    if (g_iVerbose > 1)
    {
      printf( "Size      - %5d samples\n",   (int) iSize );
      printf( "Rate      - %5d Hz\n", g_aDmfSampleRateHz[pBuffer[0] & 7] );
      printf( "Pitch     - %5d\n",   (int) pBuffer[1] );
      printf( "Amp       - %5d\n",   (int) pBuffer[2] );
    }
    pBuffer += 3;

    if (pDmfInfo->m_iVersion < 22)
    {
      if (g_iVerbose > 1)
      {
        printf( "Bits      -    16\n" );
      }
    }
    else
    {
      if (g_iVerbose > 1)
      {
        printf( "Bits      - %5d\n",   (int) pBuffer[0] );
      }
      pBuffer += 1;
    }

    pBuffer += iSize * 2;
  }

  // The next byte should be zero (for expansion?).

  if (pDmfInfo->m_iVersion >= 24)
  {
    if (pBuffer[0] != 0)
    {
      printf( "Unexpected data at the end of the DMF file!\n" );
      goto errorExit;
    }
  }

  pBuffer += 1;

  // Do a sanity check on the buffer pointer.

  if (pBuffer != (pDmfData + uDmfSize))
  {
    printf( "Parsing error at the end of the DMF file!\n" );
    goto errorExit;
  }

  //

  return true;

  errorExit:

    return false;
}



// **************************************************************************
//
// main ()
//

int main(int argc, char* argv[])
{
  size_t    uDmfSize;
  uint8_t * pDmfData;

  //
  // Read in the compressed DMF file data.
  //

  pDmfData = NULL;
  uDmfSize = 0;

  if (!ReadDMF( argv[1], &pDmfData, &uDmfSize ))
  {
    printf( "Read failed!\n" );
    goto errorExit;
  }

  //
  // Locate the data within the DMF file.
  //

  if (!DumpDMF( &g_DmfInfo, pDmfData, uDmfSize ))
  {
    printf( "Read failed!\n" );
    goto errorExit;
  }

  //
  // That's all, for now.
  //

  printf( "\nThat's all, folks!\n" );

  return 0;

  //

  errorExit:

    return 1;
}
