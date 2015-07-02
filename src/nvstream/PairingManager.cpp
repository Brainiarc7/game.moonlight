/*
 *      Copyright (C) 2015 Anthony Miyaguchi
 *      Copyright (C) 2015 Team XBMC
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */
#include "PairingManager.h"
#include "log/Log.h"
#include "NvHTTP.h"
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <cstring>
#include <cstdlib>
#include <openssl/sha.h>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include <openssl/pem.h>
#include <openssl/err.h>

using namespace MOONLIGHT;

PairingManager::PairingManager(NvHTTP* http)
    : m_http(http)
{
  m_cert = NULL;
  m_private_key = NULL;
}

PairState PairingManager::pair(std::string uid, std::string pin)
{
  std::stringstream url;

  std::array<unsigned char, 16> salt;
  RAND_bytes(salt.data(), salt.size());

  AES_KEY aes_key;
  std::array<unsigned char, 20> aes_key_hash;
  std::array<unsigned char, 20> salted_pin;
  // saltPin
  std::copy(salt.begin(), salt.end(), salted_pin.begin());
  std::copy(pin.begin(), pin.end(), salted_pin.begin() + 16);
  // generateAesKey
  SHA1(salted_pin.data(), 20, aes_key_hash.data());
  AES_set_encrypt_key(aes_key_hash.data(), 128, &aes_key);

  // Send the salt and get the server cert.
  url << m_http->baseUrlHttps << "/pair?uniqueid=" << uid
      << "&devicename=roth&updateState=1&phrase=getservercert&salt="
      << bytesToHex(salt.data(), salt.size()) << "&clientcert="
      << bytesToHex(m_cert_bytes.data(), m_cert_bytes.size());
  std::string get_cert = m_http->openHttpConnection(url.str(), false);
  url.str("");

  if (m_http->getXmlString(get_cert, "paired") != "1")
  {
    url << m_http->baseUrlHttps << "/unpair?uniqueid=" << uid;
    m_http->openHttpConnection(url.str(), true);
    return PairState::FAILED;
  }

  // generate challenge data
  std::array<unsigned char, 16> random_challenge;
  std::array<unsigned char, 16> encrypted_challenge;
  RAND_bytes(random_challenge.data(), random_challenge.size());
  AES_encrypt(random_challenge.data(), encrypted_challenge.data(), &aes_key);

  // send the encrypted challenge to the server
  url << m_http->baseUrlHttps << "/pair?uniqueid=" << uid
      << "&devicename=roth&updateState=1&clientchallenge="
      << bytesToHex(encrypted_challenge.data(), encrypted_challenge.size());
  std::string challenge_resp = m_http->openHttpConnection(url.str(), true);
  url.str("");

  if (m_http->getXmlString(challenge_resp, "paired") != "1")
  {
    url << m_http->baseUrlHttps << "/unpair?uniqueid=" << uid;
    m_http->openHttpConnection(url.str(), true);
    return PairState::FAILED;
  }

  // decode the server response and subsequent challenge
  std::vector<unsigned char> enc_challenge_resp = hexToBytes(
      m_http->getXmlString(challenge_resp, "challengeresponse"));
  std::vector<unsigned char> dec_challenge_resp(enc_challenge_resp.size());

  for (int i = 0; i < 48; i += 16)
  {
    AES_decrypt(&enc_challenge_resp[i], &dec_challenge_resp[i], &aes_key);
  }

  // compute the challenge response hash
  std::vector<unsigned char> client_secret(16);
  RAND_bytes(client_secret.data(), client_secret.size());

  std::vector<unsigned char> server_response(dec_challenge_resp.begin(),
      server_response.begin() + 20);

  std::array<unsigned char, 16 + 256 + 16> server_challenge;
  std::array<unsigned char, 32> challenge_resp_hash;
  std::array<unsigned char, 32> challenge_resp_encrypted;
  std::copy_n(dec_challenge_resp.begin() + 20, 16, server_challenge.begin());
  std::memcpy(server_challenge.data() + 16, m_cert->signature->data, 256);
  std::copy(client_secret.begin(), client_secret.end(),
      server_response.begin() + 16 + 256);
  SHA1(server_challenge.data(), server_challenge.size(),
      challenge_resp_hash.data());

  for (int i = 0; i < 32; i += 16)
  {
    AES_encrypt(&challenge_resp_hash.data()[i],
        &challenge_resp_encrypted.data()[i], &aes_key);
  }

  url << m_http->baseUrlHttps << "/pair?uniqueid=" << uid
      << "&devicename=roth&updateState=1&serverchallengeresp="
      << bytesToHex(challenge_resp_encrypted.data(),
          challenge_resp_encrypted.size());
  std::string secret_resp = m_http->openHttpConnection(url.str(), true);
  url.str("");
  if (m_http->getXmlString(secret_resp, "paired") != "1")
  {
    url << m_http->baseUrlHttps << "/unpair?uniqueid=" << uid;
    m_http->openHttpConnection(url.str(), true);
    return PairState::FAILED;
  }

  std::vector<unsigned char> server_secret_resp = hexToBytes(
      m_http->getXmlString(secret_resp, "pairingsecret"));
  // get the servers signed secret
  std::vector<unsigned char> server_secret(16);
  std::copy_n(server_secret_resp.begin(), 16, server_secret.begin());
  std::vector<unsigned char> server_signature(256);
  std::copy_n(server_secret_resp.begin() + 16, 256, server_signature.begin());

  if (!verifySignature(server_secret, server_signature, m_private_key))
  {
    url << m_http->baseUrlHttps << "/unpair?uniqueid=" << uid;
    m_http->openHttpConnection(url.str(), true);
    return PairState::FAILED;
  }

  // ensure the server challenge matched what we expected
  //TODO: concat challenge_data, m_cert->signature->data (256), server_secret
  std::vector<unsigned char> server_challenge_resp_hash;
  if (server_challenge_resp_hash != server_response)
  {
    url << m_http->baseUrlHttps << "/unpair?uniqueid=" << uid;
    m_http->openHttpConnection(url.str(), true);
    return PairState::PIN_WRONG;
  }

  // send the server our signed secret
  std::vector<unsigned char> client_pairing_secret(16 + 256);
  std::copy(client_secret.begin(), client_secret.end(),
      client_pairing_secret.begin());
  std::vector<unsigned char> signed_data = signData(client_secret,
      m_private_key);
  std::copy(signed_data.begin(), signed_data.end(),
      client_pairing_secret.begin() + 16);

  url << m_http->baseUrlHttps << "/pair?uniqueid=" << uid
      << "&devicename=roth&updateState=1&clientpairingsecret="
      << bytesToHex(&client_pairing_secret[0], client_pairing_secret.size());
  std::string client_secret_resp = m_http->openHttpConnection(url.str(), true);
  url.str("");
  if (m_http->getXmlString(client_secret_resp, "paired") != "1")
  {
    url << m_http->baseUrlHttps << "/unpair?uniqueid=" << uid;
    m_http->openHttpConnection(url.str(), true);
    return PairState::FAILED;
  }

  //do initial challenges
  url << m_http->baseUrlHttps << "/pair?uniqueid=" << uid
      << "&devicename=roth&updateState=1&phrase=pairchallenge";
  std::string pair_challenge = m_http->openHttpConnection(url.str(), true);
  url.str("");
  if (m_http->getXmlString(pair_challenge, "paired") != "1")
  {
    url << m_http->baseUrlHttps << "/unpair?uniqueid=" << uid;
    m_http->openHttpConnection(url.str(), true);
    return PairState::FAILED;
  }

  return PairState::PAIRED;
}

