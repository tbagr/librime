// implementation of levers api

#include <rime_levers_api.h>

#include <rime/service.h>
#include <rime/schema.h>
#include <rime/ticket.h>
#include <rime/dict/dictionary.h>
#include <rime/dict/user_dictionary.h>
#include <rime/gear/unity_table_encoder.h>
#include <rime/lever/custom_settings.h>
#include <rime/lever/switcher_settings.h>
#include <rime/lever/user_dict_manager.h>

#include <utf8.h>

#include <fstream>
#include <set>
#include <vector>
#include <windows.h>

using namespace rime;

static void rime_levers_trace(const wchar_t* message) {
  wchar_t temp_path[MAX_PATH] = {0};
  const DWORD length = GetTempPathW(_countof(temp_path), temp_path);
  if (length == 0 || length >= _countof(temp_path)) {
    return;
  }
  std::wstring path = std::wstring(temp_path) + L"weasel_librime_trace.log";
  std::wofstream out(path, std::ios::app);
  if (!out.is_open()) {
    return;
  }
  SYSTEMTIME st = {0};
  GetLocalTime(&st);
  out << L'[' << st.wHour << L':' << st.wMinute << L':' << st.wSecond << L'.'
      << st.wMilliseconds << L"] " << message << std::endl;
}

static RimeCustomSettings* rime_levers_custom_settings_init(
    const char* config_id,
    const char* generator_id) {
  return reinterpret_cast<RimeCustomSettings*>(new CustomSettings(
      &Service::instance().deployer(), config_id, generator_id));
}

static void rime_levers_custom_settings_destroy(RimeCustomSettings* settings) {
  delete reinterpret_cast<CustomSettings*>(settings);
}

static Bool rime_levers_load_settings(RimeCustomSettings* settings) {
  return Bool(reinterpret_cast<CustomSettings*>(settings)->Load());
}

static Bool rime_levers_save_settings(RimeCustomSettings* settings) {
  return Bool(reinterpret_cast<CustomSettings*>(settings)->Save());
}

static Bool rime_levers_customize_bool(RimeCustomSettings* settings,
                                       const char* key,
                                       Bool value) {
  an<ConfigItem> item = New<ConfigValue>(bool(value));
  auto custom_settings = reinterpret_cast<CustomSettings*>(settings);
  return custom_settings->Customize(key, item);
}

static Bool rime_levers_customize_int(RimeCustomSettings* settings,
                                      const char* key,
                                      int value) {
  an<ConfigItem> item = New<ConfigValue>(value);
  auto custom_settings = reinterpret_cast<CustomSettings*>(settings);
  return custom_settings->Customize(key, item);
}

static Bool rime_levers_customize_double(RimeCustomSettings* settings,
                                         const char* key,
                                         double value) {
  an<ConfigItem> item = New<ConfigValue>(value);
  auto custom_settings = reinterpret_cast<CustomSettings*>(settings);
  return custom_settings->Customize(key, item);
}

static Bool rime_levers_customize_string(RimeCustomSettings* settings,
                                         const char* key,
                                         const char* value) {
  an<ConfigItem> item = New<ConfigValue>(value);
  auto custom_settings = reinterpret_cast<CustomSettings*>(settings);
  return custom_settings->Customize(key, item);
}

static Bool rime_levers_customize_item(RimeCustomSettings* settings,
                                       const char* key,
                                       RimeConfig* value) {
  an<ConfigItem> item;
  if (value) {
    if (Config* v = reinterpret_cast<Config*>(value->ptr)) {
      item = v->GetItem("");
    }
  }
  auto custom_settings = reinterpret_cast<CustomSettings*>(settings);
  return custom_settings->Customize(key, item);
}

static Bool rime_levers_is_first_run(RimeCustomSettings* settings) {
  return reinterpret_cast<CustomSettings*>(settings)->IsFirstRun();
}

static Bool rime_levers_settings_is_modified(RimeCustomSettings* settings) {
  return reinterpret_cast<CustomSettings*>(settings)->modified();
}

static Bool rime_levers_settings_get_config(RimeCustomSettings* settings,
                                            RimeConfig* config) {
  if (!config)
    return False;
  config->ptr = reinterpret_cast<CustomSettings*>(settings)->config();
  return Bool(!!config->ptr);
}

