//
// Copyright RIME Developers
// Distributed under the BSD License
//
// 2013-08-31 GONG Chen <chen.sst@gmail.com>
//
#ifndef RIME_UNITY_TABLE_ENCODER_H_
#define RIME_UNITY_TABLE_ENCODER_H_

#include <rime/common.h>
#include <rime/algo/encoder.h>

namespace rime {

struct Ticket;
class ReverseLookupDictionary;
class UserDictionary;

class UnityTableEncoder : public TableEncoder, public PhraseCollector {
 public:
  UnityTableEncoder(UserDictionary* user_dict);
  ~UnityTableEncoder();

  bool Load(const Ticket& ticket);

  void CreateEntry(const string& word,
                   const string& code_str,
                   const string& weight_str);
  bool DeletePhrase(const string& phrase);
  // Encode the phrase by the schema's encoder rules without writing to
  // user_dict, and capture the first code produced.
  bool CapturePhraseCode(const string& phrase, string* code);
  // Tombstone auto-encoded junk sub-strings (excluding the phrase itself) of
  // the given phrase, only for sub-strings that already exist in the user dict.
  bool DeleteEncodedPhraseChain(const string& phrase);
  bool TranslateWord(const string& word, vector<string>* code);

  size_t LookupPhrases(UserDictEntryIterator* result,
                       const string& input,
                       bool predictive,
                       size_t limit = 0,
                       string* resume_key = NULL);

  static bool HasPrefix(const string& key);
  static bool AddPrefix(string* key);
  static bool RemovePrefix(string* key);

  // Prefix marking entries written by the (unity) table encoder, e.g. words
  // built via the make-word switch or the levers phrase APIs. Candidates from
  // such entries carry the unity symbol (yinyang) as their comment.
  static const char* kEncodedPrefix;

 protected:
  UserDictionary* user_dict_;
  the<ReverseLookupDictionary> rev_dict_;
  bool delete_mode_ = false;
  bool capture_mode_ = false;
  string captured_code_;
};

}  // namespace rime

#endif  // RIME_UNITY_TABLE_ENCODER_H_
