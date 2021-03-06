// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/install_signer.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "crypto/random.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "crypto/signature_verifier.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

#if defined(ENABLE_RLZ)
#include "rlz/lib/machine_id.h"
#endif

namespace {

using extensions::ExtensionIdSet;

const char kExpireDateKey[] = "expire_date";
const char kExpiryKey[] = "expiry";
const char kHashKey[] = "hash";
const char kIdsKey[] = "ids";
const char kInvalidIdsKey[] = "invalid_ids";
const char kProtocolVersionKey[] = "protocol_version";
const char kSaltKey[] = "salt";
const char kSignatureKey[] = "signature";

const size_t kSaltBytes = 32;

const char kBackendUrl[] =
    "https://www.googleapis.com/chromewebstore/v1.1/items/verify";

const char kPublicKeyPEM[] =                                            \
    "-----BEGIN PUBLIC KEY-----"                                        \
    "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAj/u/XDdjlDyw7gHEtaaa"  \
    "sZ9GdG8WOKAyJzXd8HFrDtz2Jcuy7er7MtWvHgNDA0bwpznbI5YdZeV4UfCEsA4S"  \
    "rA5b3MnWTHwA1bgbiDM+L9rrqvcadcKuOlTeN48Q0ijmhHlNFbTzvT9W0zw/GKv8"  \
    "LgXAHggxtmHQ/Z9PP2QNF5O8rUHHSL4AJ6hNcEKSBVSmbbjeVm4gSXDuED5r0nwx"  \
    "vRtupDxGYp8IZpP5KlExqNu1nbkPc+igCTIB6XsqijagzxewUHCdovmkb2JNtskx"  \
    "/PMIEv+TvWIx2BzqGp71gSh/dV7SJ3rClvWd2xj8dtxG8FfAWDTIIi0qZXWn2Qhi"  \
    "zQIDAQAB"                                                          \
    "-----END PUBLIC KEY-----";

GURL GetBackendUrl() {
  return GURL(kBackendUrl);
}

// Hashes |salt| with the machine id, base64-encodes it and returns it in
// |result|.
bool HashWithMachineId(const std::string& salt, std::string* result) {
  std::string machine_id;
#if defined(ENABLE_RLZ)
  if (!rlz_lib::GetMachineId(&machine_id))
    return false;
#else
  machine_id = "unknown";
#endif

  scoped_ptr<crypto::SecureHash> hash(
      crypto::SecureHash::Create(crypto::SecureHash::SHA256));

  hash->Update(machine_id.data(), machine_id.size());
  hash->Update(salt.data(), salt.size());

  std::string result_bytes(crypto::kSHA256Length, 0);
  hash->Finish(string_as_array(&result_bytes), result_bytes.size());

  base::Base64Encode(result_bytes, result);
  return true;
}

// Validates that |input| is a string of the form "YYYY-MM-DD".
bool ValidateExpireDateFormat(const std::string& input) {
  if (input.length() != 10)
    return false;
  for (int i = 0; i < 10; i++) {
    if (i == 4 ||  i == 7) {
      if (input[i] != '-')
        return false;
    } else if (!IsAsciiDigit(input[i])) {
      return false;
    }
  }
  return true;
}

}  // namespace