static RimeSwitcherSettings* rime_levers_switcher_settings_init() {
  return reinterpret_cast<RimeSwitcherSettings*>(
      new SwitcherSettings(&Service::instance().deployer()));
}

static Bool rime_levers_get_available_schema_list(
    RimeSwitcherSettings* settings,
    RimeSchemaList* list) {
  auto ss = reinterpret_cast<SwitcherSettings*>(settings);
  list->size = 0;
  list->list = NULL;
  if (ss->available().empty()) {
    return False;
  }
  list->list = new RimeSchemaListItem[ss->available().size()];
  for (const auto& info : ss->available()) {
    auto& item(list->list[list->size]);
    item.schema_id = const_cast<char*>(info.schema_id.c_str());
    item.name = const_cast<char*>(info.name.c_str());
    item.reserved = const_cast<SchemaInfo*>(&info);
    ++list->size;
  }
  return True;
}

static Bool rime_levers_get_selected_schema_list(RimeSwitcherSettings* settings,
                                                 RimeSchemaList* list) {
  auto ss = reinterpret_cast<SwitcherSettings*>(settings);
  list->size = 0;
  list->list = NULL;
  if (ss->selection().empty()) {
    return False;
  }
  list->list = new RimeSchemaListItem[ss->selection().size()];
  for (const string& schema_id : ss->selection()) {
    auto& item(list->list[list->size]);
    item.schema_id = const_cast<char*>(schema_id.c_str());
    item.name = NULL;
    item.reserved = NULL;
    ++list->size;
  }
  return True;
}

static void rime_levers_schema_list_destroy(RimeSchemaList* list) {
  delete[] list->list;
  list->size = 0;
  list->list = NULL;
}

static const char* rime_levers_get_schema_id(RimeSchemaInfo* info) {
  auto si = reinterpret_cast<SchemaInfo*>(info);
  return si && !si->schema_id.empty() ? si->schema_id.c_str() : NULL;
}

static const char* rime_levers_get_schema_name(RimeSchemaInfo* info) {
  auto si = reinterpret_cast<SchemaInfo*>(info);
  return si && !si->name.empty() ? si->name.c_str() : NULL;
}

static const char* rime_levers_get_schema_version(RimeSchemaInfo* info) {
  auto si = reinterpret_cast<SchemaInfo*>(info);
  return si && !si->version.empty() ? si->version.c_str() : NULL;
}
static const char* rime_levers_get_schema_author(RimeSchemaInfo* info) {
  auto si = reinterpret_cast<SchemaInfo*>(info);
  return si && !si->author.empty() ? si->author.c_str() : NULL;
}

static const char* rime_levers_get_schema_description(RimeSchemaInfo* info) {
  auto si = reinterpret_cast<SchemaInfo*>(info);
  return si && !si->description.empty() ? si->description.c_str() : NULL;
}

static const char* rime_levers_get_schema_file_path(RimeSchemaInfo* info) {
  auto si = reinterpret_cast<SchemaInfo*>(info);
  return si && !si->file_path.empty() ? si->file_path.c_str() : NULL;
}

static Bool rime_levers_select_schemas(RimeSwitcherSettings* settings,
                                       const char* schema_id_list[],
                                       int count) {
  auto ss = reinterpret_cast<SwitcherSettings*>(settings);
  SwitcherSettings::Selection selection;
  for (int i = 0; i < count; ++i) {
    selection.push_back(schema_id_list[i]);
  }
  return ss->Select(selection);
}

static const char* rime_levers_get_hotkeys(RimeSwitcherSettings* settings) {
  auto ss = reinterpret_cast<SwitcherSettings*>(settings);
  return !ss->hotkeys().empty() ? ss->hotkeys().c_str() : NULL;
}

static Bool rime_levers_set_hotkeys(RimeSwitcherSettings* settings,
                                    const char* hotkeys) {
  auto ss = reinterpret_cast<SwitcherSettings*>(settings);
  return Bool(ss->SetHotkeys(hotkeys));
}

