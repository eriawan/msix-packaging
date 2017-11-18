#pragma once
#define NOMINMAX /* windows.h, or more correctly windef.h, defines min as a macro... */
#include "AppxWindows.hpp"
#include "Exceptions.hpp"
#include "StreamBase.hpp"
#include "ComHelper.hpp"
#include "SHA256.hpp"

#include <string>
#include <map>
#include <functional>
#include <algorithm>

namespace xPlat {
  
    // This represents a subset of a Stream
    class HashStream : public StreamBase
    {
    public:
        HashStream(IStream* stream, const std::vector<std::uint8_t>& expectedHash) :
            m_relativePosition(0)
        {
            ULARGE_INTEGER uli;
            LARGE_INTEGER li;
            std::uint64_t streamSize;
            
            li.QuadPart = 0;
            ThrowHrIfFailed(stream->Seek(li, STREAM_SEEK_END, &uli));
            
            streamSize = uli.u.LowPart;

            ThrowHrIfFailed(stream->Seek(li, STREAM_SEEK_SET, &uli));
            
            m_cacheBuffer.resize(static_cast<std::uint32_t>(streamSize));
            ULONG bytesRead = 0;
            ThrowHrIfFailed(stream->Read(m_cacheBuffer.data(), m_cacheBuffer.size(), &bytesRead));

            ThrowErrorIfNot(xPlat::Error::AppxSignatureInvalid, 
                bytesRead == streamSize, 
                "Invalid signature");
            
            std::vector<std::uint8_t> hash;
            ThrowErrorIfNot(xPlat::Error::AppxSignatureInvalid, 
                xPlat::SHA256::ComputeHash(m_cacheBuffer.data(), m_cacheBuffer.size(), hash), 
                "Invalid signature");

            ThrowErrorIfNot(xPlat::Error::AppxSignatureInvalid, 
                expectedHash.size() == hash.size(), 
                "Signature is corrupt");

            ThrowErrorIfNot(
                xPlat::Error::AppxSignatureInvalid,
                memcmp(expectedHash.data(), hash.data(), hash.size()) == 0,
                "Signature hash doesn't match digest hash"); //TODO: better exception
        }

        HRESULT STDMETHODCALLTYPE Seek(LARGE_INTEGER move, DWORD origin, ULARGE_INTEGER *newPosition) override
        {
            LARGE_INTEGER newPos = { 0 };
            switch (origin)
            {
                case Reference::CURRENT:
                    m_relativePosition += move.u.LowPart;
                    break;
                case Reference::START:
                    m_relativePosition = move.u.LowPart;
                    break;
                case Reference::END:
                    m_relativePosition = m_cacheBuffer.size();
                    break;
            }
            m_relativePosition = std::max((std::uint64_t)0, std::min(m_relativePosition, (std::uint64_t)m_cacheBuffer.size()));
            if (newPosition) { newPosition->QuadPart = (std::uint64_t)m_relativePosition; }
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE Read(void* buffer, ULONG countBytes, ULONG* actualRead) override
        {
            HRESULT hr = static_cast<HRESULT>(Error::Stg_E_Invalidpointer);
            if (buffer)
            {
                ULONG bytesToRead = std::min((std::uint32_t)countBytes, static_cast<std::uint32_t>((std::uint64_t)m_cacheBuffer.size() - m_relativePosition));
                if (bytesToRead)
                {
                    memcpy(buffer, reinterpret_cast<BYTE*>(m_cacheBuffer.data()) + m_relativePosition, bytesToRead);
                }
                m_relativePosition += bytesToRead;
                if (actualRead) { *actualRead = bytesToRead; }                    
                hr = (countBytes == bytesToRead) ? S_OK : S_FALSE;
            }
            return hr;
        }
      
    protected:
        std::vector<std::uint8_t> m_cacheBuffer;
        std::uint64_t m_relativePosition;
    };
}