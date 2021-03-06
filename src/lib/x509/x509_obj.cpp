/*
* X.509 SIGNED Object
* (C) 1999-2007 Jack Lloyd
*
* Botan is released under the Simplified BSD License (see license.txt)
*/

#include <botan/x509_obj.h>
#include <botan/pubkey.h>
#include <botan/oids.h>
#include <botan/der_enc.h>
#include <botan/ber_dec.h>
#include <botan/parsing.h>
#include <botan/pem.h>
#include <algorithm>

namespace Botan {

namespace {
struct Pss_params
   {
   AlgorithmIdentifier hash_algo;
   AlgorithmIdentifier mask_gen_algo;
   AlgorithmIdentifier mask_gen_hash;  // redundant: decoded mask_gen_algo.parameters
   size_t salt_len;
   size_t trailer_field;
   };

Pss_params decode_pss_params(const std::vector<uint8_t>& encoded_pss_params)
   {
   Pss_params pss_parameter;
   BER_Decoder(encoded_pss_params)
      .start_cons(SEQUENCE)
         .decode_optional(pss_parameter.hash_algo, ASN1_Tag(0), PRIVATE, AlgorithmIdentifier("SHA-160",
                    AlgorithmIdentifier::USE_NULL_PARAM))
         .decode_optional(pss_parameter.mask_gen_algo, ASN1_Tag(1), PRIVATE,
                    AlgorithmIdentifier("MGF1", DER_Encoder().encode(AlgorithmIdentifier("SHA-160",
                                        AlgorithmIdentifier::USE_NULL_PARAM)).get_contents_unlocked()))
         .decode_optional(pss_parameter.salt_len, ASN1_Tag(2), PRIVATE, size_t(20))
         .decode_optional(pss_parameter.trailer_field, ASN1_Tag(3), PRIVATE, size_t(1))
      .end_cons();

   BER_Decoder(pss_parameter.mask_gen_algo.parameters).decode(pss_parameter.mask_gen_hash);

   return pss_parameter;
   }
}

/*
* Create a generic X.509 object
*/
X509_Object::X509_Object(DataSource& stream, const std::string& labels)
   {
   init(stream, labels);
   }

#if defined(BOTAN_TARGET_OS_HAS_FILESYSTEM)
/*
* Create a generic X.509 object
*/
X509_Object::X509_Object(const std::string& file, const std::string& labels)
   {
   DataSource_Stream stream(file, true);
   init(stream, labels);
   }
#endif

/*
* Create a generic X.509 object
*/
X509_Object::X509_Object(const std::vector<uint8_t>& vec, const std::string& labels)
   {
   DataSource_Memory stream(vec.data(), vec.size());
   init(stream, labels);
   }

/*
* Read a PEM or BER X.509 object
*/
void X509_Object::init(DataSource& in, const std::string& labels)
   {
   m_PEM_labels_allowed = split_on(labels, '/');
   if(m_PEM_labels_allowed.size() < 1)
      throw Invalid_Argument("Bad labels argument to X509_Object");

   m_PEM_label_pref = m_PEM_labels_allowed[0];
   std::sort(m_PEM_labels_allowed.begin(), m_PEM_labels_allowed.end());

   try {
      if(ASN1::maybe_BER(in) && !PEM_Code::matches(in))
         {
         BER_Decoder dec(in);
         decode_from(dec);
         }
      else
         {
         std::string got_label;
         DataSource_Memory ber(PEM_Code::decode(in, got_label));

         if(!std::binary_search(m_PEM_labels_allowed.begin(),
                                m_PEM_labels_allowed.end(), got_label))
            throw Decoding_Error("Invalid PEM label: " + got_label);

         BER_Decoder dec(ber);
         decode_from(dec);
         }
      }
   catch(Decoding_Error& e)
      {
      throw Decoding_Error(m_PEM_label_pref + " decoding failed: " + e.what());
      }
   }


void X509_Object::encode_into(DER_Encoder& to) const
   {
   to.start_cons(SEQUENCE)
         .start_cons(SEQUENCE)
            .raw_bytes(signed_body())
         .end_cons()
         .encode(signature_algorithm())
         .encode(signature(), BIT_STRING)
      .end_cons();
   }

/*
* Read a BER encoded X.509 object
*/
void X509_Object::decode_from(BER_Decoder& from)
   {
   from.start_cons(SEQUENCE)
         .start_cons(SEQUENCE)
            .raw_bytes(m_tbs_bits)
         .end_cons()
         .decode(m_sig_algo)
         .decode(m_sig, BIT_STRING)
      .end_cons();
   }

/*
* Return a BER encoded X.509 object
*/
std::vector<uint8_t> X509_Object::BER_encode() const
   {
   DER_Encoder der;
   encode_into(der);
   return der.get_contents_unlocked();
   }

/*
* Return a PEM encoded X.509 object
*/
std::string X509_Object::PEM_encode() const
   {
   return PEM_Code::encode(BER_encode(), m_PEM_label_pref);
   }

/*
* Return the TBS data
*/
std::vector<uint8_t> X509_Object::tbs_data() const
   {
   return ASN1::put_in_sequence(m_tbs_bits);
   }

/*
* Return the hash used in generating the signature
*/
std::string X509_Object::hash_used_for_signature() const
   {
   const OID oid = m_sig_algo.oid;
   std::vector<std::string> sig_info = split_on(OIDS::lookup(oid), '/');

   if(sig_info.size() != 2)
      throw Internal_Error("Invalid name format found for " +
                           oid.as_string());

   if(sig_info[1] == "EMSA4")
      {
      return OIDS::lookup(decode_pss_params(signature_algorithm().parameters).hash_algo.oid);
      }
   else
      {
      std::vector<std::string> pad_and_hash =
         parse_algorithm_name(sig_info[1]);

      if(pad_and_hash.size() != 2)
         {
         throw Internal_Error("Invalid name format " + sig_info[1]);
         }

      return pad_and_hash[1];
      }
   }

/*
* Check the signature on an object
*/
bool X509_Object::check_signature(const Public_Key* pub_key) const
   {
   if(!pub_key)
      throw Exception("No key provided for " + m_PEM_label_pref + " signature check");
   std::unique_ptr<const Public_Key> key(pub_key);
   return check_signature(*key);
   }

/*
* Check the signature on an object
*/
bool X509_Object::check_signature(const Public_Key& pub_key) const
   {
   try {
      std::vector<std::string> sig_info =
         split_on(OIDS::lookup(m_sig_algo.oid), '/');

      if(sig_info.size() != 2 || sig_info[0] != pub_key.algo_name())
         return false;

      std::string padding = sig_info[1];
      Signature_Format format =
         (pub_key.message_parts() >= 2) ? DER_SEQUENCE : IEEE_1363;

      if(padding == "EMSA4")
         {
         // "MUST contain RSASSA-PSS-params"
         if(signature_algorithm().parameters.empty())
            {
            return false;
            }

         Pss_params pss_parameter = decode_pss_params(signature_algorithm().parameters);

         // hash_algo must be SHA1, SHA2-224, SHA2-256, SHA2-384 or SHA2-512
         std::string hash_algo = OIDS::lookup(pss_parameter.hash_algo.oid);
         if(hash_algo != "SHA-160" && hash_algo != "SHA-224" && hash_algo != "SHA-256" && hash_algo != "SHA-384"
               && hash_algo != "SHA-512")
            {
            return false;
            }

         std::string mgf_algo = OIDS::lookup(pss_parameter.mask_gen_algo.oid);
         if(mgf_algo != "MGF1")
            {
            return false;
            }

         // For MGF1, it is strongly RECOMMENDED that the underlying hash function be the same as the one identified by hashAlgorithm
         // Must be SHA1, SHA2-224, SHA2-256, SHA2-384 or SHA2-512
         if(pss_parameter.mask_gen_hash.oid != pss_parameter.hash_algo.oid)
            {
            return false;
            }

         if(pss_parameter.trailer_field != 1)
            {
            return false;
            }

         padding += "(" + hash_algo;
         padding += "," + mgf_algo;
         padding += "," + std::to_string(pss_parameter.salt_len) +
                    ")";   // salt_len is actually not used for verification. Length is inferred from the signature
         }

      PK_Verifier verifier(pub_key, padding, format);

      return verifier.verify_message(tbs_data(), signature());
      }
   catch(std::exception&)
      {
      return false;
      }
   }

/*
* Apply the X.509 SIGNED macro
*/
std::vector<uint8_t> X509_Object::make_signed(PK_Signer* signer,
                                            RandomNumberGenerator& rng,
                                            const AlgorithmIdentifier& algo,
                                            const secure_vector<uint8_t>& tbs_bits)
   {
   return DER_Encoder()
      .start_cons(SEQUENCE)
         .raw_bytes(tbs_bits)
         .encode(algo)
         .encode(signer->sign_message(tbs_bits, rng), BIT_STRING)
      .end_cons()
   .get_contents_unlocked();
   }

/*
* Try to decode the actual information
*/
void X509_Object::do_decode()
   {
   try {
      force_decode();
      }
   catch(Decoding_Error& e)
      {
      throw Decoding_Error(m_PEM_label_pref + " decoding failed", e.what());
      }
   catch(Invalid_Argument& e)
      {
      throw Decoding_Error(m_PEM_label_pref + " decoding failed", e.what());
      }
   }

}