PairState PairingManager::getPairState(std::string serverInfo)
{
  return PairState::FAILED;
}

std::string PairingManager::bytesToHex(unsigned char* in, unsigned len)
{
  std::stringstream ss;
  ss << std::hex << std::setfill('0');
  for (int i = 0; i < len; i++)
  {
    ss << std::setw(2) << static_cast<unsigned>(in[i]);
  }
  return ss.str();
}

std::vector<unsigned char> hexToBytes(std::string s)
{
  int len = s.length();
  std::vector<unsigned char> data(len / 2);
  for (int i = 0; i < len; i += 2)
  {
    data[i / 2] = ((s[i] - '0') << 4) | (s[i + 1] - '0');
  }
  return data;
}

// TODO
bool PairingManager::verifySignature(std::vector<unsigned char> data,
    std::vector<unsigned char> signature, EVP_PKEY *pkey)
{
  return true;
}

std::vector<unsigned char> PairingManager::signData(
    std::vector<unsigned char> data, EVP_PKEY *pkey)
{
  std::vector<unsigned char> signature;
  EVP_MD_CTX *ctx = EVP_MD_CTX_create();
  if (ctx == NULL)
  {
    esyslog("EVP_MD_CTX_create failed, error 0x%1x\n", ERR_get_error());
    return signature;
  }

  do
  {
    const EVP_MD *md = EVP_get_digestbyname("SHA256");
    if (md == NULL)
    {
      esyslog("EVP_get_digestbyname failed, error 0x%1x\n", ERR_get_error());
      break;
    }

    int rc = EVP_DigestInit_ex(ctx, md, NULL);
    if (rc != 1)
    {
      esyslog("EVP_DigestInit_ex failed, error 0x%1x\n", ERR_get_error());
      break;
    }

    rc = EVP_DigestSignInit(ctx, NULL, md, NULL, pkey);
    if (rc != 1)
    {
      esyslog("EVP_DigestSignInit failed, error 0x%1x\n", ERR_get_error());
      break;
    }

    rc = EVP_DigestSignUpdate(ctx, data.data(), data.size());
    if (rc != 1)
    {
      esyslog("EVP_DigestSignUpdate failed, error 0x%1x\n", ERR_get_error());
      break;
    }

    size_t req = 0;
    rc = EVP_DigestSignFinal(ctx, NULL, &req);
    if (rc != 1 || !(req > 0))
    {
      esyslog("EVP_DigestSignFinal failed, error 0x%1x\n", ERR_get_error());
      break;
    }

    size_t slen;
    signature.reserve(req);
    rc = EVP_DigestSignFinal(ctx, signature.data(), &slen);
    if (rc != 1)
    {
      esyslog("EVP_MD_CTX_create failed, error 0x%1x\n", ERR_get_error());
      break;
    }

    if (req != slen)
    {
      esyslog(
          "EVP_DigestSignFinal failed, mismatched signature sizes %ld, %ld\n",
          req, slen);
      break;
    }
  } while (false);

  EVP_MD_CTX_destroy(ctx);

  return signature;
}