static Bool rime_levers_user_dict_iterator_init(RimeUserDictIterator* iter) {
  UserDictManager mgr(&Service::instance().deployer());
  UserDictList* list = new UserDictList;
  mgr.GetUserDictList(list);
  if (list->empty()) {
    delete list;
    return False;
  }
  iter->ptr = list;
  iter->i = 0;
  return True;
}

static void rime_levers_user_dict_iterator_destroy(RimeUserDictIterator* iter) {
  delete (UserDictList*)iter->ptr;
  iter->ptr = NULL;
  iter->i = 0;
}

static const char* rime_levers_next_user_dict(RimeUserDictIterator* iter) {
  auto list = reinterpret_cast<UserDictList*>(iter->ptr);
  if (!list || iter->i >= list->size()) {
    return NULL;
  }
  return (*list)[iter->i++].c_str();
}

static Bool rime_levers_backup_user_dict(const char* dict_name) {
  UserDictManager mgr(&Service::instance().deployer());
  return Bool(mgr.Backup(dict_name));
}

static Bool rime_levers_restore_user_dict(const char* snapshot_file) {
  UserDictManager mgr(&Service::instance().deployer());
  return Bool(mgr.Restore(path(snapshot_file)));
}

static int rime_levers_export_user_dict(const char* dict_name,
                                        const char* text_file) {
  UserDictManager mgr(&Service::instance().deployer());
  return mgr.Export(dict_name, path(text_file));
}

static int rime_levers_import_user_dict(const char* dict_name,
                                        const char* text_file) {
  UserDictManager mgr(&Service::instance().deployer());
  return mgr.Import(dict_name, path(text_file));
}

namespace {
// Forward declaration; defined below in this anonymous namespace.
bool delete_encoded_phrase_chain(UserDictionary* user_dict,
                                 const Ticket& ticket,
                                 const string& phrase);
}  // namespace

static Bool rime_levers_update_user_phrase(const char* schema_id,
                                           const char* code,
                                           const char* text,
                                           int commits) {
  rime_levers_trace(L"rime_levers_update_user_phrase begin");
  if (!schema_id || !code || !text || !*schema_id || !*code || !*text) {
    rime_levers_trace(L"rime_levers_update_user_phrase invalid args");
    return False;
  }
  Schema schema(schema_id);
  rime_levers_trace(L"rime_levers_update_user_phrase schema created");
  if (!schema.config()) {
    rime_levers_trace(L"rime_levers_update_user_phrase schema config missing");
    return False;
  }
  Ticket ticket(&schema, "translator");
  rime_levers_trace(L"rime_levers_update_user_phrase ticket created");
  auto user_dictionary = UserDictionary::Require("user_dictionary");
  if (!user_dictionary) {
    rime_levers_trace(L"rime_levers_update_user_phrase user_dictionary missing");
    return False;
  }
  the<UserDictionary> user_dict(user_dictionary->Create(ticket));
  rime_levers_trace(L"rime_levers_update_user_phrase user_dict created");
  if (!user_dict) {
    rime_levers_trace(L"rime_levers_update_user_phrase user_dict create failed");
    return False;
  }
  rime_levers_trace(L"rime_levers_update_user_phrase before user_dict load");
  if (!user_dict->Load()) {
    rime_levers_trace(L"rime_levers_update_user_phrase user_dict load failed");
    return False;
  }
  rime_levers_trace(L"rime_levers_update_user_phrase user_dict loaded");
  if (user_dict->readonly()) {
    rime_levers_trace(L"rime_levers_update_user_phrase user_dict readonly");
    return False;
  }
  if (auto dictionary = Dictionary::Require("dictionary")) {
    rime_levers_trace(L"rime_levers_update_user_phrase dictionary required");
    the<Dictionary> dict(dictionary->Create(ticket));
    if (dict && dict->Load()) {
      rime_levers_trace(L"rime_levers_update_user_phrase dictionary loaded");
      user_dict->Attach(dict->primary_table(), dict->prism());
      rime_levers_trace(L"rime_levers_update_user_phrase user_dict attached");
    }
  }
  DictEntry entry;
  entry.text = text;
  string code_str(code);
  while (!code_str.empty() && code_str.back() == ' ') {
    code_str.pop_back();
  }
  // trailing space is part of the user dict key format; without it the entry
  // would not match on exact lookup and would duplicate on sync import.
  // the entry is written without any marker, so the synced user dictionary
  // stays clean; the candidate shows the unity symbol (yinyang) because it
  // comes from the user dictionary.
  entry.custom_code = code_str + ' ';
  rime_levers_trace(L"rime_levers_update_user_phrase before UpdateEntry");
  const bool ok = user_dict->UpdateEntry(entry, commits);
  rime_levers_trace(ok ? L"rime_levers_update_user_phrase UpdateEntry ok"
                       : L"rime_levers_update_user_phrase UpdateEntry failed");
  if (ok) {
    // Clean up auto-encoded sub-string words after add/remove, so no junk
    // words from earlier phrase encoding remain.
    delete_encoded_phrase_chain(user_dict.get(), ticket, text);
  }
  return Bool(ok);
}

