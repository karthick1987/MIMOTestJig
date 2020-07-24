#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <errno.h>
#include <string.h>

// NIAPI definitions
#include "LTE_API.h"
#include "lte.h"
#include "pipe.h"
#include "handler.h"
#include "lte_handler.h"
#include "message.h"

int32_t main( int32_t argc, char* argv[] ) {

  //---------------------------------------------------------
  // check for command line parameters
  //---------------------------------------------------------

  printf( "Reading command line parameters\n" );

  uint64_t nPackets   = 0;
  uint32_t prbMaskU25 = 0;  // prb alloc
  uint32_t mcs        = 0;  // mcs

  if (argc < 4) {
    // if not enough parameters, abort
    printf( "ERROR: Not enough command line parameters. Expecting: nPackets | mcs | prbMaskU25 (as hex)\n" );
    return -1;
  } else {
    nPackets   = atoi(argv[1]);             // number of packet received
    mcs        = atoi(argv[2]);             // mcs
    prbMaskU25 = strtol(argv[3], NULL, 16); // prb mask as hex
    printf("Detected arguments - packets: %d, MCS %d, PRB mask: 0x%x\n", nPackets, mcs, prbMaskU25);
  }

  //---------------------------------------------------------
  // initialize named pipes
  //---------------------------------------------------------

  printf( "Preparing named pipes\n" );

  char* pipe_name_1 = (char*)"/tmp/api_transport_0-0_pipe";
  char* pipe_name_2 = (char*)"/tmp/api_transport_0-1_pipe";
  char* pipe_name_3 = (char*)"/tmp/api_transport_0-2_pipe";

  int32_t fd1 = -1;
  int32_t fd2 = -1;
  int32_t fd3 = -1;

  fd_set readFds1;
  int32_t fdMax1 = 0;
  fd_set readFds3;
  int32_t fdMax3 = 0;

  // Assumption: The LV counterpart has created the FIFOs and will also clean them up at the end
  OpenPipeForTx(pipe_name_2, &fd2);
  OpenPipeForRx(pipe_name_1, &fd1, &readFds1, &fdMax1);
  OpenPipeForRx(pipe_name_3, &fd3, &readFds3, &fdMax3);

  //---------------------------------------------------------
  // prepare variables
  //---------------------------------------------------------

  printf( "Preparing variables\n" );

  struct timespec ts;  // main loop wait time
  ts.tv_sec = 0;
  ts.tv_nsec = 10000L;

  // buffer for NIAPI messages
  #define MAX_BUF_U8 10240
  uint8_t*  pBufU8  = (uint8_t*)malloc(MAX_BUF_U8*sizeof(uint8_t));
  memset(pBufU8, 0, MAX_BUF_U8);
  uint32_t bufOffsetU8 = 0;

  int32_t  nread  = 0;
  int32_t  nwrite = 0;

  // NIAPI related parameters
  uint32_t msgType    = 0;
  uint32_t bodyLength = 0;
  uint64_t refId      = 0;
  uint32_t rnti       = 10;
  uint32_t tbs        = 0;          // transport block size
  GetTbs( mcs, prbMaskU25, &tbs );   // calculate tbs based on mcs and number of allocated prb

  // NIAPI messages

  PhyTimingInd phyTimingInd;

  PhyDlTxConfigReq phyDlTxConfigReq;
  InitializePhyDlTxConfigReq(  &phyDlTxConfigReq );
  phyDlTxConfigReq.subMsgHdr.cnfMode                    = 1;    // request confirmations from PHY
  phyDlTxConfigReq.dlschTxConfigBody.rnti               = rnti;
  phyDlTxConfigReq.dlschTxConfigBody.prbAllocation      = prbMaskU25;
  phyDlTxConfigReq.dlschTxConfigBody.mcs                = mcs;
  phyDlTxConfigReq.dciTxConfigDlGrantBody.rnti          = rnti;
  phyDlTxConfigReq.dciTxConfigDlGrantBody.prbAllocation = prbMaskU25;
  phyDlTxConfigReq.dciTxConfigDlGrantBody.mcs           = mcs;

  PhyDlTxPayloadReq phyDlTxPayloadReq;
  InitializePhyDlTxPayloadReq( &phyDlTxPayloadReq );
  phyDlTxPayloadReq.subMsgHdr.cnfMode                   = 1;    // request confirmations from PHY
  phyDlTxPayloadReq.genMsgHdr.bodyLength                = 17+tbs;
  phyDlTxPayloadReq.dlschMacPduTxHdr.parSetBodyLength   = 4+tbs;
  phyDlTxPayloadReq.dlschMacPduTxBody.macPduSize        = tbs;

  memset(phyDlTxPayloadReq.dlschMacPduTxBody.macPdu, 7, phyDlTxPayloadReq.dlschMacPduTxBody.macPduSize);

  PhyCnf phyCnf;

  PhyDlschRxInd phyDlschRxInd;

  // counters for debugging

  uint64_t num_PhyTimingInd      = 0;
  uint64_t num_PhyDlTxConfigReq  = 0;
  uint64_t num_PhyDlTxPayloadReq = 0;
  uint64_t num_PhyCnf            = 0;
  uint64_t num_PhyDlschRxInd     = 0;

  //---------------------------------------------------------
  // main loop
  //---------------------------------------------------------

  printf( "Start main loop\n" );
  fflush( stdout );
  uint64_t sequenceNumber = 0;
  while ( sequenceNumber < nPackets ) {
    //----------------------------------------------------------------
    // read timing indication
    //----------------------------------------------------------------

    nread = PipeRead(&fd1, &readFds1, &fdMax1, pBufU8, 16); // TTI indication = 16 bytes

    if (nread > 0) {
      sequenceNumber++;

      bufOffsetU8 = 0;
      GetMsgType( &msgType, pBufU8, &bufOffsetU8 );

      switch (msgType) {
        // Where timing trigger is received from L1, send config and payload.
        case (PHY_TIMING_IND):
          num_PhyTimingInd++;

          DeserializePhyTimingInd( &phyTimingInd, pBufU8, &bufOffsetU8 );

          //----------------------------------------------------------------
          // send DL TX config
          //----------------------------------------------------------------

          // update some filelds
          phyDlTxConfigReq.genMsgHdr.refId  = refId++;
          phyDlTxConfigReq.subMsgHdr.sfn    = phyTimingInd.subMsgHdr.sfn;
          phyDlTxConfigReq.subMsgHdr.tti    = phyTimingInd.subMsgHdr.tti;
          bufOffsetU8 = 0;
          SerializePhyDlTxConfigReq( &phyDlTxConfigReq, pBufU8, &bufOffsetU8 );
          nwrite = PipeWrite( &fd2, pBufU8, (uint16_t)bufOffsetU8 );
          num_PhyDlTxConfigReq++;

          //----------------------------------------------------------------
          // send DL TX payload
          //----------------------------------------------------------------

          // update some fields
          phyDlTxPayloadReq.genMsgHdr.refId = refId++;
          phyDlTxPayloadReq.subMsgHdr.sfn   = phyTimingInd.subMsgHdr.sfn;
          phyDlTxPayloadReq.subMsgHdr.tti   = phyTimingInd.subMsgHdr.tti;
          bufOffsetU8 = 0;
          SerializePhyDlTxPayloadReq( &phyDlTxPayloadReq, pBufU8, &bufOffsetU8 );
          nwrite = PipeWrite(&fd2, pBufU8, (uint16_t)bufOffsetU8);
          num_PhyDlTxPayloadReq++;

          break;

        default:
          printf( "Received UNKNOWN message. pipe=%s msgType=0x%04x\n", pipe_name_1, msgType );
      }
    }

    //----------------------------------------------------------------
    // read DL RX payload
    //----------------------------------------------------------------

    // read general message header
    nread = PipeRead( &fd3, &readFds3, &fdMax3, pBufU8, 8 ); // genMsgHdr = 8 bytes

    if (nread > 0) {

      // Extract message type and body length (variable)
      bufOffsetU8 = 0;
      GetMsgType( &msgType, pBufU8, &bufOffsetU8 );
      GetBodyLength( &bodyLength, pBufU8, &bufOffsetU8 );

      // Read rest of the message
      if ( bodyLength > PipeRead(&fd3, &readFds3, &fdMax3, pBufU8+8, bodyLength ) ) {
        printf( "ERROR: Could not read requested amount of bytes. pipe=%s, num_requested=%i, num_received=%i\n", pipe_name_3, bodyLength, nread );
      }

      switch (msgType) {
        case (PHY_DLSCH_RX_IND):
          num_PhyDlschRxInd++;
          DeserializePhyDlschRxInd( &phyDlschRxInd, pBufU8, &bufOffsetU8 );
          break;

        case (PHY_CNF):
          num_PhyCnf++;
          DeserializePhyCnf( &phyCnf, pBufU8, &bufOffsetU8 );
          break;

        default:
          printf( "Received UNKNOWN message. pipe=%s msgType=0x%04x\n", pipe_name_3, msgType );
          break;
      }
    }

    nanosleep( &ts, NULL );   // wait a little bit and then start over again

  }

  free( pBufU8 );

  //---------------------------------------------------------

  printf( "All done\n" );
  printf( "num_PhyTimingInd      = %i\n", num_PhyTimingInd );
  printf( "num_PhyDlTxConfigReq  = %i\n", num_PhyDlTxConfigReq );
  printf( "num_PhyDlTxPayloadReq = %i\n", num_PhyDlTxPayloadReq );
  printf( "num_PhyCnf            = %i\n", num_PhyCnf );
  printf( "num_PhyDlschRxInd     = %i\n", num_PhyDlschRxInd );

  return 0;
}
