//
// Copyright RIME Developers
// Distributed under the BSD License
//
// 2013-08-31 GONG Chen <chen.sst@gmail.com>
//
#include <boost/algorithm/string.hpp>
#include <set>
#include <utf8.h>
#include <rime/dict/dict_settings.h>
#include <rime/dict/user_dictionary.h>
#include <rime/dict/reverse_lookup_dictionary.h>
#include <rime/gear/unity_table_encoder.h>

namespace rime {

const char* UnityTableEncoder::kEncodedPrefix = "\x7f""enc\x1f";

UnityTableEncoder::UnityTableEncoder(UserDictionary* user_dict)
    : TableEncoder(NULL), user_dict_(user_dict) {
  set_collector(this);
}

UnityTableEncoder::~UnityTableEncoder() {}

bool UnityTableEncoder::Load(const Ticket& ticket) {
  auto c = ReverseLookupDictionary::Require("reverse_lookup_dictionary");
  if (!c) {
    LOG(ERROR) << "component not available: reverse_lookup_dictionary";
    return false;
  }
  rev_dict_.reset(c->Create(ticket));
  if (!rev_dict_ || !rev_dict_->Load()) {
    LOG(ERROR) << "error loading dictionary for unity table encoder.";
    return false;
  }
  auto settings = rev_dict_->GetDictSettings();
  if (!settings || !settings->use_rule_based_encoder()) {
    LOG(WARNING) << "unity table encoder is not enabled in dict settings.";
    return false;
  }
  return LoadSettings(settings.get());
}

void UnityTableEncoder::CreateEntry(const string& word,
                                    const string& code_str,
                                    const string& weight_str) {
  if (!user_dict_)
    return;
  if (capture_mode_) {
    if (captured_code_.empty() && !word.empty() && !code_str.empty()) {
      captured_code_ = code_str;
    }
    return;
  }
  DictEntry entry;
  entry.text = word;
  entry.custom_code = code_str + ' ';
  if (delete_mode_) {
    user_dict_->UpdateEntry(entry, -1, kEncodedPrefix);
    return;
  }
  int commits = (weight_str == "0") ? 0 : 1;
  user_dict_->UpdateEntry(entry, commits, kEncodedPrefix);
}

bool UnityTableEncoder::DeletePhrase(const string& phrase) {
  if (!loaded_ || !user_dict_ || phrase.empty()) {
    return false;
  }
  delete_mode_ = true;
  bool ok = EncodePhrase(phrase, "0");
  delete_mode_ = false;
  return ok;
}

bool UnityTableEncoder::CapturePhraseCode(const string& phrase, string* code) {
  if (!loaded_ || !user_dict_ || !code || phrase.empty()) {
    return false;
  }
  capture_mode_ = true;
  captured_code_.clear();
  bool ok = EncodePhrase(phrase, "1");
  capture_mode_ = false;
  if (ok && !captured_code_.empty()) {
    *code = captured_code_;
    return true;
  }
  return false;
}

bool UnityTableEncoder::DeleteEncodedPhraseChain(const string& phrase) {
  if (!loaded_ || !user_dict_ || phrase.empty()) {
    return false;
  }
  vector<string> chars;
  auto it = phrase.begin();
  while (it != phrase.end()) {
    auto start = it;
    utf8::next(it, phrase.end());
    chars.emplace_back(start, it);
  }
  if (chars.size() < 2) {
    return false;
  }
  std::set<string> phrases_to_delete;
  // exclude the phrase itself: it is written at fixed frequency, not junk
  for (size_t length = 1; length < chars.size(); ++length) {
    for (size_t start = 0; start + length <= chars.size(); ++start) {
      string subphrase;
      for (size_t i = start; i < start + length; ++i) {
        subphrase += chars[i];
      }
      phrases_to_delete.insert(subphrase);
    }
  }
  bool deleted_any = false;
  for (const auto& subphrase : phrases_to_delete) {
    string code;
    if (!CapturePhraseCode(subphrase, &code)) {
      continue;
    }
    // only tombstone sub-strings that already exist as auto-encoded entries,
    // otherwise a fresh tombstone would pollute the user dict as well
    UserDictEntryIterator iter;
    LookupPhrases(&iter, code, false, 1, nullptr);
    if (iter.exhausted()) {
      continue;
    }
    deleted_any = DeletePhrase(subphrase) || deleted_any;
  }
  return deleted_any;
}

bool UnityTableEncoder::TranslateWord(const string& word,
                                      vector<string>* code) {
  if (!rev_dict_) {
    return false;
  }
  string str_list;
  if (rev_dict_->LookupStems(word, &str_list) ||
      rev_dict_->ReverseLookup(word, &str_list)) {
    boost::split(*code, str_list, boost::is_any_of(" "));
    return !code->empty();
  }
  return false;
}

size_t UnityTableEncoder::LookupPhrases(UserDictEntryIterator* result,
                                        const string& input,
                                        bool predictive,
                                        size_t limit,
                                        string* resume_key) {
  if (!user_dict_)
    return 0;
  return user_dict_->LookupWords(result, kEncodedPrefix + input, predictive,
                                 limit, resume_key);
}

bool UnityTableEncoder::HasPrefix(const string& key) {
  return boost::starts_with(key, kEncodedPrefix);
}

bool UnityTableEncoder::AddPrefix(string* key) {
  key->insert(0, kEncodedPrefix);
  return true;
}

bool UnityTableEncoder::RemovePrefix(string* key) {
  if (!HasPrefix(*key))
    return false;
  key->erase(0, strlen(kEncodedPrefix));
  return true;
}

}  // namespace rime