static Bool rime_levers_add_user_phrase(const char* schema_id,
                                        const char* code,
                                        const char* text) {
  return rime_levers_update_user_phrase(schema_id, code, text, 1);
}

static Bool rime_levers_remove_user_phrase(const char* schema_id,
                                           const char* code,
                                           const char* text) {
  return rime_levers_update_user_phrase(schema_id, code, text, -1);
}

// ---- make-word support: whole-phrase encoding + junk sub-string cleanup ----
// Reference: trime2 (app/src/main/jni/librime_jni/rime_jni.cc)
// SinglePhraseEncoder / deleteEncodedPhraseChain / addExplicitEncodedPhrase.
namespace {

class SinglePhraseEncoder : public UnityTableEncoder {
 public:
  using UnityTableEncoder::UnityTableEncoder;
  void CreateEntry(const string& word,
                   const string& code_str,
                   const string& weight_str) override {
    if (captured_code_.empty() && !word.empty() && !code_str.empty()) {
      captured_code_ = code_str;
    }
  }
  const string& captured_code() const { return captured_code_; }

 private:
  string captured_code_;
};

vector<string> split_utf8_chars(const string& text) {
  vector<string> chars;
  auto it = text.begin();
  while (it != text.end()) {
    auto start = it;
    utf8::next(it, text.end());
    chars.emplace_back(start, it);
  }
  return chars;
}

// Delete auto-encoded words of every sub-string (including the phrase itself)
// of the given phrase, cleaning up junk words left by phrase encoding.
bool delete_encoded_phrase_chain(UserDictionary* user_dict,
                                 const Ticket& ticket,
                                 const string& phrase) {
  if (!user_dict || phrase.empty()) {
    return false;
  }
  UnityTableEncoder encoder(user_dict);
  if (!encoder.Load(ticket) || !encoder.loaded()) {
    return false;
  }
  auto chars = split_utf8_chars(phrase);
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
    if (!encoder.CapturePhraseCode(subphrase, &code)) {
      continue;
    }
    // only tombstone sub-strings that already exist as auto-encoded entries,
    // otherwise a fresh tombstone would pollute the user dict as well
    UserDictEntryIterator iter;
    encoder.LookupPhrases(&iter, code, false, 1, nullptr);
    if (iter.exhausted()) {
      continue;
    }
    deleted_any = encoder.DeletePhrase(subphrase) || deleted_any;
  }
  return deleted_any;
}

}  // namespace

