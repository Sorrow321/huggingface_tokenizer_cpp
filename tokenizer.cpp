#include <iostream>
#include <algorithm>
#include <vector>
#include <string>
#include <regex>
#include <fstream>
#include "nlohmann/json.hpp"
#include <unicode/uchar.h>

using namespace std;
using json = nlohmann::json;


template <std::ctype_base::mask mask>
class IsNot
{
    std::locale myLocale;
    std::ctype<char> const* myCType;
public:
    IsNot( std::locale const& l = std::locale() )
        : myLocale( l )
        , myCType( &std::use_facet<std::ctype<char> >( l ) )
    {
    }
    bool operator()( char ch ) const
    {
        return ! myCType->is( mask, ch );
    }
};

typedef IsNot<std::ctype_base::space> IsNotSpace;

std::string trim( std::string const& original )
{
    std::string::const_iterator right = std::find_if(original.rbegin(), original.rend(), IsNotSpace()).base();
    std::string::const_iterator left = std::find_if(original.begin(), right, IsNotSpace() );
    return std::string( left, right );
}

std::vector<std::wstring> split(const std::wstring& input) {
    std::wstringstream stream(input);
    std::vector<std::wstring> words;
    std::wstring word;
    while (stream >> word) {
        words.push_back(word);
    }
    return words;
}

bool isPunctuation(UChar32 charCode) {
    UCharCategory category = static_cast<UCharCategory>(u_charType(charCode));

    switch (category) {
        case U_DASH_PUNCTUATION:
        case U_START_PUNCTUATION:
        case U_END_PUNCTUATION:
        case U_CONNECTOR_PUNCTUATION:
        case U_OTHER_PUNCTUATION:
        case U_INITIAL_PUNCTUATION:
        case U_FINAL_PUNCTUATION:
            return true;
        default:
            return false;
    }
}

bool _is_punctuation(UChar32 c)
{
    if((c >= 33 && c <= 47) || (c >= 58 && c <= 64) || (c >= 91 && c <= 96) || (c >= 123 && c <= 126)) {
        return true;
    }
    if (isPunctuation(c)) {
        return true;
    }
    return false;
}

bool _is_chinese_char(UChar32 c) {
    // This defines a "Chinese character" as anything in the CJK Unicode block:
    // https://en.wikipedia.org/wiki/CJK_Unified_Ideographs_(Unicode_block)
    //
    // Note that the CJK Unicode block is NOT all Japanese and Korean characters,
    // despite its name. The modern Korean Hangul alphabet is a different block,
    // as is Japanese Hiragana and Katakana. Those alphabets are used to write
    // space-separated words, so they are not treated specially and handled
    // like all of the other languages.

    if ((c >= 0x4E00 && c <= 0x9FFF) ||  // CJK Unified Ideographs
        (c >= 0x3400 && c <= 0x4DBF) ||  // CJK Unified Ideographs Extension A
        (c >= 0x20000 && c <= 0x2A6DF) ||  // CJK Unified Ideographs Extension B
        (c >= 0x2A700 && c <= 0x2B73F) ||  // CJK Unified Ideographs Extension C
        (c >= 0x2B740 && c <= 0x2B81F) ||  // CJK Unified Ideographs Extension D
        (c >= 0x2B820 && c <= 0x2CEAF) ||  // CJK Unified Ideographs Extension E
        (c >= 0xF900 && c <= 0xFAFF) ||  // CJK Compatibility Ideographs
        (c >= 0x2F800 && c <= 0x2FA1F)) {  // CJK Compatibility Ideographs Supplement
        return true;
    }
    return false;
}

wstring pad_chinese_chars(const wstring& text)
{
    vector<wchar_t> vec_padded_chars;
    for(auto &c: text) {
        if(_is_chinese_char(static_cast<UChar32>(c))) {
            vec_padded_chars.push_back(L' '); // wide-character representation of space
            vec_padded_chars.push_back(c);
            vec_padded_chars.push_back(L' ');
        }else{
            vec_padded_chars.push_back(c);
        }
    }
    return wstring(vec_padded_chars.begin(), vec_padded_chars.end());
}

