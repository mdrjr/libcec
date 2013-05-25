/*
 * This file is part of the libCEC(R) library.
 *
 * libCEC(R) is Copyright (C) 2011-2013 Pulse-Eight Limited.  All rights reserved.
 * libCEC(R) is an original work, containing original code.
 *
 * libCEC(R) is a trademark of Pulse-Eight Limited.
 *
 * This program is dual-licensed; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *
 * Alternatively, you can license this library under a commercial license,
 * please contact Pulse-Eight Licensing for more information.
 *
 * For more information contact:
 * Pulse-Eight Licensing       <license@pulse-eight.com>
 *     http://www.pulse-eight.com/
 *     http://www.pulse-eight.net/
 */

#include "env.h"

#if defined(HAVE_EXYNOS_API)
#include "ExynosCECAdapterCommunication.h"

#include "lib/CECTypeUtils.h"
#include "lib/LibCEC.h"
#include "lib/platform/sockets/cdevsocket.h"
#include "lib/platform/util/StdString.h"
#include "lib/platform/util/buffer.h"

extern "C" {
#include "libcec.h"
}

using namespace std;
using namespace CEC;
using namespace PLATFORM;

#include "AdapterMessageQueue.h"

#define LIB_CEC m_callback->GetLib()


CExynosCECAdapterCommunication::CExynosCECAdapterCommunication(IAdapterCommunicationCallback *callback) :
    IAdapterCommunication(callback),
    m_bLogicalAddressChanged(false)
{ 
  CLockObject lock(m_mutex);

  m_iNextMessage = 0;
  m_logicalAddresses.Clear();
}


CExynosCECAdapterCommunication::~CExynosCECAdapterCommunication(void)
{
  Close();

  CLockObject lock(m_mutex);
}


bool CExynosCECAdapterCommunication::IsOpen(void)
{
  return IsInitialised();
}

    
bool CExynosCECAdapterCommunication::Open(uint32_t iTimeoutMs, bool UNUSED(bSkipChecks), bool bStartListening)
{
  if (CECOpen())
  {
    if (!bStartListening || CreateThread())
        return true;
  }
  CECClose();
  return false;
}


void CExynosCECAdapterCommunication::Close(void)
{
  StopThread(0);

  CECClose();
}


std::string CExynosCECAdapterCommunication::GetError(void) const
{
  std::string strError(m_strError);
  return strError;
}


cec_adapter_message_state CExynosCECAdapterCommunication::Write(
  const cec_command &data, bool &UNUSED(bRetry), uint8_t UNUSED(iLineTimeout), bool UNUSED(bIsReply))
{
  uint8_t buffer[CEC_MAX_FRAME_SIZE];
  uint32_t size = 1;
  cec_adapter_message_state rc = ADAPTER_MESSAGE_STATE_ERROR;

  if ((size_t)data.parameters.size + data.opcode_set > sizeof(buffer))
  {
    LIB_CEC->AddLog(CEC_LOG_ERROR, "%s: data size too large !", __func__);
    return ADAPTER_MESSAGE_STATE_ERROR;
  }
  
  buffer[0] = (data.initiator << 4) | (data.destination & 0x0f);

  if (data.opcode_set)
  {
    buffer[1] = data.opcode;
    size++;

    memcpy(&buffer[size], data.parameters.data, data.parameters.size);
    size += data.parameters.size;
  }
    

  if (CECSendMessage(buffer, size) != size)
    LIB_CEC->AddLog(CEC_LOG_ERROR, "%s: write failed !", __func__);

  return rc;
}


uint16_t CExynosCECAdapterCommunication::GetFirmwareVersion(void)
{
  return 0;
}


cec_vendor_id CExynosCECAdapterCommunication::GetVendorId(void)
{
  return cec_vendor_id(0);
}


uint16_t CExynosCECAdapterCommunication::GetPhysicalAddress(void)
{  
  return 0x1000;
}


cec_logical_addresses CExynosCECAdapterCommunication::GetLogicalAddresses(void)
{
  CLockObject lock(m_mutex);

  return m_logicalAddresses;
}


bool CExynosCECAdapterCommunication::SetLogicalAddresses(const cec_logical_addresses &addresses)
{
  unsigned int log_addr = addresses.primary;
  
  if (CECSetLogicalAddr(log_addr) == 0)
  {
    LIB_CEC->AddLog(CEC_LOG_ERROR, "%s: CECSetLogicalAddr failed !", __func__);
    return false;
  }
  m_logicalAddresses = addresses;
  m_bLogicalAddressChanged = true;
  
  return true;
}


void CExynosCECAdapterCommunication::HandleLogicalAddressLost(cec_logical_address UNUSED(oldAddress))
{
  if (CECSetLogicalAddr(CEC_MSG_BROADCAST) == 0)
  {
    LIB_CEC->AddLog(CEC_LOG_ERROR, "%s: CECSetLogicalAddr failed !", __func__);
  }
}


void *CExynosCECAdapterCommunication::Process(void)
{
  bool bHandled;
  uint8_t buffer[CEC_MAX_FRAME_SIZE];
  uint32_t size;
  uint32_t opcode, status;
  cec_logical_address initiator, destination;

  while (!IsStopped())
  {
    size = CECReceiveMessage(buffer, CEC_MAX_FRAME_SIZE, 1000000);
    if ( size > 0)
    {
      initiator = cec_logical_address(buffer[0] >> 4);
      destination = cec_logical_address(buffer[0] & 0x0f);
      
        cec_command cmd;

        cec_command::Format(
          cmd, initiator, destination,
          ( size > 3 ) ? cec_opcode(buffer[1]) : CEC_OPCODE_NONE);

        for( uint8_t i = 2; i < size; i++ )
          cmd.parameters.PushBack(buffer[i]);

        if (!IsStopped())
          m_callback->OnCommandReceived(cmd);
    }
  }

  return 0;
}

#endif	// HAVE_EXYNOS_API