// Encode a whole phrase by the schema's encoder rules into user_dict at
// fixed frequency, and clean up all auto-encoded sub-strings of the phrase.
static Bool rime_levers_encode_user_phrase(const char* schema_id,
                                           const char* text) {
  rime_levers_trace(L"rime_levers_encode_user_phrase begin");
  if (!schema_id || !text || !*schema_id || !*text) {
    rime_levers_trace(L"rime_levers_encode_user_phrase invalid args");
    return False;
  }
  Schema schema(schema_id);
  if (!schema.config()) {
    rime_levers_trace(L"rime_levers_encode_user_phrase schema config missing");
    return False;
  }
  Ticket ticket(&schema, "translator");
  auto user_dictionary = UserDictionary::Require("user_dictionary");
  if (!user_dictionary) {
    rime_levers_trace(L"rime_levers_encode_user_phrase user_dictionary missing");
    return False;
  }
  the<UserDictionary> user_dict(user_dictionary->Create(ticket));
  if (!user_dict || !user_dict->Load() || user_dict->readonly()) {
    rime_levers_trace(L"rime_levers_encode_user_phrase user_dict failed");
    return False;
  }
  if (auto dictionary = Dictionary::Require("dictionary")) {
    the<Dictionary> dict(dictionary->Create(ticket));
    if (dict && dict->Load()) {
      user_dict->Attach(dict->primary_table(), dict->prism());
    }
  }
  SinglePhraseEncoder encoder(user_dict.get());
  if (!encoder.Load(ticket) || !encoder.loaded()) {
    rime_levers_trace(L"rime_levers_encode_user_phrase encoder load failed");
    return False;
  }
  const string phrase(text);
  if (!encoder.EncodePhrase(phrase, "1") || encoder.captured_code().empty()) {
    rime_levers_trace(L"rime_levers_encode_user_phrase encode failed");
    return False;
  }
  DictEntry entry;
  entry.text = phrase;
  // trailing space is part of the user dict key format; without it the entry
  // would not match on exact lookup and would duplicate on sync import
  entry.custom_code = encoder.captured_code() + ' ';
  const bool inserted = user_dict->UpdateEntry(entry, 1);
  if (!inserted) {
    rime_levers_trace(L"rime_levers_encode_user_phrase UpdateEntry failed");
    return False;
  }
  delete_encoded_phrase_chain(user_dict.get(), ticket, phrase);
  rime_levers_trace(L"rime_levers_encode_user_phrase ok");
  return True;
}

static RimeCustomApi* RIME_FLAVORED(rime_levers_get_api)() {
  static RIME_FLAVORED(RimeLeversApi) s_api = {0};
  if (!s_api.data_size) {
    RIME_STRUCT_INIT(RIME_FLAVORED(RimeLeversApi), s_api);
    s_api.custom_settings_init = rime_levers_custom_settings_init;
    s_api.custom_settings_destroy = rime_levers_custom_settings_destroy;
    s_api.load_settings = rime_levers_load_settings;
    s_api.save_settings = rime_levers_save_settings;
    s_api.customize_bool = rime_levers_customize_bool;
    s_api.customize_int = rime_levers_customize_int;
    s_api.customize_double = rime_levers_customize_double;
    s_api.customize_string = rime_levers_customize_string;
    s_api.is_first_run = rime_levers_is_first_run;
    s_api.settings_is_modified = rime_levers_settings_is_modified;
    s_api.settings_get_config = rime_levers_settings_get_config;
    s_api.switcher_settings_init = rime_levers_switcher_settings_init;
    s_api.get_available_schema_list = rime_levers_get_available_schema_list;
    s_api.get_selected_schema_list = rime_levers_get_selected_schema_list;
    s_api.schema_list_destroy = rime_levers_schema_list_destroy;
    s_api.get_schema_id = rime_levers_get_schema_id;
    s_api.get_schema_name = rime_levers_get_schema_name;
    s_api.get_schema_version = rime_levers_get_schema_version;
    s_api.get_schema_author = rime_levers_get_schema_author;
    s_api.get_schema_description = rime_levers_get_schema_description;
    s_api.get_schema_file_path = rime_levers_get_schema_file_path;
    s_api.select_schemas = rime_levers_select_schemas;
    s_api.get_hotkeys = rime_levers_get_hotkeys;
    s_api.set_hotkeys = rime_levers_set_hotkeys;
    s_api.user_dict_iterator_init = rime_levers_user_dict_iterator_init;
    s_api.user_dict_iterator_destroy = rime_levers_user_dict_iterator_destroy;
    s_api.next_user_dict = rime_levers_next_user_dict;
    s_api.backup_user_dict = rime_levers_backup_user_dict;
    s_api.restore_user_dict = rime_levers_restore_user_dict;
    s_api.export_user_dict = rime_levers_export_user_dict;
    s_api.import_user_dict = rime_levers_import_user_dict;
    s_api.add_user_phrase = rime_levers_add_user_phrase;
    s_api.remove_user_phrase = rime_levers_remove_user_phrase;
    s_api.encode_user_phrase = rime_levers_encode_user_phrase;
    s_api.customize_item = rime_levers_customize_item;
  }
  return (RimeCustomApi*)&s_api;
}