vector<wstring> run_split_on_punctuation(const wstring& text, bool split_specials, const vector<wstring>& special_tokens)
{
    if(!split_specials && find(special_tokens.begin(), special_tokens.end(), text) != special_tokens.end()) {
        // we do not want to split special tokens and we found the text in the vector of special tokens
        return vector<wstring> {text};
    }
    size_t i = 0;
    bool start_new_word = true;
    vector<vector<wchar_t>> output;

    while(i < text.length()) {
        wchar_t c = text[i];
        if (_is_punctuation(static_cast<UChar32>(c))) {
            vector<wchar_t> s;
            s.push_back(c);
            output.push_back(s);
            start_new_word = true;
        }else{
            if(start_new_word) {
                vector<wchar_t> empty_str;
                output.push_back(empty_str);
            }
            start_new_word = false;
            output.back().push_back(c);
        }
        i++;
    }

    vector<wstring> out_str;
    for (size_t i = 0; i < output.size(); i++) {
        wstring s(output[i].begin(), output[i].end());
        out_str.push_back(s);
    }
    return out_str;
}

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

class TrieNode {
public:
    std::unordered_map<wchar_t, std::unique_ptr<TrieNode>> children;
    bool is_end;
    std::wstring delimiter;

    TrieNode() : is_end(false) {}
};

class Splitter {
private:
    std::unique_ptr<TrieNode> root;

    void insert(const std::wstring& str) {
        TrieNode* current = root.get();
        for (wchar_t ch : str) {
            if (!current->children[ch]) {
                current->children[ch] = std::make_unique<TrieNode>();
            }
            current = current->children[ch].get();
        }
        current->is_end = true;
        current->delimiter = str;
    }

public:
    Splitter(const std::vector<std::wstring>& delimiters) {
        root = std::make_unique<TrieNode>();
        for (const auto& delimiter : delimiters) {
            insert(delimiter);
        }
    }

    std::vector<std::wstring> split(const std::wstring& input) {
        std::vector<std::wstring> result;
        size_t start = 0;
        
        while (start < input.length()) {
            // Try to find the next delimiter starting from current position
            size_t best_match_length = 0;
            std::wstring matched_delimiter;
            
            // Check for possible delimiter match starting at current position
            TrieNode* current = root.get();
            size_t pos = start;
            
            while (pos < input.length() && current->children.count(input[pos])) {
                current = current->children[input[pos]].get();
                pos++;
                if (current->is_end) {
                    best_match_length = pos - start;
                    matched_delimiter = current->delimiter;
                }
            }

            if (best_match_length > 0) {
                // Add substring before delimiter if it exists
                if (start < start + best_match_length) {
                    result.push_back(input.substr(start, best_match_length));
                }
                start += best_match_length;
            } else {
                // No delimiter found at current position
                size_t next_pos = start + 1;
                bool found_next = false;
                
                // Find next possible delimiter start
                while (next_pos < input.length()) {
                    if (root->children.count(input[next_pos])) {
                        found_next = true;
                        break;
                    }
                    next_pos++;
                }
                
                // Add the substring up to next possible delimiter or end
                result.push_back(input.substr(start, (found_next ? next_pos - start : std::wstring::npos)));
                start = next_pos;
            }
        }
        
        return result;
    }
};


std::string wstring_to_utf8(const std::wstring& wstr)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
    return converter.to_bytes(wstr);
}

std::wstring utf8_to_wstring(const std::string& str)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
    return converter.from_bytes(str);
}