namespace extensions {

InstallSignature::InstallSignature() {
}
InstallSignature::~InstallSignature() {
}

void InstallSignature::ToValue(base::DictionaryValue* value) const {
  CHECK(value);

  base::ListValue* id_list = new base::ListValue();
  for (ExtensionIdSet::const_iterator i = ids.begin(); i != ids.end();
       ++i)
    id_list->AppendString(*i);

  value->Set(kIdsKey, id_list);
  value->SetString(kExpireDateKey, expire_date);
  std::string salt_base64;
  std::string signature_base64;
  base::Base64Encode(salt, &salt_base64);
  base::Base64Encode(signature, &signature_base64);
  value->SetString(kSaltKey, salt_base64);
  value->SetString(kSignatureKey, signature_base64);
}

// static
scoped_ptr<InstallSignature> InstallSignature::FromValue(
    const base::DictionaryValue& value) {

  scoped_ptr<InstallSignature> result(new InstallSignature);

  std::string salt_base64;
  std::string signature_base64;
  if (!value.GetString(kExpireDateKey, &result->expire_date) ||
      !value.GetString(kSaltKey, &salt_base64) ||
      !value.GetString(kSignatureKey, &signature_base64) ||
      !base::Base64Decode(salt_base64, &result->salt) ||
      !base::Base64Decode(signature_base64, &result->signature)) {
    result.reset();
    return result.Pass();
  }

  const base::ListValue* ids = NULL;
  if (!value.GetList(kIdsKey, &ids)) {
    result.reset();
    return result.Pass();
  }

  for (base::ListValue::const_iterator i = ids->begin(); i != ids->end(); ++i) {
    std::string id;
    if (!(*i)->GetAsString(&id)) {
      result.reset();
      return result.Pass();
    }
    result->ids.insert(id);
  }

  return result.Pass();
}


InstallSigner::InstallSigner(net::URLRequestContextGetter* context_getter,
                             const ExtensionIdSet& ids)
    : ids_(ids), context_getter_(context_getter) {
}

InstallSigner::~InstallSigner() {
}

// static
bool InstallSigner::VerifySignature(const InstallSignature& signature) {
  if (signature.ids.empty())
    return true;

  std::string signed_data;
  for (ExtensionIdSet::const_iterator i = signature.ids.begin();
       i != signature.ids.end(); ++i)
    signed_data.append(*i);

  std::string hash_base64;
  if (!HashWithMachineId(signature.salt, &hash_base64))
    return false;
  signed_data.append(hash_base64);

  signed_data.append(signature.expire_date);

  std::string public_key;
  if (!Extension::ParsePEMKeyBytes(kPublicKeyPEM, &public_key))
    return false;

  crypto::SignatureVerifier verifier;
  if (!verifier.VerifyInit(extension_misc::kSignatureAlgorithm,
                           sizeof(extension_misc::kSignatureAlgorithm),
                           reinterpret_cast<const uint8*>(
                               signature.signature.data()),
                           signature.signature.size(),
                           reinterpret_cast<const uint8*>(public_key.data()),
                           public_key.size()))
    return false;

  verifier.VerifyUpdate(reinterpret_cast<const uint8*>(signed_data.data()),
                        signed_data.size());
  return verifier.VerifyFinal();
}


class InstallSigner::FetcherDelegate : public net::URLFetcherDelegate {
 public:
  explicit FetcherDelegate(const base::Closure& callback)
      : callback_(callback) {
  }

  virtual ~FetcherDelegate() {
  }

  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE {
    callback_.Run();
  }

 private:
  base::Closure callback_;
  DISALLOW_COPY_AND_ASSIGN(FetcherDelegate);
};

// static
ExtensionIdSet InstallSigner::GetForcedNotFromWebstore() {
  std::string value = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kExtensionsNotWebstore);
  if (value.empty())
    return ExtensionIdSet();

  std::vector<std::string> ids;
  base::SplitString(value, ',', &ids);
  return ExtensionIdSet(ids.begin(), ids.end());
}

void InstallSigner::GetSignature(const SignatureCallback& callback) {
  CHECK(!url_fetcher_.get());
  CHECK(callback_.is_null());
  CHECK(salt_.empty());
  callback_ = callback;

  // If the set of ids is empty, just return an empty signature and skip the
  // call to the server.
  if (ids_.empty()) {
    if (!callback_.is_null())
      callback_.Run(scoped_ptr<InstallSignature>(new InstallSignature()));
    return;
  }

  salt_ = std::string(kSaltBytes, 0);
  DCHECK_EQ(kSaltBytes, salt_.size());
  crypto::RandBytes(string_as_array(&salt_), salt_.size());

  std::string hash_base64;
  if (!HashWithMachineId(salt_, &hash_base64)) {
    ReportErrorViaCallback();
    return;
  }

  if (!context_getter_) {
    ReportErrorViaCallback();
    return;
  }

  base::Closure closure = base::Bind(&InstallSigner::ParseFetchResponse,
                                     base::Unretained(this));

  delegate_.reset(new FetcherDelegate(closure));
  url_fetcher_.reset(net::URLFetcher::Create(
      GetBackendUrl(), net::URLFetcher::POST, delegate_.get()));
  url_fetcher_->SetRequestContext(context_getter_);

  // The request protocol is JSON of the form:
  // {
  //   "protocol_version": "1",
  //   "hash": "<base64-encoded hash value here>",
  //   "ids": [ "<id1>", "id2" ]
  // }
  base::DictionaryValue dictionary;
  dictionary.SetInteger(kProtocolVersionKey, 1);
  dictionary.SetString(kHashKey, hash_base64);
  scoped_ptr<base::ListValue> id_list(new base::ListValue);
  for (ExtensionIdSet::const_iterator i = ids_.begin(); i != ids_.end(); ++i) {
    id_list->AppendString(*i);
  }
  dictionary.Set(kIdsKey, id_list.release());
  std::string json;
  base::JSONWriter::Write(&dictionary, &json);
  if (json.empty()) {
    ReportErrorViaCallback();
    return;
  }
  url_fetcher_->SetUploadData("application/json", json);
  url_fetcher_->Start();
}

void InstallSigner::ReportErrorViaCallback() {
  InstallSignature* null_signature = NULL;
  if (!callback_.is_null())
    callback_.Run(scoped_ptr<InstallSignature>(null_signature));
}

void InstallSigner::ParseFetchResponse() {
  std::string response;
  if (!url_fetcher_->GetStatus().is_success() ||
      !url_fetcher_->GetResponseAsString(&response) ||
      response.empty()) {
    ReportErrorViaCallback();
    return;
  }

  // The response is JSON of the form:
  // {
  //   "protocol_version": "1",
  //   "signature": "<base64-encoded signature>",
  //   "expiry": "<date in YYYY-MM-DD form>",
  //   "invalid_ids": [ "<id3>", "<id4>" ]
  // }
  // where |invalid_ids| is a list of ids from the original request that
  // could not be verified to be in the webstore.

  base::DictionaryValue* dictionary = NULL;
  scoped_ptr<base::Value> parsed(base::JSONReader::Read(response));
  if (!parsed.get() || !parsed->GetAsDictionary(&dictionary)) {
    ReportErrorViaCallback();
    return;
  }

  int protocol_version = 0;
  std::string signature_base64;
  std::string signature;
  std::string expire_date;

  dictionary->GetInteger(kProtocolVersionKey, &protocol_version);
  dictionary->GetString(kSignatureKey, &signature_base64);
  dictionary->GetString(kExpiryKey, &expire_date);

  if (protocol_version != 1 || signature_base64.empty() ||
      !ValidateExpireDateFormat(expire_date) ||
      !base::Base64Decode(signature_base64, &signature)) {
    ReportErrorViaCallback();
    return;
  }

  ExtensionIdSet invalid_ids;
  const base::ListValue* invalid_ids_list = NULL;
  if (dictionary->GetList(kInvalidIdsKey, &invalid_ids_list)) {
    for (size_t i = 0; i < invalid_ids_list->GetSize(); i++) {
      std::string id;
      if (!invalid_ids_list->GetString(i, &id)) {
        ReportErrorViaCallback();
        return;
      }
      invalid_ids.insert(id);
    }
  }

  HandleSignatureResult(signature, expire_date, invalid_ids);
}

void InstallSigner::HandleSignatureResult(const std::string& signature,
                                          const std::string& expire_date,
                                          const ExtensionIdSet& invalid_ids) {
  ExtensionIdSet valid_ids =
      base::STLSetDifference<ExtensionIdSet>(ids_, invalid_ids);

  scoped_ptr<InstallSignature> result;
  if (!signature.empty()) {
    result.reset(new InstallSignature);
    result->ids = valid_ids;
    result->salt = salt_;
    result->signature = signature;
    result->expire_date = expire_date;
    if (!VerifySignature(*result)) {
      UMA_HISTOGRAM_BOOLEAN("InstallSigner.InvalidSignature", true);
      result.reset();
    }
  }

  if (!callback_.is_null())
    callback_.Run(result.Pass());
}


}  // namespace extensions