class WordPieceTokenizer
{
private:
json jsonObj;
json vocab;
size_t max_input_chars_per_word;
wstring unk_token;
vector<wstring> special_tokens;

public:
WordPieceTokenizer(const string& config_path)
{
    std::ifstream file(config_path);
    file >> jsonObj;
    vocab = jsonObj["model"]["vocab"];
    max_input_chars_per_word = jsonObj["model"]["max_input_chars_per_word"];
    unk_token = utf8_to_wstring(jsonObj["model"]["unk_token"]);
    
    // create list of special tokens to not split them
    for(auto item: jsonObj["added_tokens"]) {
        if(item["special"]) {
            special_tokens.push_back(utf8_to_wstring(item["content"]));
        }
    }
}
int get_word_index(const wstring& word)
{
    string w_word = wstring_to_utf8(word);
    if(vocab.find(w_word) != vocab.end()) {
        //cout << "Found word. Id: " << vocab[word] << endl;
        return vocab[w_word];
    }else{
        //cout << "Not found" << endl;
        return -1;
    }
}


vector<size_t> tokenize_full(const wstring& input_text, bool split_specials=false)
{
    wstring padded_text = pad_chinese_chars(input_text);
    vector<wstring> tokens = split(padded_text);

    // split the input using special tokens as delimiters
    // using Trie like the original HuggingFace algorithm
    Splitter splitter(special_tokens);

    vector<wstring> special_word_tokenized;
    for(size_t i = 0; i < tokens.size(); i++) {
        auto split_by_special = splitter.split(tokens[i]);
        special_word_tokenized.insert(special_word_tokenized.end(), split_by_special.begin(), split_by_special.end());
    }

    vector<wstring> basic_tokenized;
    for(size_t i = 0; i < special_word_tokenized.size(); i++) {
        auto splitted_by_punc = run_split_on_punctuation(special_word_tokenized[i], split_specials, special_tokens);
        basic_tokenized.insert(basic_tokenized.end(), splitted_by_punc.begin(), splitted_by_punc.end());
    }


    vector<wstring> wordpiece_tokenized;
    for(size_t i = 0; i < basic_tokenized.size(); i++) {
        auto splitted_by_wordpiece = wordpiece_tokenize(basic_tokenized[i]);
        wordpiece_tokenized.insert(wordpiece_tokenized.end(), splitted_by_wordpiece.begin(), splitted_by_wordpiece.end());
    }
    vector<size_t> tokenized_ids;
    tokenized_ids.push_back(get_word_index(utf8_to_wstring("[CLS]")));
    vector<size_t> seq_ids = convert_tokens_to_ids(wordpiece_tokenized);
    tokenized_ids.insert(tokenized_ids.end(), seq_ids.begin(), seq_ids.end());
    tokenized_ids.push_back(get_word_index(utf8_to_wstring("[SEP]")));
    return tokenized_ids;
}


vector<wstring> wordpiece_tokenize(const wstring& input_text)
{
    vector<wstring> tokens = split(input_text);
    vector<wstring> output_tokens;
    for(size_t i = 0; i < tokens.size(); i++) {
        auto& tok = tokens[i];
        if(tok.length() > max_input_chars_per_word) {
            output_tokens.push_back(unk_token);
            continue;
        }

        bool is_bad = false;
        size_t start = 0;
        vector<wstring> sub_tokens;

        while(start < tok.length()) {
            size_t end = tok.length();
            wstring cur_substr;
            while(start < end) {
                wstring substr = tok.substr(start, end-start);
                if(start > 0) {
                    substr = L"##" + substr;
                }
                size_t idx = get_word_index(substr);
                if(idx != -1) {
                    cur_substr = substr;
                    break;
                }
                end--;
            }

            if(cur_substr.empty()) {
                is_bad = true;
                break;
            }
            sub_tokens.push_back(cur_substr);
            start = end;
        }

        if(is_bad) {
            output_tokens.push_back(unk_token);
        }else{
            output_tokens.insert(output_tokens.end(), sub_tokens.begin(), sub_tokens.end());
        }
    }
    return output_tokens;
}

vector<size_t> convert_tokens_to_ids(const vector<wstring>& input_seq)
{
    vector<size_t> output_ids;
    for(size_t i = 0; i < input_seq.size(); i++) {
        output_ids.push_back(get_word_index(input_seq[i]));
    }
    return output_ids;
}

};


int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <input_file>" << std::endl;
        return 1;
    }

    std::locale::global(std::locale(""));
    WordPieceTokenizer tokenizer("tokenizer.json");

    // Open the input file
    std::wifstream file(argv[1]);
    file.imbue(std::locale(""));
    if (!file) {
        std::cerr << "Error: Unable to open input file." << std::endl;
        return 1;
    }

    // Read the entire file content into a single wide string
    std::wstringstream buffer;
    buffer << file.rdbuf();
    std::wstring input_text = buffer.str();

    std::wcout << input_text << std::endl;

    // Tokenize the input text
    auto r = tokenizer.tokenize_full(input_text);
    std::wcout << "===== TOKENS START=====" << std::endl;
    for (auto &x : r) {
        std::wcout << x << std::endl;
    }
    std::wcout << "===== TOKENS END ======" << std::endl;

    return 0;
}